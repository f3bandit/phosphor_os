// Phosphor for Android, box art on the phone: the LaunchBox Games Database (no account), as phosphord's
// src_launchbox.c and Phosphor for Windows' scrape_launchbox.go. Metadata.zip (~110 MB) is fetched once
// into the app's cache and read as a stream; what's kept is the games of Phosphor's systems (names,
// alternate names, details, front box file names), in a small database in the cache (fresh again after
// 30 days); the zip is deleted. Looked up by the official name a content match gave.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.database.sqlite.SQLiteDatabase
import android.util.Xml
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.xmlpull.v1.XmlPullParser
import java.io.File
import java.util.zip.ZipFile

private const val LB_ZIP = "https://gamesdb.launchbox-app.com/Metadata.zip"
private const val LB_IMAGES = "https://images.launchbox-app.com/"
private const val FRESH_MS = 30L * 24 * 3600 * 1000

private val lbPlatforms = mapOf(
    "PSX" to listOf("Sony Playstation"), "Saturn" to listOf("Sega Saturn"), "MegaCD" to listOf("Sega CD"), "TGFX16CD" to listOf("NEC TurboGrafx-CD"),
    "NES" to listOf("Nintendo Entertainment System", "Nintendo Famicom Disk System"), "SNES" to listOf("Super Nintendo Entertainment System"),
    "Genesis" to listOf("Sega Genesis"), "N64" to listOf("Nintendo 64"), "SMS" to listOf("Sega Master System", "Sega SG-1000"),
    "TGFX16" to listOf("NEC TurboGrafx-16"), "ATARI7800" to listOf("Atari 7800", "Atari 2600"),
    "GAMEBOY" to listOf("Nintendo Game Boy", "Nintendo Game Boy Color"), "GBA" to listOf("Nintendo Game Boy Advance"),
    "GameGear" to listOf("Sega Game Gear"), "WonderSwan" to listOf("WonderSwan", "WonderSwan Color"), "AtariLynx" to listOf("Atari Lynx"),
    "Arcade" to listOf("Arcade"), "C64" to listOf("Commodore 64"), "MSX" to listOf("Microsoft MSX", "Microsoft MSX2"), "Atari800" to listOf("Atari 800"),
)

private class LbGame(val id: Long, val platform: String, val norm: String, val overview: String, val dev: String, val pub: String,
                     val genre: String, val year: String, val players: String, val order: Int)

private class LbImage(val region: String, val file: String, val flyer: Boolean)

class LaunchBox(private val dir: File) {
    private val load = Mutex()
    private var db: SQLiteDatabase? = null
    /** when getting it last failed (not a connection problem): not tried again for an hour */
    private var failedAt = 0L

    private val dbFile get() = File(dir, "launchbox.db")

    private fun fresh(d: SQLiteDatabase): Boolean = runCatching {
        d.rawQuery("SELECT v FROM meta WHERE k='saved'", null).use { c -> c.moveToFirst() && System.currentTimeMillis() - c.getLong(0) < FRESH_MS }
    }.getOrDefault(false)

    /** the database, from the cache or fetched; null and -1 on network trouble, 1 when it can't be had */
    private suspend fun get(status: (String) -> Unit): Pair<SQLiteDatabase?, Int> = load.withLock {
        db?.let { if (fresh(it)) return@withLock it to 0 }
        if (System.currentTimeMillis() - failedAt < 3600_000) return@withLock null to 1
        withContext(Dispatchers.IO) {
            if (dbFile.exists()) {
                val d = runCatching { SQLiteDatabase.openDatabase(dbFile.path, null, SQLiteDatabase.OPEN_READONLY) }.getOrNull()
                if (d != null && fresh(d)) { db = d; return@withContext d to 0 }
                d?.close()
            }
            dir.mkdirs()
            val zip = File(dir, "launchbox-metadata.zip")
            status("Getting the LaunchBox games database (one time, about 110 MB)")
            var lastMb = -1L
            val rc = netGetFile(LB_ZIP, zip) { done, total ->
                val mb = done shr 20
                if (total > 0 && mb / 8 != lastMb / 8) { lastMb = mb; status("Getting the LaunchBox games database: $mb of ${total shr 20} MB") }
            }
            if (rc < 0) return@withContext null to -1
            if (rc != 0) { failedAt = System.currentTimeMillis(); return@withContext null to 1 }
            status("Reading the LaunchBox games database (one time, a few minutes)")
            val ok = try { importZip(zip); true } catch (e: kotlinx.coroutines.CancellationException) { zip.delete(); throw e } catch (e: Exception) { false }
            zip.delete()
            if (!ok) { failedAt = System.currentTimeMillis(); return@withContext null to 1 }
            val d = runCatching { SQLiteDatabase.openDatabase(dbFile.path, null, SQLiteDatabase.OPEN_READONLY) }.getOrNull() ?: return@withContext null to 1
            db = d
            d to 0
        }
    }

    private suspend fun importZip(zipPath: File) {
        val tmp = File(dir, "launchbox.db.new")
        tmp.delete()
        val out = SQLiteDatabase.openOrCreateDatabase(tmp, null)
        try {
            out.execSQL("CREATE TABLE meta(k TEXT PRIMARY KEY, v INTEGER)")
            out.execSQL("CREATE TABLE games(id INTEGER PRIMARY KEY, platform TEXT, norm TEXT, overview TEXT, dev TEXT, pub TEXT, genre TEXT, year TEXT, players TEXT, ord INTEGER)")
            out.execSQL("CREATE TABLE alt(norm TEXT, id INTEGER)")
            out.execSQL("CREATE TABLE images(id INTEGER, region TEXT, file TEXT, flyer INTEGER)")
            val keep = lbPlatforms.values.flatten().toSet()
            val platformOf = HashMap<Long, String>()
            ZipFile(zipPath).use { zf ->
                val entry = zf.getEntry("Metadata.xml") ?: error("no Metadata.xml in the zip")
                zf.getInputStream(entry).buffered(1 shl 16).use { stream ->
                    val xp = Xml.newPullParser()
                    xp.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false)
                    runCatching { xp.setFeature("http://xmlpull.org/v1/doc/features.html#relaxed", true) }
                    xp.setInput(stream, "UTF-8")
                    out.beginTransaction()
                    val insG = out.compileStatement("INSERT OR REPLACE INTO games VALUES(?,?,?,?,?,?,?,?,?,?)")
                    val insA = out.compileStatement("INSERT INTO alt VALUES(?,?)")
                    val insI = out.compileStatement("INSERT INTO images VALUES(?,?,?,?)")
                    var order = 0; var n = 0
                    var ev = xp.eventType
                    while (ev != XmlPullParser.END_DOCUMENT) {
                        if (ev == XmlPullParser.START_TAG && xp.depth == 2) {
                            val kind = xp.name
                            if (kind == "Game" || kind == "GameAlternateName" || kind == "GameImage") {
                                // the element's children, as name -> text
                                val f = HashMap<String, String>()
                                while (true) {
                                    ev = xp.next()
                                    if (ev == XmlPullParser.END_TAG && xp.depth == 2) break
                                    if (ev == XmlPullParser.START_TAG && xp.depth == 3) {
                                        val k = xp.name
                                        f[k] = runCatching { xp.nextText() }.getOrDefault("")
                                    }
                                    if (ev == XmlPullParser.END_DOCUMENT) break
                                }
                                val id = f["DatabaseID"]?.toLongOrNull()
                                if (id != null) when (kind) {
                                    "Game" -> {
                                        val plat = f["Platform"] ?: ""
                                        if (plat in keep) {
                                            var year = f["ReleaseYear"] ?: ""
                                            val rd = f["ReleaseDate"] ?: ""
                                            if (year.isEmpty() && rd.length >= 4) year = rd.substring(0, 4)
                                            val pl = (f["MaxPlayers"] ?: "").let { if (it == "0") "" else it }
                                            order++
                                            platformOf[id] = plat
                                            insG.clearBindings()
                                            insG.bindLong(1, id); insG.bindString(2, plat); insG.bindString(3, titleNorm(f["Name"] ?: ""))
                                            insG.bindString(4, f["Overview"] ?: ""); insG.bindString(5, f["Developer"] ?: ""); insG.bindString(6, f["Publisher"] ?: "")
                                            insG.bindString(7, (f["Genres"] ?: "").replace(";", ",")); insG.bindString(8, year); insG.bindString(9, pl); insG.bindLong(10, order.toLong())
                                            insG.executeInsert()
                                        }
                                    }
                                    "GameAlternateName" -> if (id in platformOf) {
                                        val norm = titleNorm(f["AlternateName"] ?: "")
                                        if (norm.isNotEmpty()) { insA.bindString(1, norm); insA.bindLong(2, id); insA.executeInsert() }
                                    }
                                    "GameImage" -> platformOf[id]?.let { plat ->
                                        val type = f["Type"] ?: ""
                                        val flyer = type == "Advertisement Flyer - Front" && plat == "Arcade"
                                        if (type == "Box - Front" || flyer) {
                                            insI.bindLong(1, id); insI.bindString(2, f["Region"] ?: ""); insI.bindString(3, f["FileName"] ?: ""); insI.bindLong(4, if (flyer) 1 else 0)
                                            insI.executeInsert()
                                        }
                                    }
                                }
                                if (++n % 5000 == 0) currentCoroutineContext().ensureActive()
                            }
                        }
                        ev = xp.next()
                    }
                    out.execSQL("CREATE INDEX g_norm ON games(platform, norm)")
                    out.execSQL("CREATE INDEX a_norm ON alt(norm)")
                    out.execSQL("CREATE INDEX i_id ON images(id)")
                    out.execSQL("INSERT INTO meta VALUES('saved', ${System.currentTimeMillis()})")
                    out.setTransactionSuccessful()
                    out.endTransaction()
                }
            }
        } finally {
            out.close()
        }
        db?.close(); db = null
        File(dir, "launchbox.db-journal").delete()
        if (!tmp.renameTo(dbFile)) error("can't keep the database")
    }

    private fun game(d: SQLiteDatabase, where: String, args: Array<String>): List<LbGame> =
        d.rawQuery("SELECT id, platform, norm, overview, dev, pub, genre, year, players, ord FROM games WHERE $where", args).use { c ->
            val out = ArrayList<LbGame>()
            while (c.moveToNext()) out.add(LbGame(c.getLong(0), c.getString(1), c.getString(2), c.getString(3), c.getString(4), c.getString(5), c.getString(6), c.getString(7), c.getString(8), c.getInt(9)))
            out
        }

    suspend fun lookup(s: ScrapeRun, g: SGame, r: SRes): Int {
        val plats = lbPlatforms[g.sys] ?: return 0
        if (g.official.isEmpty()) return 0
        val (d, rc) = get(s::status)
        if (rc < 0) return -1
        if (rc != 0 || d == null) return 0
        val norm = titleNorm(nameBase(g.official))
        if (norm.isEmpty()) return 0
        // the name itself first, then an alternate name; the first platform first
        val best = withContext(Dispatchers.IO) {
            fun rank(x: LbGame) = (if (x.norm == norm) 2 else 0) + (if (x.platform == plats[0]) 1 else 0)
            var best: LbGame? = null
            fun consider(x: LbGame) {
                if (x.platform !in plats) return
                val b = best
                if (b == null || rank(x) > rank(b) || (rank(x) == rank(b) && x.order < b.order)) best = x
            }
            for (p in plats) game(d, "platform=? AND norm=?", arrayOf(p, norm)).sortedBy { it.order }.forEach { consider(it) }
            game(d, "id IN (SELECT id FROM alt WHERE norm=?)", arrayOf(norm)).forEach { consider(it) }
            best
        } ?: return 0
        r.set(F_DESC, best.overview); r.set(F_DEV, best.dev); r.set(F_PUB, best.pub); r.set(F_GENRE, best.genre)
        r.set(F_YEAR, best.year); r.set(F_PLAYERS, best.players)
        val want = when {
            g.official.contains("(USA") || g.official.contains("USA)") -> "North America"
            g.official.contains("Europe") -> "Europe"
            g.official.contains("Japan") -> "Japan"
            else -> "North America"
        }
        val imgs = withContext(Dispatchers.IO) {
            d.rawQuery("SELECT region, file, flyer FROM images WHERE id=? ORDER BY rowid", arrayOf(best.id.toString())).use { c ->
                val out = ArrayList<LbImage>()
                while (c.moveToNext()) out.add(LbImage(c.getString(0), c.getString(1), c.getInt(2) != 0))
                out
            }
        }
        fun score(i: LbImage) = (if (!i.flyer) 8 else 0) + (if (i.region == want) 4 else 0) + (if (i.region == "World") 2 else 0) + (if (i.region.isEmpty()) 1 else 0)
        imgs.sortedByDescending { score(it) }.take(2).forEach { r.addArt("launchbox", LB_IMAGES + netEscape(it.file)) }
        return 1
    }

    /** bytes the database takes in the cache */
    fun size(): Long = dbFile.length()
}
