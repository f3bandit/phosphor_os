// Phosphor for Android, box art on the phone: libretro (no account), as phosphord's src_libretro.c and
// Phosphor for Windows' scrape_libretro.go. Each system's official list (No-Intro, Redump) and
// libretro's details lists are fetched once and kept in the app's cache (fresh again after 30 days).
// Matched by CRC (cartridges) or product code + size (discs), else the exact list name or the same
// title words. Gives the game its official name; box art: libretro's thumbnails, by that name.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File

private const val LR_DB = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat"
private const val LR_THUMBS = "https://thumbnails.libretro.com"
private const val FRESH_MS = 30L * 24 * 3600 * 1000

private class LrMap(val lr: List<String>, val disc: Boolean)

private val lrSystems = mapOf(
    "PSX" to LrMap(listOf("Sony - PlayStation"), true),
    "Saturn" to LrMap(listOf("Sega - Saturn"), true),
    "MegaCD" to LrMap(listOf("Sega - Mega-CD - Sega CD"), true),
    "TGFX16CD" to LrMap(listOf("NEC - PC Engine CD - TurboGrafx-CD"), true),
    "NES" to LrMap(listOf("Nintendo - Nintendo Entertainment System", "Nintendo - Family Computer Disk System"), false),
    "SNES" to LrMap(listOf("Nintendo - Super Nintendo Entertainment System"), false),
    "Genesis" to LrMap(listOf("Sega - Mega Drive - Genesis"), false),
    "N64" to LrMap(listOf("Nintendo - Nintendo 64"), false),
    "SMS" to LrMap(listOf("Sega - Master System - Mark III", "Sega - SG-1000"), false),
    "TGFX16" to LrMap(listOf("NEC - PC Engine - TurboGrafx 16", "NEC - PC Engine SuperGrafx"), false),
    "ATARI7800" to LrMap(listOf("Atari - 7800", "Atari - 2600"), false),
    "GAMEBOY" to LrMap(listOf("Nintendo - Game Boy", "Nintendo - Game Boy Color"), false),
    "GBA" to LrMap(listOf("Nintendo - Game Boy Advance"), false),
    "GameGear" to LrMap(listOf("Sega - Game Gear"), false),
    "WonderSwan" to LrMap(listOf("Bandai - WonderSwan", "Bandai - WonderSwan Color"), false),
    "AtariLynx" to LrMap(listOf("Atari - Lynx"), false),
)

private val lrDetailLists = listOf("developer", "publisher", "genre", "releaseyear", "maxusers")

class LrEntry(
    val name: String, val crc: String, val serial: String, val size: Long, val lrSys: String,
    var desc: String = "", var dev: String = "", var pub: String = "", var genre: String = "", var year: String = "", var players: String = "",
)

private class LrList(val saved: Long, val entries: List<LrEntry>)

// ---------- the DAT files: clrmamepro format ----------

private class DatEntry {
    var name = ""; var comment = ""; var serial = ""; var crc = ""; var desc = ""; var dev = ""; var pub = ""
    var genre = ""; var year = ""; var players = ""; var size = 0L; var haveRom = false
}

private class Tok(val s: String, val quoted: Boolean)

private fun datTokens(b: ByteArray): List<Tok> {
    val out = ArrayList<Tok>(b.size / 6)
    var i = 0
    fun ws(c: Int) = c == ' '.code || c == '\t'.code || c == '\n'.code || c == '\r'.code || c == 0x0B || c == 0x0C
    while (i < b.size) {
        val c = b[i].toInt() and 0xFF
        if (ws(c)) { i++; continue }
        if (c == '('.code || c == ')'.code) { out.add(Tok(c.toChar().toString(), false)); i++; continue }
        if (c == '"'.code) {
            i++
            val sb = java.io.ByteArrayOutputStream()
            while (i < b.size && b[i].toInt() != '"'.code) {
                if (b[i].toInt() == '\\'.code && i + 1 < b.size) i++
                sb.write(b[i].toInt())
                i++
            }
            i++
            out.add(Tok(String(sb.toByteArray(), Charsets.UTF_8), true))
            continue
        }
        var j = i
        while (j < b.size) {
            val d = b[j].toInt() and 0xFF
            if (d == ' '.code || d == '\t'.code || d == '\n'.code || d == '\r'.code || d == '('.code || d == ')'.code) break
            j++
        }
        out.add(Tok(String(b, i, j - i, Charsets.UTF_8), false))
        i = j
    }
    return out
}

private fun parseDat(b: ByteArray, cb: (DatEntry) -> Unit) {
    val t = datTokens(b)
    var i = 0
    while (i < t.size) {
        if (t[i].quoted || t[i].s != "game") { i++; continue }
        if (i + 1 >= t.size || t[i + 1].s != "(") { i++; continue }
        i += 2
        val e = DatEntry()
        var depth = 1; var inRom = false
        var key = ""
        while (i < t.size && depth > 0) {
            val tk = t[i]
            i++
            if (!tk.quoted && tk.s == "(") {
                depth++
                if (key == "rom") inRom = !e.haveRom
                key = ""
                continue
            }
            if (!tk.quoted && tk.s == ")") {
                depth--
                if (inRom && depth == 1) { inRom = false; e.haveRom = true }
                continue
            }
            if (key.isEmpty()) { key = tk.s; continue }
            val v = tk.s
            if (depth == 1) {
                when (key) {
                    "name" -> e.name = v; "comment" -> e.comment = v; "serial" -> e.serial = v; "description" -> e.desc = v
                    "developer" -> e.dev = v; "publisher" -> e.pub = v; "genre" -> e.genre = v; "releaseyear" -> e.year = v
                    "users", "maxusers" -> e.players = v
                }
            } else if (inRom) {
                when (key) {
                    "crc" -> e.crc = v.take(8).lowercase()
                    "size" -> e.size = v.toLongOrNull() ?: 0
                    "serial" -> if (e.serial.isEmpty()) e.serial = v
                }
            }
            key = ""
        }
        cb(e)
    }
}

/** "???" words out: "Nintendo ???" -> "Nintendo" */
private fun lrTidy(s: String) = s.split(' ', '\t').filter { it.isNotEmpty() && it.trim('?').isNotEmpty() }.joinToString(" ")

class Libretro(private val dir: File) {
    private val mu = Mutex()
    private val lists = HashMap<String, LrList>()
    private val loading = HashMap<String, Mutex>()

    private fun path(lr: String) = File(dir, lr.replace(" ", "_") + ".bin")

    private fun save(f: File, l: LrList) {
        f.parentFile?.mkdirs()
        val tmp = File(f.path + ".new")
        runCatching {
            DataOutputStream(tmp.outputStream().buffered()).use { o ->
                o.writeLong(l.saved); o.writeInt(l.entries.size)
                for (e in l.entries) {
                    o.writeUTF(e.name); o.writeUTF(e.crc); o.writeUTF(e.serial); o.writeLong(e.size); o.writeUTF(e.lrSys)
                    for (v in listOf(e.desc.take(20000), e.dev, e.pub, e.genre, e.year, e.players)) o.writeUTF(v)
                }
            }
            tmp.renameTo(f)
        }
    }

    private fun load(f: File): LrList? = runCatching {
        DataInputStream(f.inputStream().buffered()).use { i ->
            val saved = i.readLong(); val n = i.readInt()
            LrList(saved, List(n) {
                LrEntry(i.readUTF(), i.readUTF(), i.readUTF(), i.readLong(), i.readUTF(), i.readUTF(), i.readUTF(), i.readUTF(), i.readUTF(), i.readUTF(), i.readUTF())
            })
        }
    }.getOrNull()

    /** one list, from the cache or fetched; null on network trouble */
    private suspend fun get(lr: String, disc: Boolean, status: (String) -> Unit): LrList? {
        val now = System.currentTimeMillis()
        mu.withLock { lists[lr]?.takeIf { now - it.saved < FRESH_MS }?.let { return it } }
        val lk = mu.withLock { loading.getOrPut(lr) { Mutex() } }
        return lk.withLock {
            mu.withLock { lists[lr]?.takeIf { now - it.saved < FRESH_MS }?.let { return@withLock it } }
            load(path(lr))?.takeIf { now - it.saved < FRESH_MS }?.let { l -> mu.withLock { lists[lr] = l }; return@withLock l }
            status("Getting the $lr game list")
            val kind = if (disc) "redump" else "no-intro"
            val r = netRequest("GET", "$LR_DB/$kind/${netEscape(lr)}.dat", max = 256L shl 20)
            if (r.rc < 0) return@withLock null
            val entries = ArrayList<LrEntry>()
            if (r.rc == 0 && r.body != null) parseDat(r.body) { e ->
                if (e.name.isNotEmpty()) entries.add(LrEntry(e.name, e.crc, serialNorm(e.serial), e.size, lr))
            }
            val byCrc = HashMap<String, MutableList<Int>>(); val bySer = HashMap<String, MutableList<Int>>(); val byName = HashMap<String, MutableList<Int>>()
            entries.forEachIndexed { i, e ->
                if (e.crc.isNotEmpty()) byCrc.getOrPut(e.crc) { ArrayList() }.add(i)
                if (e.serial.isNotEmpty()) bySer.getOrPut(e.serial) { ArrayList() }.add(i)
                byName.getOrPut(e.name) { ArrayList() }.add(i)
            }
            for (d in lrDetailLists) {
                val dr = netRequest("GET", "$LR_DB/$d/${netEscape(lr)}.dat", max = 256L shl 20)
                if (dr.rc < 0) return@withLock null
                if (dr.rc != 0 || dr.body == null) continue
                parseDat(dr.body) { e ->
                    e.dev = lrTidy(e.dev); e.pub = lrTidy(e.pub); e.genre = lrTidy(e.genre)
                    val ser = serialNorm(e.serial)
                    val nm = e.name.ifEmpty { e.comment }
                    val idx = when {
                        e.crc.isNotEmpty() -> byCrc[e.crc]
                        ser.isNotEmpty() -> bySer[ser]
                        nm.isNotEmpty() -> byName[nm]
                        else -> null
                    } ?: return@parseDat
                    for (i in idx) {
                        val x = entries[i]
                        if (e.desc.isNotEmpty()) x.desc = e.desc
                        if (e.dev.isNotEmpty()) x.dev = e.dev
                        if (e.pub.isNotEmpty()) x.pub = e.pub
                        if (e.genre.isNotEmpty()) x.genre = e.genre
                        if (e.year.isNotEmpty()) x.year = e.year
                        if (e.players.isNotEmpty()) x.players = e.players
                    }
                }
            }
            val l = LrList(System.currentTimeMillis(), entries)
            save(path(lr), l)
            mu.withLock { lists[lr] = l }
            l
        }
    }

    /** no product code or CRC in the lists: the exact list name, or the same title words with the same
     *  disc number, the file's region first */
    private fun byName(all: List<LrEntry>, title: String): LrEntry? {
        all.firstOrNull { it.name.equals(title, ignoreCase = true) }?.let { return it }
        val want = nameKey(title)
        if (want.length < 2) return null
        val wreg = regionOf(title).lowercase()
        val wdisc = discNo(title)
        var best = -1
        var pick: LrEntry? = null
        for (e in all) {
            if (nameKey(e.name) != want) continue
            val d = discNo(e.name)
            if (d != wdisc && !(wdisc == 0 && d == 1)) continue
            val reg = regionOf(e.name).lowercase()
            var score = when {
                wreg.isNotEmpty() && reg == wreg -> 4
                wreg.isNotEmpty() && reg.contains(wreg) -> 3
                wreg.isEmpty() && reg.contains("usa") -> 2
                else -> 1
            }
            if (e.name.contains("(Beta") || e.name.contains("(Proto") || e.name.contains("(Demo")) score--
            if (score > best) { best = score; pick = e }
        }
        return pick
    }

    private fun thumbUrls(lrsys: String, official: String): List<String> {
        val out = ArrayList<String>()
        for (k in 0 until 3) {
            var v = official
            if (k == 1) { // "(USA, Canada)" -> "(USA)"
                val p = v.indexOf('(')
                if (p < 0) continue
                val c = v.indexOf(',', p); val e = v.indexOf(')', p)
                if (c < 0 || e < 0 || c > e) continue
                v = v.substring(0, c) + v.substring(e)
            } else if (k == 2) { // without " (Disc 1)"
                val d = v.indexOf(" (Disc ")
                if (d < 0) continue
                val e = v.indexOf(')', d + 1)
                if (e < 0) continue
                v = v.substring(0, d) + v.substring(e + 1)
            }
            out.add("$LR_THUMBS/${netEscape(lrsys)}/Named_Boxarts/${netEscape(thumbName(v))}.png")
        }
        return out
    }

    suspend fun lookup(s: ScrapeRun, g: SGame, r: SRes): Int {
        val m = lrSystems[g.sys] ?: return 0
        val all = ArrayList<LrEntry>()
        for (lr in m.lr) {
            val l = get(lr, m.disc, s::status) ?: return -1
            all.addAll(l.entries)
        }
        var hit: LrEntry? = null
        val ser = serialNorm(g.serial)
        if (m.disc && ser.isNotEmpty()) {
            for (e in all) if (e.serial == ser && (hit == null || (e.size == g.size && hit.size != g.size))) hit = e
            if (hit == null && g.size > 0) hit = all.firstOrNull { it.serial.contains(ser) && it.size == g.size }
        } else if (!m.disc && g.crc.isNotEmpty()) {
            val crc = g.crc.lowercase()
            hit = all.firstOrNull { it.crc == crc }
        }
        if (hit == null) hit = byName(all, g.title)
        if (hit == null) return 0
        g.official = hit.name; g.lrSys = hit.lrSys
        r.set(F_DESC, hit.desc); r.set(F_DEV, hit.dev); r.set(F_PUB, hit.pub); r.set(F_GENRE, hit.genre)
        r.set(F_YEAR, hit.year); r.set(F_PLAYERS, hit.players); r.set(F_REGION, regionOf(hit.name))
        for (u in thumbUrls(hit.lrSys, hit.name)) r.addArt("libretro", u)
        return 1
    }
}
