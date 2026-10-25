// Phosphor for Android: automatic matching (the same rules as Phosphor for Windows' automatch.go). The
// games box art couldn't find are matched to the game they're based on by their name. A translation
// ([T-En]), a hack ((Hack), [h], Kaizo) or a fan version (Redux, FastROM, Undub, v1.1 ...) is the base
// game with something added: take that out and what's left is the base game's name.
//   - The base game is in the library: linked to it ("Based on").
//   - It isn't: the box art sources are asked for it by name (the game's own name too, made readable).
//   - Boot files, test ROMs and tools are set aside as not games.
// Every match is kept here (files/automatch.json) for review: Keep, Undo; an undone game isn't matched
// automatically again.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.content.Context
import com.f3bandit.phosphor.PhosphorApi
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

// ---------- what a name says ----------

private val amTagRe = Regex("""\s*[(\[][^)\]]*[)\]]""")
private val amTransRe = Regex("""(?i)\[T[-+]?[A-Za-z]{2,3}\b[^\]]*\]|\((?:[^)]*\b)?(?:translat(?:ed|ion)|english)\b[^)]*\)|\beng(?:lish)? ?(?:dub(?:bed)?|patch(?:ed)?|trans(?:lation)?)\b""")
private val amHackRe = Regex("""(?i)\[h\d*[^\]]*\]|\((?:[^)]*\b)?(?:rom ?)?hack(?:ed)?\b[^)]*\)|\b(?:rom ?hack|kaizo)\b""")
private val amFanRe = Regex("""(?i)\b(fast ?rom|restoration|restored|complete|deluxe|perfect edition|special edition|special version|""" +
    """enhanced( colou?rs)?|enhancement|improvement|undub|purist|redux|remastered|msu-?1|sram|redesign(ed)?|rebalanced|""" +
    """major fixes( for)?|black label|sa-?1|arcade colou?rs|6-button( support)?|master patched|patched|un-?worked designs|""" +
    """grind-be-gone|v\d+(\.\d+)*[a-z]?)\b""")
private val amWordRe = Regex("""(?i)\b(rom ?hack|hack(ed)?|kaizo|translat(ed|ion)|eng(lish)? ?(dub(bed)?|patch(ed)?|trans(lation)?)|""" +
    """(nes|famicom|fc|snes)[ -]?(port(ed)?|conversion)|nes ?2 ?snes)\b""")
// a game ported from another system: "Super Mario Bros. (SNES Port)", "Zelda II NES2SNES"
private val amPortRe = Regex("""(?i)\((?:[^)]*\b)?(?:port(?:ed)?|conversion)\b[^)]*\)|\b(?:nes|famicom|fc|snes)[ -]?(?:port(?:ed)?|conversion)\b|\bnes ?2 ?snes\b""")

/** the system a port came from ("" when it isn't known): the SNES ports are NES games */
fun amPortFrom(sys: String, tag: String): String {
    val t = tag.lowercase()
    return when {
        sys == "SNES" && "snes to" !in t -> "NES"
        "nes" in t && "snes" !in t && sys != "NES" -> "NES"
        else -> ""
    }
}
private val amStop = setOf("the", "of", "a", "an", "and")
private val amNotGameRe = Regex("""(?i)^(mister[- ](boot|demo)|boot ?\d*|empty|bootdisc)$|\b(btn ?test|pad ?test|controller (tester|test)|test ?rom|service disc|game ?shark|action replay|tonyhax|unirom|rom flasher|boot ?disc|trainer)\b""")
private val amVerRe = Regex("""(?i)[\s._-]*v?\d+(\.\d+)+[a-z]?$""")
private val amDotRe = Regex("""([A-Za-z0-9])\.([A-Za-z])""")
private val amCamelRe = Regex("""([a-z])([A-Z])""")
private val amDiscRe = Regex("""(?i)\s*-?\s*\bdisc\s*(\d+)\b""")
private val amRoman = mapOf("ii" to "2", "iii" to "3", "iv" to "4", "v" to "5", "vi" to "6", "vii" to "7", "viii" to "8", "ix" to "9", "x" to "10",
    "xi" to "11", "xii" to "12", "xiii" to "13", "xiv" to "14", "xv" to "15", "xvi" to "16")
private fun fields(s: String) = s.split(' ', '\t', '\n', '\r').filter { it.isNotEmpty() }

/** a file-style name made readable: "Zeldas.Adventure.DX.v2.0.0" -> "Zeldas Adventure DX" */
fun amClean(t0: String): String {
    var t = t0.replace("_", " ")
    t = amVerRe.replace(t, "")
    t = amDotRe.replace(t, "$1 $2")
    if (!t.lowercase().contains("(disc")) t = amDiscRe.replace(t, " (Disc $1)")
    if (!t.trim().contains(' ')) {
        t = amCamelRe.replace(t, "$1 $2")
        if (!t.contains(' ')) t = t.replace("-", " ")
    }
    return fields(t).joinToString(" ")
}

/** a system's short name stuck on the end of a word ("icarusnes" -> "icarus"), each way it can be read */
fun amGlued(t: String): List<String> {
    val ws = fields(t)
    if (ws.isEmpty()) return emptyList()
    val last = ws.last().lowercase()
    return listOf("snes", "nes", "gbc", "gba", "sfc", "smd", "pce").filter { last.endsWith(it) && last.length >= it.length + 4 }
        .map { (ws.dropLast(1) + ws.last().substring(0, last.length - it.length)).joinToString(" ") }
}

/** a boot file, test ROM or tool: the words that said so */
fun amNotGame(t: String): String = amNotGameRe.find(amTagRe.replace(t, " ").trim())?.value ?: ""

/** what the name says the game is, and the words that said it */
fun amKind(title: String): Pair<String, String> {
    amPortRe.find(title)?.let { return "port" to it.value.trim() }
    amTransRe.find(title)?.let { return "translation" to it.value.trim() }
    amHackRe.find(title)?.let { return "hack" to it.value.trim() }
    amFanRe.find(title)?.let { return "fan" to it.value }
    return "" to ""
}

/** the title's words for comparing, no tags, no "the", "of" ...; Roman numbers as digits */
fun amWords(t: String): String = fields(nameKey(t)).filter { it !in amStop }.map { amRoman[it] ?: it }.joinToString(" ")

/** amWords without the spaces: "Sa-Ga" and "SaGa", "Pac Man" and "Pacman" are the same */
fun amKey(t: String): String = amWords(t).replace(" ", "")

/** the base game's name: tags and the fan version's words out */
fun amStrip(t0: String): String {
    var t = amTagRe.replace(t0, " ").replace("_", " ")
    t = amFanRe.replace(t, " ")
    t = amWordRe.replace(t, " ")
    return fields(t).joinToString(" ").trim(' ', '-', ':')
}

/** the main title, before " - " */
fun amMain(t: String): String = t.indexOf(" - ").let { if (it > 0) t.substring(0, it) else t }

fun amRegionScore(name: String, want: String): Int {
    val reg = regionOf(name).lowercase()
    var score = 0
    if (want.isNotEmpty() && reg.isNotEmpty() && (reg == want || reg.contains(want) || want.contains(reg))) score = 100
    for ((i, r) in listOf("usa", "world", "europe", "japan").withIndex()) if (reg.contains(r)) { score += 40 - 10 * i; break }
    for (bad in listOf("beta", "proto", "demo", "sample", "pirate")) if ("$reg ${name.lowercase()}".contains(bad)) score -= 30
    return score
}

/** the same word, or one spelt a little shorter ("Saiya" / "Saiyan") */
fun amSameWord(a0: String, b0: String): Boolean {
    if (a0 == b0) return true
    val (a, b) = if (a0.length > b0.length) b0 to a0 else a0 to b0
    return a.length >= 4 && b.length - a.length <= 2 && b.startsWith(a)
}

// ---------- in the library ----------

class AmLibGame(val id: Long, val title: String, val linked: Boolean, val art: Boolean)

/** the game in the library this one is based on: the same name with the tags out, or the main title */
fun amLibBase(id: Long, title: String, lib: List<AmLibGame>): AmLibGame? {
    val want = regionOf(title).lowercase()
    val cands = lib.filter { it.id != id && !it.linked && amKind(it.title).first.isEmpty() }
    fun pick(key: String): AmLibGame? {
        if (key.length < 2) return null
        return cands.filter { amKey(it.title) == key }.maxByOrNull { amRegionScore(it.title, want) + if (it.art) 5 else 0 }
    }
    val base = amStrip(amClean(title))
    pick(amKey(base))?.let { return it }
    val m = amMain(base)
    return if (m != base) pick(amKey(m)) else null
}

// ---------- in the lists ----------

/** a game in the lists under the same name written differently: exact, main (one game under the main
 *  title), fuzzy (a subtitle sharing most of its words with one game only); "" when none */
fun amLoose(all: List<LrEntry>, title: String): Pair<LrEntry?, String> {
    val base = amStrip(amClean(title))
    val want = amKey(base)
    if (want.isEmpty()) return null to ""
    val main = amMain(base)
    val mainK = amKey(main)
    val wreg = regionOf(title).lowercase()
    val wdisc = discNo(amClean(title))
    fun words(t: String) = fields(amWords(t)).filter { it != "super" && it.trim { c -> c in '0'..'9' }.isNotEmpty() }
    val sub = if (main != base) words(base.removePrefix(main).trim()) else emptyList()
    fun score(e: LrEntry): Int {
        var v = amRegionScore(e.name, wreg) - 8 * (amTagRe.findAll(e.name).count() - 1)
        val d = discNo(e.name)
        if (d == wdisc || (wdisc == 0 && d <= 1)) v += 60
        return v
    }
    val exact = ArrayList<LrEntry>()
    val groups = LinkedHashMap<String, ArrayList<LrEntry>>()
    val shared = HashMap<String, Pair<Int, Int>>()
    for (e in all) {
        if (amKind(e.name).first.isNotEmpty()) continue
        val nb = amStrip(e.name)
        val nk = amKey(nb)
        if (nk == want) { exact.add(e); continue }
        if (mainK.length < 4 || amKey(amMain(nb)) != mainK) continue
        if (nk !in groups) {
            val tw = words(nb.removePrefix(amMain(nb)))
            shared[nk] = sub.count { w -> tw.any { amSameWord(w, it) } } to tw.size
        }
        groups.getOrPut(nk) { ArrayList() }.add(e)
    }
    if (exact.isNotEmpty()) return exact.maxByOrNull { score(it) } to "exact"
    if (groups.size == 1) {
        val (k, cs) = groups.entries.first()
        val sh = shared[k]!!
        if (sub.isNotEmpty() && sh.second > 0 && sh.first == 0) return null to ""
        return cs.maxByOrNull { score(it) } to "main"
    }
    if (sub.size >= 2) {
        var top = 0; var second = 0; var pick = ""
        for ((k, x) in shared) {
            if (x.first > top) { second = top; top = x.first; pick = k } else if (x.first > second) second = x.first
        }
        if (top >= 2 && top > second && top * 3 >= sub.size * 2 && top * 2 >= shared[pick]!!.second)
            return groups[pick]!!.maxByOrNull { score(it) } to "fuzzy"
    }
    return null to ""
}

// ---------- the matches kept for review ----------

class AmEntry(
    val id: Long, val title: String, val sys: String, val system: String, var kind: String, var tag: String,
    var how: String = "", var baseId: Long = 0, var baseTitle: String = "", var foundBy: List<String> = emptyList(),
    var fields: List<String> = emptyList(), var art: Boolean = false, val at: Long = System.currentTimeMillis() / 1000, var kept: Boolean = false,
) {
    fun json(): JSONObject = JSONObject().put("id", id).put("title", title).put("sys", sys).put("system", system).put("kind", kind).put("tag", tag)
        .put("how", how).put("base_id", baseId).put("base_title", baseTitle).put("found_by", JSONArray(foundBy)).put("fields", JSONArray(fields))
        .put("art", art).put("at", at).put("kept", kept)

    companion object {
        fun of(j: JSONObject): AmEntry {
            fun list(k: String) = j.optJSONArray(k)?.let { a -> (0 until a.length()).map { a.optString(it) } } ?: emptyList()
            return AmEntry(j.optLong("id"), j.optString("title"), j.optString("sys"), j.optString("system"), j.optString("kind"), j.optString("tag"),
                j.optString("how"), j.optLong("base_id"), j.optString("base_title"), list("found_by"), list("fields"), j.optBoolean("art"),
                j.optLong("at"), j.optBoolean("kept"))
        }
    }
}

object AutoMatch {
    private val lock = Any()
    private var file: File? = null
    private val entries = LinkedHashMap<Long, AmEntry>()
    private val undone = HashSet<Long>()

    fun init(c: Context): Unit = synchronized(lock) {
        if (file != null) return
        file = File(c.applicationContext.filesDir, "automatch.json")
        runCatching {
            val j = JSONObject(file!!.readText())
            j.optJSONObject("entries")?.let { e -> e.keys().forEach { k -> e.optJSONObject(k)?.let { entries[it.optLong("id")] = AmEntry.of(it) } } }
            j.optJSONArray("undone")?.let { a -> for (i in 0 until a.length()) undone.add(a.optLong(i)) }
        }
    }

    private fun save() {
        val e = JSONObject(); entries.values.forEach { e.put(it.id.toString(), it.json()) }
        val f = file ?: return
        val tmp = File(f.path + ".tmp")
        runCatching { tmp.writeText(JSONObject().put("entries", e).put("undone", JSONArray(undone.toList())).toString()); tmp.renameTo(f) }
    }

    fun isUndone(id: Long) = synchronized(lock) { id in undone }
    fun add(e: AmEntry) = synchronized(lock) { entries[e.id] = e; save() }
    fun get(id: Long) = synchronized(lock) { entries[id] }

    /** pending first, then by system and title */
    fun list(): JSONObject = synchronized(lock) {
        val l = entries.values.sortedWith(compareBy<AmEntry>({ it.kept }, { it.sys }, { it.title.lowercase() }))
        JSONObject().put("matches", JSONArray(l.map { it.json() })).put("undone", undone.size)
    }

    fun keep(id: Long?) = synchronized(lock) { entries.values.filter { id == null || it.id == id }.forEach { it.kept = true }; save() }
    fun forget(id: Long) = synchronized(lock) { entries.remove(id); save() }

    /** undo: only what the match put in; not matched automatically again */
    suspend fun undo(api: PhosphorApi, id: Long) {
        val e = synchronized(lock) { entries[id] } ?: throw IllegalStateException("no such match")
        when (e.how) {
            "library" -> api.unlink(e.id)
            "skip" -> api.notGame(e.id, false)
            else -> api.scrapeUndo(e.id, e.fields, e.art)
        }
        synchronized(lock) { entries.remove(id); undone.add(id); save() }
    }
}
