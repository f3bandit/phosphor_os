// Phosphor for Android: box art looked up on the phone (Box art > Look up on this phone), the same way
// Phosphor for Windows does it on the PC (scrape_run.go): the phone fetches the lists and pictures,
// matches with the same rules as phosphord's scraper.c, makes the box art and sends the finished picture
// (PUT /art) and the details (scrape_result) to the MiSTer. The MiSTer's own lookup waits meanwhile,
// and the TV says who's doing it. A few games at once; services that ask for a pace get it.
// Each picture is kept in the app's folder (ArtStore) until the MiSTer has it.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.f3bandit.phosphor.PhosphorApi
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

const val F_DESC = 0; const val F_DEV = 1; const val F_PUB = 2; const val F_GENRE = 3; const val F_YEAR = 4
const val F_PLAYERS = 5; const val F_REGION = 6; const val F_COUNT = 7
val FIELD_COLS = arrayOf("description", "developer", "publisher", "genre", "year", "players", "region")

/** one game to look up (scrape_todo) */
class SGame(
    val id: Long, val sys: String, val system: String, val title: String, val crc: String, val serial: String,
    val size: Long, val disc: Boolean, val arcade: Boolean, val hasArt: Boolean, val has: List<String>,
) {
    var official = ""
    var lrSys = ""
}

class SRes {
    val f = arrayOfNulls<String>(F_COUNT)
    val art = ArrayList<Pair<String, String>>()
    val foundBy = ArrayList<String>()
    fun set(i: Int, v0: String) {
        if (f[i] != null) return
        val v = trimField(v0)
        if (v.isEmpty()) return
        f[i] = v
    }
    fun addArt(src: String, u: String) { if (u.isNotEmpty() && art.size < 8) art.add(src to u) }
    fun complete(hasArt: Boolean): Boolean {
        for (i in 0 until F_COUNT) if (f[i] == null && i != F_REGION) return false
        return hasArt || art.isNotEmpty()
    }
}

/** what the screens show of a run on the phone */
data class PhoneScrapeState(
    val running: Boolean = false, val status: String = "", val total: Int = 0, val done: Int = 0, val found: Int = 0,
    val notFound: Int = 0, val later: Int = 0, val game: String = "", val system: String = "", val notes: List<String> = emptyList(),
    val started: Long = 0, val ended: Long = 0,
)

object PhoneScrape {
    var state by mutableStateOf(PhoneScrapeState())
        internal set
    private val mu = Any()
    internal fun update(f: (PhoneScrapeState) -> PhoneScrapeState) = synchronized(mu) { state = f(state) }
}

private class Source(val id: String, val name: String, val ready: (ScrapeRun) -> Boolean, val lookup: suspend (ScrapeRun, SGame, SRes) -> Int)

class ScrapeRun(private val context: Context, private val api: PhosphorApi) {
    private var accounts: Map<String, Map<String, String>> = emptyMap()
    private val order = ArrayList<Source>()
    private val cache = File(context.cacheDir, "boxart-lists")
    private val libretro = Libretro(File(cache, "libretro"))
    private val launchbox = LaunchBox(cache)

    private val sources = listOf(
        Source("libretro", "libretro", { true }) { s, g, r -> libretro.lookup(s, g, r) },
        Source("arcadedb", "ArcadeDB", { true }) { s, g, r -> adbLookup(s, g, r) },
        Source("launchbox", "LaunchBox", { true }) { s, g, r -> launchbox.lookup(s, g, r) },
        Source("thegamesdb", "TheGamesDB", { it.acct("thegamesdb", "apikey").isNotEmpty() }) { s, g, r -> tgdbLookup(s, g, r) },
        Source("screenscraper", "ScreenScraper", {
            (it.acct("screenscraper", "devid").isNotEmpty() && it.acct("screenscraper", "devpassword").isNotEmpty()) ||
                (it.acct("screenscraper", "user").isNotEmpty() && it.acct("screenscraper", "password").isNotEmpty())
        }) { s, g, r -> ssLookup(s, g, r) },
        Source("igdb", "IGDB", { it.acct("igdb", "client_id").isNotEmpty() && it.acct("igdb", "client_secret").isNotEmpty() }) { s, g, r -> igdbLookup(s, g, r) },
    )

    fun acct(sec: String, key: String) = accounts[sec]?.get(key) ?: ""
    fun status(t: String) = PhoneScrape.update { it.copy(status = t) }
    fun note(t: String) = PhoneScrape.update { if (t in it.notes) it else it.copy(notes = it.notes + t) }

    /** the first picture that downloads and decodes, made into box art, kept and sent; 1 sent, 0 none, -1 trouble */
    private suspend fun fetchArt(g: SGame, r: SRes, from: Int): Int {
        var trouble = false
        for (i in from until r.art.size) {
            val res = netRequest("GET", r.art[i].second, max = 32L shl 20)
            if (res.rc < 0) { trouble = true; continue }
            if (res.rc != 0 || res.body == null) continue
            val jpg = makeBoxArt(res.body) ?: continue
            ArtStore.save(g.id, g.title, g.system, jpg)
            try {
                api.setArt(g.id, jpg)
            } catch (e: CancellationException) { throw e } catch (e: Exception) {
                note("Couldn't send box art to the MiSTer: ${e.message} (kept on the phone: Storage > This phone)")
                trouble = true
                continue
            }
            ArtStore.sent(g.id)
            return 1
        }
        return if (trouble) -1 else 0
    }

    /** as scrape_one in phosphord: 1 found (with art, or it had some), 0 nobody has it, -1 trouble */
    private suspend fun one(g: SGame): Pair<Int, SRes> {
        var r = SRes()
        for (h in g.has) FIELD_COLS.indexOf(h).let { if (it >= 0) r.f[it] = "" }
        var art = g.hasArt
        var found = false; var trouble = false; var tried = 0
        for (src in order) {
            if (!currentCoroutineContext().isActive) return -1 to r
            if (!src.ready(this)) continue
            if (r.complete(art)) {
                if (art) break
                val a = fetchArt(g, r, tried)
                tried = r.art.size
                if (a > 0) { art = true; break }
                if (a < 0) trouble = true
            }
            when (src.lookup(this, g, r)) {
                1 -> { found = true; r.foundBy.add(src.name) }
                -1 -> trouble = true
            }
        }
        if (found && !art) when (fetchArt(g, r, tried)) { 1 -> art = true; -1 -> trouble = true }
        if (!found) r = SRes()  // nothing filled in: the MiSTer still tries Disc 1's art
        return (if (art) 1 else if (trouble) -1 else 0) to r
    }

    private suspend fun report(g: SGame, rc: Int, r: SRes) {
        val fields = HashMap<String, String>()
        for (i in 0 until F_COUNT) r.f[i]?.takeIf { it.isNotEmpty() }?.let { fields[FIELD_COLS[i]] = it }
        api.scrapeResult(g.id, r.foundBy.isNotEmpty(), rc < 0, g.official, fields, g.system)
    }

    private fun parseGames(a: JSONArray): List<SGame> = (0 until a.length()).mapNotNull { a.optJSONObject(it) }.map { j ->
        val has = j.optJSONArray("has") ?: JSONArray()
        SGame(j.optLong("id"), js(j, "sys"), js(j, "system"), js(j, "title"), js(j, "crc"), js(j, "serial"), j.optLong("size"),
            j.optBoolean("disc"), j.optBoolean("arcade"), j.optBoolean("has_art"), (0 until has.length()).map { has.optString(it) })
    }

    /** the whole run; returns when every game has been looked up, the run was stopped, or the MiSTer stopped answering */
    suspend fun run() {
        PhoneScrape.update { PhoneScrapeState(running = true, status = "Starting", started = System.currentTimeMillis()) }
        var stoppedBy = ""
        try {
            var first = true
            val fails = AtomicInteger(0)
            val seen = HashSet<Long>()
            while (currentCoroutineContext().isActive) {
                val todo: JSONObject = try { api.scrapeTodo(first) } catch (e: CancellationException) { throw e } catch (e: Exception) {
                    stoppedBy = "The MiSTer didn't answer: ${e.message}"; status(stoppedBy); return
                }
                if (first) {
                    val cfg = todo.optJSONObject("config") ?: JSONObject()
                    val acc = cfg.optJSONObject("accounts") ?: JSONObject()
                    accounts = acc.keys().asSequence().associateWith { sec ->
                        val o = acc.optJSONObject(sec) ?: JSONObject()
                        o.keys().asSequence().associateWith { o.optString(it) }
                    }
                    val ord = cfg.optJSONArray("order") ?: JSONArray()
                    for (i in 0 until ord.length()) sources.firstOrNull { it.id == ord.optString(i) }?.let { order.add(it) }
                    PhoneScrape.update { it.copy(total = todo.optInt("total")) }
                    first = false
                }
                val games = parseGames(todo.optJSONArray("games") ?: JSONArray()).filter { seen.add(it.id) }
                if (games.isEmpty()) return
                // Disc 2 and later after Disc 1: they borrow its art
                val sorted = games.sortedBy { if (discNo(it.title) <= 1) 0 else 1 }
                status("Looking up box art")
                val stopped = AtomicBoolean(false)
                val next = AtomicInteger(0)
                val lock = Mutex()
                coroutineScope {
                    repeat(4) {
                        launch {
                            while (isActive && !stopped.get()) {
                                val i = next.getAndIncrement()
                                if (i >= sorted.size) break
                                val g = sorted[i]
                                PhoneScrape.update { it.copy(game = g.title, system = g.system) }
                                val (rc, r) = one(g)
                                if (!isActive) break
                                try { lock.withLock { report(g, rc, r) } } catch (e: CancellationException) { throw e } catch (e: Exception) {
                                    stoppedBy = "The MiSTer didn't answer: ${e.message}"; status(stoppedBy); stopped.set(true); break
                                }
                                PhoneScrape.update {
                                    when {
                                        rc < 0 -> it.copy(done = it.done + 1, later = it.later + 1)
                                        rc > 0 -> it.copy(done = it.done + 1, found = it.found + 1)
                                        else -> it.copy(done = it.done + 1, notFound = it.notFound + 1)
                                    }
                                }
                                if (rc < 0) {
                                    if (fails.incrementAndGet() >= 12) {
                                        stoppedBy = "No connection: stopped (Look up box art to try again)"; status(stoppedBy); stopped.set(true)
                                    }
                                } else fails.set(0)
                            }
                        }
                    }
                }
                if (stopped.get()) return
            }
        } finally {
            val cancelled = !currentCoroutineContext().isActive
            val st = PhoneScrape.state
            val msg = when { cancelled -> "Stopped"; stoppedBy.isNotEmpty() -> stoppedBy; else -> "Done" }
            PhoneScrape.update { it.copy(running = false, status = msg, ended = System.currentTimeMillis()) }
            // tell the MiSTer, even when stopped (its own lookup waits for this)
            kotlinx.coroutines.withContext(kotlinx.coroutines.NonCancellable) {
                runCatching { api.scrapePcDone("$msg (looked up on the phone: ${st.found} found, ${st.notFound} not found)") }
            }
        }
    }
}
