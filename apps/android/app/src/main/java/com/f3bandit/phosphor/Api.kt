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

data class CloudPlace(val name: String, val type: String, val inUse: Boolean)

data class CloudState(
    val installed: Boolean, val ready: Boolean, val remote: String, val device: String, val lastPush: String,
    val pending: Boolean, val lastError: String, val places: List<CloudPlace>,
    val job: String, val busy: Boolean, val jobOk: Boolean, val output: String, val at: Long,
)

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
    suspend fun link(id: Long, base: Long) { cmd(obj("cmd" to "link", "id" to id, "base" to base)) }
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

    /** the game's box art (JPEG), or null when it has none */
    suspend fun art(id: Long): ByteArray? = withContext(Dispatchers.IO) {
        try { request("GET", "/art/$id") } catch (e: PhosphorError) { if (e.code == 404) null else throw e }
    }

    suspend fun setArt(id: Long, image: ByteArray, type: String = "image/jpeg"): Game = withContext(Dispatchers.IO) {
        val r = JSONObject(String(request("PUT", "/art/$id", image, type, 60_000)))
        if (!r.optBoolean("ok")) throw PhosphorError(r.optString("error").ifEmpty { "Couldn't store the picture" })
        parseGame(r.getJSONObject("game"))
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
