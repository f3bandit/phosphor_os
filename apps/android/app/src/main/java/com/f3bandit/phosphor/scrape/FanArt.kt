// Phosphor for Android: fan-made box art (the same list as Phosphor for Windows' fanart.go): covers for
// games no database has, such as ROM hacks and homebrew. The list is read from GitHub (the copy built
// into the app when GitHub can't be reached); each cover is fetched from where its maker posted it, or,
// when that link is gone, from Phosphor's copy in pack.zip. Every cover is credited (hack_by,
// cover_from). A game's exact name is matched first, then its name with tags and versions out.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.content.Context
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipFile

private const val FANART_URL = "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/fanart.json"
private const val FANPACK_URL = "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/pack.zip"

class FanGame(val sys: String, val names: List<String>, val art: String, val pack: String, val hackBy: String, val coverFrom: String,
              val developer: String, val year: String, val genre: String, val description: String)

fun fanKey(t: String) = amKey(amStrip(amClean(t)))
fun fanExact(t: String) = buildString { for (c in t.lowercase()) if (c in 'a'..'z' || c in '0'..'9' || c.code > 127) append(c) }

object FanArt {
    private val lock = Any()
    private var map: Map<String, FanGame>? = null
    private var at = 0L

    fun index(text: String): Map<String, FanGame>? = runCatching {
        val a = JSONObject(text).getJSONArray("games")
        val m = HashMap<String, FanGame>()
        val clash = HashSet<String>()
        for (i in 0 until a.length()) {
            val j = a.getJSONObject(i)
            val names = j.optJSONArray("names")?.let { n -> (0 until n.length()).map { n.optString(it) } } ?: continue
            val g = FanGame(j.optString("sys"), names, j.optString("art"), j.optString("pack"), j.optString("hack_by"), j.optString("cover_from"),
                j.optString("developer"), j.optString("year"), j.optString("genre"), j.optString("description"))
            if (g.art.isEmpty()) continue
            for (n in names) {
                fanExact(n).takeIf { it.isNotEmpty() }?.let { m["=${g.sys}|$it"] = g }
                val k = fanKey(n)
                if (k.isEmpty()) continue
                val key = "${g.sys}|$k"
                if (m[key]?.let { it !== g } == true) clash.add(key)
                m[key] = g
            }
        }
        clash.forEach { m.remove(it) }
        m
    }.getOrNull()

    /** the fan art for a game, if the list has it (the list is read again after an hour) */
    suspend fun find(context: Context, sys: String, title: String): FanGame? {
        val now = System.currentTimeMillis()
        val m = synchronized(lock) { map?.takeIf { now - at < 3_600_000 } } ?: run {
            var fresh = runCatching { context.assets.open("web/fanart.json").bufferedReader().readText() }.getOrNull()?.let { index(it) }
            val r = netRequest("GET", FANART_URL, max = 4L shl 20)
            if (r.rc == 0 && r.body != null) index(String(r.body))?.takeIf { it.isNotEmpty() }?.let { fresh = it }
            synchronized(lock) { map = fresh ?: emptyMap(); at = now; map!! }
        }
        return m["=$sys|${fanExact(title)}"] ?: m["$sys|${fanKey(title)}"]
    }

    suspend fun lookup(context: Context, g: SGame, r: SRes): Int {
        val f = find(context, g.sys, g.title) ?: return 0
        var desc = f.description
        if (f.hackBy.isNotEmpty() && !desc.contains(f.hackBy)) desc = "$desc Hack by ${f.hackBy}.".trim()
        r.set(F_DESC, desc); r.set(F_DEV, f.developer); r.set(F_YEAR, f.year); r.set(F_GENRE, f.genre)
        r.addArt("fan art", f.art)
        if (f.pack.isNotEmpty()) r.addArt("fan art pack", "pack:${f.pack}")
        return 1
    }

    // ---------- the backup pack: downloaded once, only when a cover's link is gone ----------

    private val packLock = Any()

    suspend fun packGet(context: Context, name: String): ByteArray? {
        val f = File(context.cacheDir, "boxart-lists/fanart-pack.zip")
        fun read(): ByteArray? = runCatching { ZipFile(f).use { z -> z.getEntry(name)?.let { e -> z.getInputStream(e).use { it.readBytes() } } } }.getOrNull()
        synchronized(packLock) { if (f.exists()) read()?.let { return it } }
        val r = netRequest("GET", FANPACK_URL, max = 64L shl 20)
        if (r.rc != 0 || r.body == null) return null
        synchronized(packLock) {
            f.parentFile?.mkdirs()
            val tmp = File(f.path + ".new"); tmp.writeBytes(r.body); tmp.renameTo(f)
            return read()
        }
    }
}
