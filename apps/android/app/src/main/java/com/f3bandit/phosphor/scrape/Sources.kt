// Phosphor for Android, box art on the phone: ArcadeDB (no account) and the sources that need an account
// or key (TheGamesDB, ScreenScraper, IGDB), as phosphord's src_arcadedb.c and src_accounts.c and Phosphor
// for Windows' scrape_sources.go. The logins are the ones saved on the MiSTer (sent to the phone over
// the home network) and only go to those services.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONArray
import org.json.JSONObject
import java.util.Calendar
import java.util.TimeZone

class PlatNums(val tgdb: Int, val ss: Int, val igdb: List<Int>)

val platOf = mapOf(
    "PSX" to PlatNums(10, 57, listOf(7)), "Saturn" to PlatNums(17, 22, listOf(32)), "MegaCD" to PlatNums(21, 20, listOf(78)),
    "TGFX16CD" to PlatNums(4955, 114, listOf(150)), "NES" to PlatNums(7, 3, listOf(18, 51)), "SNES" to PlatNums(6, 4, listOf(19)),
    "Genesis" to PlatNums(18, 1, listOf(29)), "N64" to PlatNums(3, 14, listOf(4)), "SMS" to PlatNums(35, 2, listOf(64, 84)),
    "TGFX16" to PlatNums(34, 31, listOf(86)), "ATARI7800" to PlatNums(27, 41, listOf(60, 59)),
    "GAMEBOY" to PlatNums(4, 9, listOf(33, 22)), "GBA" to PlatNums(5, 12, listOf(24)), "GameGear" to PlatNums(20, 21, listOf(35)),
    "WonderSwan" to PlatNums(4925, 45, listOf(57, 123)), "AtariLynx" to PlatNums(4924, 28, listOf(61)), "Arcade" to PlatNums(23, 75, listOf(52)),
    "C64" to PlatNums(40, 66, listOf(15)), "MSX" to PlatNums(4929, 113, listOf(27)), "Atari800" to PlatNums(4943, 43, listOf(65)),
)

/** a string field ("" when missing or not a string, like the Go's jstr) */
fun js(o: JSONObject?, k: String): String = (o?.opt(k) as? String) ?: ""
fun jo(o: JSONObject?, k: String): JSONObject? = o?.opt(k) as? JSONObject
fun ja(o: JSONObject?, k: String): JSONArray = (o?.opt(k) as? JSONArray) ?: JSONArray()
fun jnum(o: JSONObject?, k: String): Double? = (o?.opt(k) as? Number)?.toDouble()

private fun parse(b: ByteArray?): JSONObject? = b?.let { runCatching { JSONObject(String(it, Charsets.UTF_8)) }.getOrNull() }

// ---------- ArcadeDB ----------

private val adbPace = Pace(250)

private fun adbHistory(h: String): String {
    if (h.isEmpty()) return ""
    var s = h
    if (s.startsWith("Arcade Video game published")) { val i = s.indexOf('\n'); if (i >= 0) s = s.substring(i + 1) }
    s = s.replace("\r", "")
    for (c in listOf("- TECHNICAL -", "- TRIVIA -", "- CONTRIBUTE -", "- UPDATES -", "- SERIES -", "- STAFF -")) {
        val i = s.indexOf(c); if (i >= 0) s = s.substring(0, i)
    }
    return s.trim('\n', ' ')
}

/** the set's entry, and 0 found / 1 not found / -1 trouble */
private suspend fun adbQuery(set: String): Pair<JSONObject?, Int> {
    adbPace.await()
    val r = netRequest("GET", "https://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en&game_name=" + netEscape(set))
    if (r.rc != 0) return null to r.rc
    val j = parse(r.body) ?: return null to 1
    val res = ja(j, "result")
    if (res.length() == 0) return null to 1
    val x = res.opt(0) as? JSONObject ?: return null to 1
    return x to 0
}

private fun adbFill(x: JSONObject, r: SRes) {
    r.set(F_DEV, js(x, "manufacturer")); r.set(F_PUB, js(x, "manufacturer"))
    r.set(F_YEAR, js(x, "year")); r.set(F_GENRE, js(x, "genre"))
    jnum(x, "players")?.let { if (it > 0) r.set(F_PLAYERS, it.toInt().toString()) }
    r.set(F_DESC, adbHistory(js(x, "history")))
}

/** a hack or a homebrew name: never looked up by its title */
private fun arcadeHackName(t: String): Boolean {
    val l = t.lowercase()
    return l.contains("hbmame") || l.contains("hack") || l.contains("[h]") || l.contains("trainer")
}

suspend fun adbLookup(s: ScrapeRun, g: SGame, r: SRes): Int {
    if (!g.arcade) return 0
    if (g.serial.isEmpty()) { // no MAME set name (a laserdisc game, a TTL machine): looked up by its name
        if (g.official.isEmpty() && !arcadeHackName(g.title)) g.official = g.title
        return 0
    }
    val (x, rc) = adbQuery(g.serial)
    if (rc < 0) return -1
    if (x == null) {
        // not a MAME set (a MiSTer-only set, homebrew): its .mra name is the game's own name
        if (g.official.isEmpty() && !arcadeHackName(g.title)) g.official = g.title
        return 0
    }
    js(x, "title").let { if (it.isNotEmpty() && g.official.isEmpty()) g.official = it }
    adbFill(x, r)
    // a clone mostly has no flyer of its own: its parent's is the game's
    if (js(x, "url_image_flyer").isEmpty() && js(x, "cloneof").isNotEmpty()) {
        val (p, prc) = adbQuery(js(x, "cloneof"))
        if (prc < 0) return -1
        if (p != null) {
            adbFill(p, r)
            r.addArt("arcadedb", js(p, "url_image_flyer"))
            r.addArt("arcadedb", js(x, "url_image_title"))
            r.addArt("arcadedb", js(p, "url_image_title"))
            return 1
        }
    }
    r.addArt("arcadedb", js(x, "url_image_flyer"))
    r.addArt("arcadedb", js(x, "url_image_title"))
    return 1
}

// ---------- TheGamesDB ----------

suspend fun tgdbLookup(s: ScrapeRun, g: SGame, r: SRes): Int {
    val p = platOf[g.sys] ?: return 0
    if (g.official.isEmpty()) return 0
    val base = nameBase(g.official)
    val u = "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=${netEscape(s.acct("thegamesdb", "apikey"))}&name=${netEscape(base)}" +
        "&fields=players,overview&filter%5Bplatform%5D=${p.tgdb}&include=boxart"
    val res = netRequest("GET", u)
    if (res.rc == 2) { s.note("TheGamesDB refused the API key"); return 0 }
    if (res.rc != 0) return if (res.rc < 0) -1 else 0
    val j = parse(res.body) ?: return 0
    val want = titleNorm(base)
    val games = ja(jo(j, "data"), "games")
    var best: JSONObject? = null
    for (i in 0 until games.length()) {
        val m = games.opt(i) as? JSONObject ?: continue
        if (titleNorm(js(m, "game_title")) == want) { best = m; break }
    }
    if (best == null) return 0
    r.set(F_DESC, js(best, "overview"))
    js(best, "release_date").let { if (it.length >= 4) r.set(F_YEAR, it.substring(0, 4)) }
    jnum(best, "players")?.let { if (it > 0) r.set(F_PLAYERS, it.toInt().toString()) }
    val bx = jo(jo(j, "include"), "boxart")
    val baseUrl = js(jo(bx, "base_url"), "original")
    val id = jnum(best, "id")?.toInt() ?: -1
    val imgs = jo(bx, "data")?.opt(id.toString()) as? JSONArray
    if (imgs != null) for (i in 0 until imgs.length()) {
        val m = imgs.opt(i) as? JSONObject ?: continue
        if (baseUrl.isNotEmpty() && js(m, "filename").isNotEmpty() && js(m, "side") == "front") { r.addArt("thegamesdb", baseUrl + js(m, "filename")); break }
    }
    return 1
}

// ---------- ScreenScraper ----------

private val ssLock = Mutex() // one request at a time (a user's thread limit)

/** a developer login, or the regular screenscraper.fr login on its own (used for both then) */
private fun ssDev(s: ScrapeRun, k: String, userK: String) =
    if (s.acct("screenscraper", "devid").isNotEmpty()) s.acct("screenscraper", k) else s.acct("screenscraper", userK)

private fun langText(arr: JSONArray, field: String): String {
    var first: JSONObject? = null
    for (i in 0 until arr.length()) {
        val m = arr.opt(i) as? JSONObject ?: continue
        if (first == null) first = m
        if (js(m, "langue") == "en") return js(m, field)
    }
    return first?.let { js(it, field) } ?: ""
}

suspend fun ssLookup(s: ScrapeRun, g: SGame, r: SRes): Int {
    val p = platOf[g.sys] ?: return 0
    if (g.crc.isEmpty() && g.serial.isEmpty()) return 0
    val nm = if (g.arcade) g.serial else g.title
    var u = "https://api.screenscraper.fr/api2/jeuInfos.php?devid=${netEscape(ssDev(s, "devid", "user"))}&devpassword=${netEscape(ssDev(s, "devpassword", "password"))}" +
        "&softname=Phosphor&output=json&ssid=${netEscape(s.acct("screenscraper", "user"))}&sspassword=${netEscape(s.acct("screenscraper", "password"))}" +
        "&systemeid=${p.ss}&romtype=rom&romnom=${netEscape(nm)}"
    if (g.crc.isNotEmpty()) u += "&crc=" + g.crc
    if (g.disc && g.serial.isNotEmpty()) u += "&serialnum=" + netEscape(g.serial)
    if (g.size > 0) u += "&romtaille=" + g.size
    val res = ssLock.withLock { netRequest("GET", u, max = 16L shl 20) }
    if (res.rc == 2) { s.note("ScreenScraper refused the login (HTTP ${res.http})"); return 0 }
    if (res.http == 430 || res.http == 429) return -1
    if (res.rc != 0) return if (res.rc < 0) -1 else 0
    val j = parse(res.body) ?: return 0
    val jeu = jo(jo(j, "response"), "jeu") ?: return 0
    if (g.official.isEmpty()) langText(ja(jeu, "noms"), "text").let { if (it.isNotEmpty()) g.official = it }
    r.set(F_DESC, langText(ja(jeu, "synopsis"), "text"))
    r.set(F_DEV, js(jo(jeu, "developpeur"), "text"))
    r.set(F_PUB, js(jo(jeu, "editeur"), "text"))
    r.set(F_PLAYERS, js(jo(jeu, "joueurs"), "text"))
    (ja(jeu, "genres").opt(0) as? JSONObject)?.let { r.set(F_GENRE, langText(ja(it, "noms"), "text")) }
    (ja(jeu, "dates").opt(0) as? JSONObject)?.let { d -> js(d, "text").let { if (it.length >= 4) r.set(F_YEAR, it.substring(0, 4)) } }
    val media = ja(jeu, "medias")
    for (reg in listOf("us", "wor", "eu", "ss", "jp")) {
        for (i in 0 until media.length()) {
            val m = media.opt(i) as? JSONObject ?: continue
            if (js(m, "type") == "box-2D" && js(m, "region") == reg && js(m, "url").isNotEmpty()) { r.addArt("screenscraper", js(m, "url")); return 1 }
        }
    }
    return 1
}

// ---------- IGDB ----------

private object Igdb {
    val mu = Mutex()
    var token = ""
    var expires = 0L
}

private val igdbPace = Pace(260)

/** 0 ok, 1 no (refused), -1 trouble */
private suspend fun igdbAuth(s: ScrapeRun): Int = Igdb.mu.withLock {
    if (Igdb.token.isNotEmpty() && System.currentTimeMillis() < Igdb.expires - 60_000) return@withLock 0
    val u = "https://id.twitch.tv/oauth2/token?client_id=${netEscape(s.acct("igdb", "client_id"))}&client_secret=${netEscape(s.acct("igdb", "client_secret"))}&grant_type=client_credentials"
    val res = netRequest("POST", u, body = ByteArray(0), max = 1L shl 20)
    if (res.rc != 0) {
        if (res.rc == 1 || res.rc == 2) s.note("IGDB: Twitch refused the client id/secret")
        return@withLock if (res.rc < 0) -1 else 1
    }
    val j = parse(res.body)
    if (j == null || js(j, "access_token").isEmpty()) return@withLock 1
    Igdb.token = js(j, "access_token")
    Igdb.expires = System.currentTimeMillis() + ((jnum(j, "expires_in") ?: 3600.0) * 1000).toLong()
    0
}

suspend fun igdbLookup(s: ScrapeRun, g: SGame, r: SRes): Int {
    val p = platOf[g.sys] ?: return 0
    if (g.official.isEmpty()) return 0
    when (igdbAuth(s)) { 0 -> {}; -1 -> return -1; else -> return 0 }
    val base = nameBase(g.official).replace("\"", "'")
    val q = "search \"$base\"; fields name,summary,first_release_date,genres.name,involved_companies.company.name,involved_companies.developer," +
        "involved_companies.publisher,cover.image_id; where platforms = (${p.igdb.joinToString(",")}); limit 10;"
    val tok = Igdb.mu.withLock { Igdb.token }
    igdbPace.await()
    val res = netRequest("POST", "https://api.igdb.com/v4/games",
        mapOf("Client-ID" to s.acct("igdb", "client_id"), "Authorization" to "Bearer $tok", "Accept" to "application/json"), q.toByteArray())
    if (res.rc == 2) { Igdb.mu.withLock { Igdb.token = "" }; return 0 }
    if (res.rc != 0) return if (res.rc < 0) -1 else 0
    val arr = res.body?.let { runCatching { JSONArray(String(it, Charsets.UTF_8)) }.getOrNull() } ?: return 0
    val want = titleNorm(nameBase(g.official))
    var best: JSONObject? = null
    for (i in 0 until arr.length()) {
        val x = arr.opt(i) as? JSONObject ?: continue
        if (titleNorm(js(x, "name")) == want) { best = x; break }
    }
    if (best == null) return 0
    r.set(F_DESC, js(best, "summary"))
    jnum(best, "first_release_date")?.let {
        val c = Calendar.getInstance(TimeZone.getTimeZone("UTC")); c.timeInMillis = it.toLong() * 1000
        r.set(F_YEAR, c.get(Calendar.YEAR).toString())
    }
    (ja(best, "genres").opt(0) as? JSONObject)?.let { r.set(F_GENRE, js(it, "name")) }
    val ic = ja(best, "involved_companies")
    for (i in 0 until ic.length()) {
        val m = ic.opt(i) as? JSONObject ?: continue
        val co = js(jo(m, "company"), "name")
        if (m.optBoolean("developer")) r.set(F_DEV, co)
        if (m.optBoolean("publisher")) r.set(F_PUB, co)
    }
    js(jo(best, "cover"), "image_id").let { if (it.isNotEmpty()) r.addArt("igdb", "https://images.igdb.com/igdb/image/upload/t_cover_big/$it.jpg") }
    return 1
}
