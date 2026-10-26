// Phosphor for Android: talks to phosphord's remote access (docs/remote-api.md) over the home network.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Socket
import java.net.URL

const val REMOTE_PORT = 8767

/** What the MiSTer said went wrong, worded for the user. code is the HTTP status (0: not reached). */
class PhosphorError(message: String, val code: Int = 0) : IOException(message)

data class Info(val address: String, val version: String)

data class SystemItem(val id: String, val name: String, val games: Int)

data class GameItem(
    val id: Long, val title: String, val system: String, val art: Boolean, val details: Boolean,
    val linked: Boolean, val favorite: Boolean, val scrape: String,
)

data class GameFile(val name: String, val where: String, val size: Long, val crc32: String?, val serial: String?, val online: Boolean)

data class Game(
    val id: Long, val title: String, val system: String, val systemName: String,
    val year: String?, val developer: String?, val publisher: String?, val genre: String?,
    val players: String?, val region: String?, val description: String?,
    val hasArt: Boolean, val favorite: Boolean, val playCount: Int, val playSeconds: Long,
    val basedOnId: Long?, val basedOnTitle: String?, val scrape: String, val files: List<GameFile>,
)

data class Candidate(val id: Long, val title: String, val score: Int)

data class Status(
    val version: String, val games: Int, val indexing: Boolean, val identifying: Boolean, val generation: Int,
    val scraperOn: Boolean, val scraperRunning: Boolean, val scraperStatus: String,
    val missingArt: Int, val missingDetails: Int, val notFound: Int,
)

data class Snapshot(val name: String, val whenText: String, val reason: String, val kb: Int)

data class BackupState(
    val installed: Boolean, val dest: String, val lastError: String, val snapshots: List<Snapshot>,
    val job: String, val busy: Boolean, val jobOk: Boolean, val output: String, val at: Long,
)

data class Pad(val port: Int, val connected: Boolean, val id: Int, val buttons: Int, val lx: Int, val ly: Int, val rx: Int, val ry: Int)

data class CardBackup(val name: String, val bytes: Long, val at: Long)

data class CardState(
    val mainApi: Boolean, val available: Boolean, val canLoad: Boolean, val inGame: Boolean, val on: Boolean,
    val job: String, val port: Int, val busy: Boolean, val jobOk: Boolean, val done: Int, val total: Int,
    val message: String, val file: String, val backups: List<CardBackup>,
)

data class BtDevice(val mac: String, val name: String, val connected: Boolean, val input: Boolean)

data class BtState(val present: Boolean, val devices: List<BtDevice>, val busy: Boolean, val ok: Boolean, val at: Long, val found: String, val message: String)

data class DiagItem(val area: String, val key: String, val level: String, val message: String)

data class DiagState(val installed: Boolean, val busy: Boolean, val at: Long, val level: String, val summary: String, val error: String, val items: List<DiagItem>)

data class CloudPlace(val name: String, val type: String, val inUse: Boolean)

data class CloudState(
    val installed: Boolean, val ready: Boolean, val remote: String, val device: String, val lastPush: String,
    val pending: Boolean, val lastError: String, val places: List<CloudPlace>,
    val job: String, val busy: Boolean, val jobOk: Boolean, val output: String, val at: Long,
)

data class Category(val id: Int, val name: String, val games: Int)

data class ScrapeSource(val id: String, val name: String, val on: Boolean, val ready: Boolean, val needs: String?)

data class ScrapeAccount(val sec: String, val key: String, val set: Boolean, val value: String)

/** the MiSTer's own box art run (scrape_progress) */
data class ScrapeProgress(val running: Boolean, val status: String, val system: String, val game: String, val source: String,
                          val total: Int, val done: Int, val found: Int, val notFound: Int, val later: Int)

data class PowerEvent(val at: Long, val what: String)
data class PowerState(val installed: Boolean, val count: Int, val warnAt: Int, val warn: Boolean, val events: List<PowerEvent>)

data class Job(val name: String, val busy: Boolean, val ok: Boolean, val output: String)

data class GuardFile(val path: String, val reset: Boolean)
data class RestorePoint(val name: String, val label: String, val created: String, val linux: String, val complete: Boolean)
data class GuardState(val installed: Boolean, val linux: String, val updatedAt: String, val updatedFrom: String, val awaiting: Boolean,
                      val files: List<GuardFile>, val points: List<RestorePoint>, val job: Job?)

data class Drive(val path: String, val kind: String, val fs: String, val sizeKb: Long, val freeKb: Long, val model: String,
                 val gen: String, val watts: String, val mounted: Boolean)
data class StorageCheck(val job: String, val target: String, val busy: Boolean, val ok: Boolean, val at: Long, val output: String)
data class StorageState(val installed: Boolean, val drives: List<Drive>, val overlapTool: List<String>, val last: StorageCheck)

data class UpdatePackage(val file: String, val version: String)
data class Rollback(val version: String, val date: String)
data class UpdateState(val available: Boolean, val version: String, val packages: List<UpdatePackage>, val rollbacks: List<Rollback>,
                       val state: String, val msg: String, val at: Long)

/** compares dotted versions: "0.27.16" > "0.27.9" */
fun versionNewer(a: String, b: String): Boolean {
    val x = a.split('.', '-').map { it.toIntOrNull() ?: 0 }; val y = b.split('.', '-').map { it.toIntOrNull() ?: 0 }
    for (i in 0 until maxOf(x.size, y.size)) { val p = x.getOrElse(i) { 0 }; val q = y.getOrElse(i) { 0 }; if (p != q) return p > q }
    return false
}

private fun JSONObject.str(k: String): String? = if (isNull(k)) null else optString(k).takeIf { it.isNotEmpty() }

/** One paired MiSTer. Every call does network I/O: call from a coroutine (they switch to Dispatchers.IO). */
class PhosphorApi(val address: String, private val token: String?) {

    private fun request(method: String, path: String, body: ByteArray? = null, type: String? = null, timeoutMs: Int = 20_000): ByteArray {
        val c = URL("http://$address$path").openConnection() as HttpURLConnection
        try {
            c.requestMethod = method
            c.connectTimeout = 5_000
            c.readTimeout = timeoutMs
            c.useCaches = false
            token?.let { c.setRequestProperty("Authorization", "Bearer $it") }
            if (body != null) {
                c.doOutput = true
                c.setRequestProperty("Content-Type", type ?: "application/json")
                c.setFixedLengthStreamingMode(body.size)
                c.outputStream.use { it.write(body) }
            }
            val code = try { c.responseCode } catch (e: IOException) {
                throw PhosphorError("Can't reach the MiSTer at $address. Is it on, on this WiFi, with Remote access turned on?")
            }
            val data = (if (code in 200..299) c.inputStream else c.errorStream)?.use { it.readBytes() } ?: ByteArray(0)
            if (code != 200) {
                val msg = runCatching { JSONObject(String(data)).optString("error") }.getOrNull()
                throw PhosphorError(msg?.takeIf { it.isNotEmpty() } ?: "The MiSTer answered $code", code)
            }
            return data
        } catch (e: PhosphorError) {
            throw e
        } catch (e: IOException) {
            throw PhosphorError("Can't reach the MiSTer at $address. Is it on, on this WiFi, with Remote access turned on?")
        } finally {
            c.disconnect()
        }
    }

    /** a phosphord command; throws PhosphorError with its error text when ok is false */
    suspend fun cmd(json: JSONObject, timeoutMs: Int = 20_000): JSONObject = withContext(Dispatchers.IO) {
        val r = JSONObject(String(request("POST", "/api", json.toString().toByteArray(), "application/json", timeoutMs)))
        if (!r.optBoolean("ok")) throw PhosphorError(r.optString("error").ifEmpty { "The MiSTer said no" })
        r
    }

    private fun obj(vararg kv: Pair<String, Any?>) = JSONObject().apply { kv.forEach { (k, v) -> if (v != null) put(k, v) } }

    suspend fun pair(code: String, name: String): Pair<String, String> = withContext(Dispatchers.IO) {
        val r = JSONObject(String(request("POST", "/pair", obj("code" to code, "name" to name).toString().toByteArray())))
        r.getString("token") to r.optString("id")
    }

    suspend fun status(): Status {
        val s = cmd(obj("cmd" to "status"))
        val sc = runCatching { cmd(obj("cmd" to "scrape")) }.getOrNull()
        suspend fun count(f: String) = runCatching { cmd(obj("cmd" to "games", "filter" to f, "limit" to 0)).optInt("total") }.getOrDefault(0)
        return Status(
            version = s.optString("version"), games = s.optInt("games"), indexing = s.optBoolean("indexing"),
            identifying = s.optBoolean("identifying"), generation = s.optInt("generation"),
            scraperOn = sc?.optBoolean("on") ?: false, scraperRunning = sc?.optBoolean("running") ?: false,
            scraperStatus = sc?.optString("status") ?: "",
            missingArt = count("missing_art"), missingDetails = count("missing_details"), notFound = count("not_found"),
        )
    }

    suspend fun systems(): List<SystemItem> {
        val a = cmd(obj("cmd" to "systems")).getJSONArray("systems")
        return (0 until a.length()).map { a.getJSONObject(it) }.filter { it.optInt("games") > 0 }
            .map { SystemItem(it.getString("id"), it.getString("name"), it.optInt("games")) }
    }

    suspend fun games(system: String? = null, filter: String? = null, q: String? = null, offset: Int = 0, limit: Int = 500): Pair<Int, List<GameItem>> {
        val r = cmd(obj("cmd" to "games", "system" to system, "filter" to filter, "q" to q?.takeIf { it.isNotBlank() }, "offset" to offset, "limit" to limit))
        val a = r.getJSONArray("games")
        return r.optInt("total") to (0 until a.length()).map { a.getJSONObject(it) }.map {
            GameItem(it.getLong("id"), it.optString("title"), it.optString("system"), it.optBoolean("art"), it.optBoolean("details"),
                it.optBoolean("linked"), it.optBoolean("favorite"), it.optString("scrape"))
        }
    }

    suspend fun game(id: Long): Game = parseGame(cmd(obj("cmd" to "game", "id" to id)).getJSONObject("game"))

    suspend fun edit(id: Long, fields: Map<String, String>): Game =
        parseGame(cmd(obj("cmd" to "edit", "id" to id, "set" to JSONObject(fields as Map<*, *>))).getJSONObject("game"))

    suspend fun favorite(id: Long, on: Boolean) { cmd(obj("cmd" to "favorite", "id" to id, "on" to on)) }
    suspend fun launch(id: Long) { cmd(obj("cmd" to "launch", "id" to id), 30_000) }
    suspend fun rescrape(id: Long) { cmd(obj("cmd" to "rescrape", "id" to id)) }
    /** port: a game ported from another system, based on the original there */
    suspend fun link(id: Long, base: Long, port: Boolean = false) { cmd(obj("cmd" to "link", "id" to id, "base" to base).also { if (port) it.put("port", true) }) }
    suspend fun unlink(id: Long) { cmd(obj("cmd" to "unlink", "id" to id)) }

    suspend fun candidates(id: Long, q: String?): List<Candidate> {
        val a = cmd(obj("cmd" to "candidates", "id" to id, "q" to q?.takeIf { it.isNotBlank() }, "limit" to 30)).getJSONArray("games")
        return (0 until a.length()).map { a.getJSONObject(it) }.map { Candidate(it.getLong("id"), it.optString("title"), it.optInt("score")) }
    }

    /** cloud saves (cloud-sync module on the MiSTer); with what, starts a job: setup, add, use, test, push,
     *  preview, pull, devices. Passwords and sign-in tokens go only to the MiSTer, on the home network. */
    suspend fun cloud(what: String? = null, extra: Map<String, Any> = emptyMap()): CloudState {
        val q = obj("cmd" to "cloud", "do" to what)
        extra.forEach { (k, v) -> q.put(k, if (v is Map<*, *>) JSONObject(v) else v) }
        val r = cmd(q)
        val st = r.optJSONObject("status") ?: JSONObject()
        val j = r.optJSONObject("job") ?: JSONObject()
        val pa = r.optJSONArray("remotes") ?: JSONArray()
        return CloudState(
            installed = r.optBoolean("installed"), ready = st.optString("rclone").isNotEmpty(), remote = st.optString("remote"),
            device = st.optString("device"), lastPush = st.optString("last_push"), pending = st.optString("pending") == "1",
            lastError = st.optString("last_error"),
            places = (0 until pa.length()).map { pa.getJSONObject(it) }.map { CloudPlace(it.optString("name"), it.optString("type"), it.optBoolean("in_use")) },
            job = j.optString("name"), busy = j.optBoolean("busy"), jobOk = j.optBoolean("ok"), output = j.optString("output"), at = j.optLong("at"),
        )
    }

    /** snapshots on the MiSTer's own drives (backup-agent module); what: now, preview, restore;
     *  only: "saves|savestates" to put back just the saves, null for everything in the snapshot */
    suspend fun backup(what: String? = null, name: String? = null, only: String? = null): BackupState {
        val r = cmd(obj("cmd" to "backup", "do" to what, "name" to name, "only" to only))
        val st = r.optJSONObject("status") ?: JSONObject()
        val j = r.optJSONObject("job") ?: JSONObject()
        val sa = r.optJSONArray("snapshots") ?: JSONArray()
        return BackupState(
            installed = r.optBoolean("installed"), dest = st.optString("dest"), lastError = st.optString("last_error"),
            snapshots = (0 until sa.length()).map { sa.getJSONObject(it) }.map { Snapshot(it.optString("name"), it.optString("when"), it.optString("reason"), it.optInt("kb")) },
            job = j.optString("name"), busy = j.optBoolean("busy"), jobOk = j.optBoolean("ok"), output = j.optString("output"), at = j.optLong("at"),
        )
    }

    /** memory cards on the front ports (phosphor-main + the Phosphor menu core); what: backup, restore
     *  (backs the card up first), check, prepare (loads the Phosphor menu core, menu only) */
    suspend fun memcard(what: String? = null, port: Int? = null, name: String? = null): CardState {
        val r = cmd(obj("cmd" to "memcard", "do" to what, "port" to port, "name" to name))
        val p = r.optJSONObject("ports") ?: JSONObject()
        val j = r.optJSONObject("job") ?: JSONObject()
        val ba = r.optJSONArray("backups") ?: JSONArray()
        return CardState(
            mainApi = p.optBoolean("main_api"), available = p.optBoolean("available"), canLoad = p.optBoolean("can_load"),
            inGame = p.optBoolean("in_game"), on = p.optBoolean("on"),
            job = j.optString("name"), port = j.optInt("port"), busy = j.optBoolean("busy"), jobOk = j.optBoolean("ok"),
            done = j.optInt("done"), total = j.optInt("total", 1), message = j.optString("message"), file = j.optString("file"),
            backups = (0 until ba.length()).map { ba.getJSONObject(it) }.map { CardBackup(it.optString("name"), it.optLong("bytes"), it.optLong("at")) },
        )
    }

    /** both front-port controllers, live (turns the ports on; never loads the menu core) */
    suspend fun pads(): List<Pad> {
        val r = cmd(obj("cmd" to "pads"))
        val a = r.optJSONArray("pads") ?: JSONArray()
        return (0 until a.length()).map { a.getJSONObject(it) }.map {
            Pad(it.optInt("port"), it.optBoolean("connected"), it.optInt("id"), it.optInt("buttons"),
                it.optInt("lx"), it.optInt("ly"), it.optInt("rx"), it.optInt("ry"))
        }
    }

    suspend fun padAnalog(port: Int) { cmd(obj("cmd" to "pad_analog", "port" to port)) }

    /** Bluetooth controllers paired with the MiSTer; what: pair (looks for one in pairing mode, up to 90 s),
     *  cancel, forget, disconnect (with mac) */
    suspend fun bluetooth(what: String? = null, mac: String? = null): BtState {
        val r = cmd(obj("cmd" to "bluetooth", "do" to what, "mac" to mac))
        val p = r.optJSONObject("pairing") ?: JSONObject()
        val a = r.optJSONArray("devices") ?: JSONArray()
        return BtState(
            present = r.optBoolean("present"),
            devices = (0 until a.length()).map { a.getJSONObject(it) }.map { BtDevice(it.optString("mac"), it.optString("name"), it.optBoolean("connected"), it.optBoolean("input")) },
            busy = p.optBoolean("busy"), ok = p.optBoolean("ok"), at = p.optLong("at"), found = p.optString("found"), message = p.optString("message"),
        )
    }

    suspend fun diag(run: Boolean = false): DiagState {
        val r = cmd(obj("cmd" to "diag", "run" to if (run) true else null))
        val sm = r.optJSONObject("summary") ?: JSONObject()
        val a = r.optJSONArray("results") ?: JSONArray()
        return DiagState(
            r.optBoolean("installed"), r.optString("busy").isNotEmpty(), if (r.isNull("at")) 0 else r.optLong("at"),
            sm.optString("level"), sm.optString("text"), r.optString("error"),
            (0 until a.length()).map { a.getJSONObject(it) }.map { DiagItem(it.optString("area"), it.optString("key"), it.optString("level"), it.optString("message")) },
        )
    }

    suspend fun categories(): List<Category> {
        val a = cmd(obj("cmd" to "categories")).optJSONArray("categories") ?: JSONArray()
        return (0 until a.length()).map { a.getJSONObject(it) }.map { Category(it.optInt("id"), it.optString("name"), it.optInt("games")) }
    }

    /** a category's systems that have games (all categories: null) */
    suspend fun systemsIn(category: Int?): List<SystemItem> {
        val a = cmd(obj("cmd" to "systems", "category" to category)).getJSONArray("systems")
        return (0 until a.length()).map { a.getJSONObject(it) }.filter { it.optInt("games") > 0 }
            .map { SystemItem(it.getString("id"), it.getString("name"), it.optInt("games")) }
    }

    /** the game played last, or null */
    suspend fun last(): Game? = cmd(obj("cmd" to "last")).optJSONObject("game")?.let { parseGame(it) }

    /** look for new games and work out what they are (runs only when asked) */
    suspend fun rescan() { cmd(obj("cmd" to "rescan")) }

    /** the MiSTer's own box art lookup: on starts it */
    suspend fun scrapeOn() { cmd(obj("cmd" to "scrape", "on" to true)) }

    suspend fun scrapeProgress(): ScrapeProgress {
        val r = cmd(obj("cmd" to "scrape_progress"))
        return ScrapeProgress(r.optBoolean("running"), r.optString("status"), r.optString("system"), r.optString("game"), r.optString("source"),
            r.optInt("total"), r.optInt("done"), r.optInt("found"), r.optInt("not_found"), r.optInt("later"))
    }

    /** the sources, in the order they're asked; with order/off, saves them first */
    suspend fun scrapeSources(order: List<String>? = null, off: List<String>? = null): List<ScrapeSource> {
        val r = cmd(obj("cmd" to "scrape_sources", "order" to order?.let { JSONArray(it) }, "off" to off?.let { JSONArray(it) }))
        val a = r.optJSONArray("sources") ?: JSONArray()
        return (0 until a.length()).map { a.getJSONObject(it) }.map {
            ScrapeSource(it.optString("id"), it.optString("name"), it.optBoolean("on"), it.optBoolean("ready"), it.str("needs"))
        }
    }

    /** the logins saved on the MiSTer (secrets only as set or not); with sec/key, saves one value first */
    suspend fun scrapeAccounts(sec: String? = null, key: String? = null, value: String? = null): List<ScrapeAccount> {
        val set = if (sec != null && key != null) JSONObject().put("sec", sec).put("key", key).put("value", value ?: "") else null
        val a = cmd(obj("cmd" to "scrape_accounts", "set" to set)).optJSONArray("accounts") ?: JSONArray()
        return (0 until a.length()).map { a.getJSONObject(it) }.map { ScrapeAccount(it.optString("sec"), it.optString("key"), it.optBoolean("set"), it.optString("value")) }
    }

    /** the power check (health-watch); ack dismisses the warning */
    suspend fun power(ack: Boolean = false): PowerState {
        val r = cmd(obj("cmd" to "power", "do" to if (ack) "ack" else null))
        val a = r.optJSONArray("events") ?: JSONArray()
        return PowerState(r.optBoolean("installed"), r.optInt("count"), r.optInt("warn_at", 3), r.optBoolean("warn"),
            (0 until a.length()).map { a.getJSONObject(it) }.map { PowerEvent(it.optDouble("at").toLong(), it.optString("what")) })
    }

    /** restart, shut down (safely) or reload the TV screen */
    suspend fun system(what: String) { cmd(obj("cmd" to "system", "do" to what), 30_000) }

    private fun job(j: JSONObject?) = j?.takeIf { it.optString("name").isNotEmpty() }?.let { Job(it.optString("name"), it.optBoolean("busy"), it.optBoolean("ok"), it.optString("output")) }

    /** updates & restore points (update-guard); what: restore, keep, rollback (items: paths or a point's name) */
    suspend fun guard(what: String? = null, items: List<String>? = null): GuardState {
        val r = cmd(obj("cmd" to "guard", "do" to what, "items" to items?.let { JSONArray(it) }))
        val fa = r.optJSONArray("files") ?: JSONArray(); val pa = r.optJSONArray("points") ?: JSONArray()
        return GuardState(
            r.optBoolean("installed"), r.optString("linux"), r.optString("updated_at"), r.optString("updated_from"), r.optBoolean("awaiting"),
            (0 until fa.length()).map { fa.getJSONObject(it) }.map { GuardFile(it.optString("path"), it.optBoolean("reset")) },
            (0 until pa.length()).map { pa.getJSONObject(it) }.map { RestorePoint(it.optString("name"), it.optString("label"), it.optString("created"), it.optString("linux"), it.optBoolean("complete", true)) },
            job(r.optJSONObject("job")),
        )
    }

    /** the drives (sd_integrity); what: check (test, target) or mount (target) */
    suspend fun storage(what: String? = null, test: String? = null, target: String? = null): StorageState {
        val r = cmd(obj("cmd" to "storage", "do" to what, "test" to test, "target" to target), 30_000)
        val da = r.optJSONArray("drives") ?: JSONArray(); val oa = r.optJSONArray("overlap_tool") ?: JSONArray()
        val l = r.optJSONObject("last") ?: JSONObject()
        return StorageState(
            r.optBoolean("installed"),
            (0 until da.length()).map { da.getJSONObject(it) }.map {
                Drive(it.optString("path"), it.optString("kind"), it.optString("fs"), it.optDouble("size_kb").toLong(), it.optDouble("free_kb").toLong(),
                    it.optString("model"), it.optString("gen"), it.optString("watts"), it.optBoolean("mounted", true))
            },
            (0 until oa.length()).map { oa.getString(it) },
            StorageCheck(l.optString("job"), l.optString("target"), l.optBoolean("busy"), l.optBoolean("ok"), l.optDouble("at").toLong(), l.optString("output")),
        )
    }

    /** Phosphor's own updates: packages on the MiSTer, versions to roll back to; what: install (file) or rollback (version) */
    suspend fun update(what: String? = null, arg: String? = null): UpdateState {
        val r = cmd(obj("cmd" to "update", "do" to what, "file" to if (what == "install") arg else null, "version" to if (what == "rollback") arg else null), 30_000)
        val pa = r.optJSONArray("packages") ?: JSONArray(); val ra = r.optJSONArray("rollbacks") ?: JSONArray()
        val st = r.optJSONObject("state") ?: JSONObject()
        return UpdateState(
            r.optBoolean("available"), r.optString("version"),
            (0 until pa.length()).map { pa.getJSONObject(it) }.map { UpdatePackage(it.optString("file"), it.optString("version")) },
            (0 until ra.length()).map { ra.getJSONObject(it) }.map { Rollback(it.optString("version"), it.optString("date")) },
            st.optString("state"), st.optString("msg"), st.optLong("at"),
        )
    }

    // ---- box art looked up on the phone (the same protocol as Phosphor for Windows) ----

    /** the games still to look up, with the MiSTer's source order and logins (start: a new run) */
    suspend fun scrapeTodo(start: Boolean, limit: Int = 400): JSONObject = cmd(obj("cmd" to "scrape_todo", "start" to start, "limit" to limit), 60_000)

    suspend fun scrapeResult(id: Long, found: Boolean, later: Boolean, official: String, fields: Map<String, String>, system: String) {
        cmd(obj("cmd" to "scrape_result", "id" to id, "found" to found, "later" to later, "official" to official,
            "fields" to JSONObject(fields as Map<*, *>), "system" to system))
    }

    suspend fun scrapePcDone(status: String) { cmd(obj("cmd" to "scrape_pc_done", "status" to status)) }

    /** the games nothing was found for (automatic matching); null when the MiSTer's Phosphor is too old for it */
    suspend fun scrapeTodoMiss(offset: Int, limit: Int = 1000): JSONObject? =
        cmd(obj("cmd" to "scrape_todo", "miss" to true, "offset" to offset, "limit" to limit), 60_000).takeIf { it.optBoolean("miss") }

    /** undo what an automatic match filled in: only those fields, and the box art if it came from it */
    suspend fun scrapeUndo(id: Long, fields: List<String>, art: Boolean) {
        cmd(obj("cmd" to "scrape_undo", "id" to id, "fields" to JSONArray(fields), "art" to art))
    }

    /** a boot file, test ROM or tool: out of "not found" (on), or back (off) */
    suspend fun notGame(id: Long, on: Boolean) { cmd(obj("cmd" to "not_game", "id" to id, "on" to on)) }

    /** answers at all (any reply, even a refusal, means it's there) */
    suspend fun ping(): Boolean = withContext(Dispatchers.IO) {
        try { request("GET", "/info", timeoutMs = 4000); true } catch (e: PhosphorError) { e.code != 0 }
    }

    /** the game's box art (JPEG), or null when it has none */
    suspend fun art(id: Long): ByteArray? = withContext(Dispatchers.IO) {
        try { request("GET", "/art/$id") } catch (e: PhosphorError) { if (e.code == 404) null else throw e }
    }

    /** the box art of the disc in the dock's drive (JPEG or PNG), or null */
    suspend fun discArt(): ByteArray? = withContext(Dispatchers.IO) {
        try { request("GET", "/art/disc") } catch (e: PhosphorError) { if (e.code == 404) null else throw e }
    }

    /** a screenshot on the SS1 (kind "thumb" or "file"; id "<system>/<file>"), or null */
    suspend fun shot(kind: String, id: String): ByteArray? = withContext(Dispatchers.IO) {
        val p = id.split("/").joinToString("/") { java.net.URLEncoder.encode(it, "UTF-8").replace("+", "%20") }
        try { request("GET", "/shots/$kind/$p", timeoutMs = 30_000) } catch (e: PhosphorError) { if (e.code == 404) null else throw e }
    }

    /** a music album's cover on the SS1 ("cd": the music CD in the drive), or null */
    suspend fun musicCover(id: String): ByteArray? = withContext(Dispatchers.IO) {
        try { request("GET", "/music/cover/$id") } catch (e: PhosphorError) { if (e.code == 404) null else throw e }
    }

    suspend fun setArt(id: Long, image: ByteArray, type: String = "image/jpeg"): Game = parseGame(setArtJson(id, image, type).getJSONObject("game"))

    /** the reply as the MiSTer sent it ({ok, game}) */
    suspend fun setArtJson(id: Long, image: ByteArray, type: String = "image/jpeg"): JSONObject = withContext(Dispatchers.IO) {
        val r = JSONObject(String(request("PUT", "/art/$id", image, type, 60_000)))
        if (!r.optBoolean("ok")) throw PhosphorError(r.optString("error").ifEmpty { "Couldn't store the picture" })
        r
    }

    companion object {
        fun parseGame(g: JSONObject): Game {
            val fa = g.optJSONArray("files") ?: JSONArray()
            val files = (0 until fa.length()).map { fa.getJSONObject(it) }.map {
                GameFile(it.optString("path").substringAfterLast('/'), it.optString("where"), it.optLong("size"),
                    it.str("crc32"), it.str("serial"), it.optBoolean("online", true))
            }
            val b = g.optJSONObject("based_on")
            return Game(
                id = g.getLong("id"), title = g.optString("title"), system = g.optString("system_id"),
                systemName = g.optString("system_name"), year = g.str("year"), developer = g.str("developer"),
                publisher = g.str("publisher"), genre = g.str("genre"), players = g.str("players"), region = g.str("region"),
                description = g.str("description"), hasArt = !g.isNull("art") && g.has("art"), favorite = g.optBoolean("favorite"),
                playCount = g.optInt("play_count"), playSeconds = g.optLong("play_seconds"),
                basedOnId = b?.optLong("id"), basedOnTitle = b?.str("title"), scrape = g.optString("scrape"), files = files,
            )
        }

        fun normalize(address: String): String {
            var a = address.trim().removePrefix("http://").removePrefix("https://").trimEnd('/')
            if (a.isNotEmpty() && !a.contains(':')) a = "$a:$REMOTE_PORT"
            return a
        }

        /** GET /info: is there a Phosphor at this address? */
        suspend fun info(address: String, timeoutMs: Int = 1500): Info? = withContext(Dispatchers.IO) {
            try {
                val c = URL("http://${normalize(address)}/info").openConnection() as HttpURLConnection
                c.connectTimeout = timeoutMs
                c.readTimeout = timeoutMs
                val r = c.inputStream.use { JSONObject(String(it.readBytes())) }
                c.disconnect()
                if (r.optString("name") == "Phosphor") Info(normalize(address), r.optString("version")) else null
            } catch (e: Exception) {
                null
            }
        }

        /** every address in the phone's /24 with port 8767 open and a Phosphor answering */
        suspend fun discover(myIp: String): List<Info> = coroutineScope {
            val base = myIp.substringBeforeLast('.')
            val gate = Semaphore(48)
            (1..254).map { n ->
                async(Dispatchers.IO) {
                    gate.withPermit {
                        val ip = "$base.$n"
                        val open = try {
                            Socket().use { it.connect(InetSocketAddress(ip, REMOTE_PORT), 400); true }
                        } catch (e: Exception) { false }
                        if (open) info(ip) else null
                    }
                }
            }.awaitAll().filterNotNull()
        }
    }
}
