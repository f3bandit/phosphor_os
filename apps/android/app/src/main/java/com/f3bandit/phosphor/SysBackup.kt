// Phosphor for Android: MiSTer & Linux backups (SS1 Tool's, like Phosphor for Windows' sysbk.go): every
// version of the SS1's settings files (MiSTer.ini and its profiles, Phosphor's settings, the network
// address, name and SSH settings) and system files (Main, the menu core, the Linux kernel), kept in the
// app's own storage (files/sysbk/<id>/<sha256>/). Linux itself (linux.img, about 400 MB) is left to
// Phosphor for Windows. A file is only fetched when it changed; one that changed when Linux did looks
// reset by the update and is flagged. Putting one back sends it in pieces; phosphord checks it and keeps
// the file as it was on the SS1 first. Passwords, logins, pairings and keys never come here.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest

object SysBackup {
    private const val SKIP = "linux+linux.img"      // ~400 MB: Phosphor for Windows keeps it
    private const val PIECE = 512 * 1024
    private val lock = Mutex()
    @Volatile var busy = false; private set
    @Volatile var job = ""; private set
    @Volatile var file = ""; private set
    @Volatile var done = 0L; private set
    @Volatile var total = 0L; private set
    @Volatile var error = ""; private set
    @Volatile var restart = false; private set
    var onChange: () -> Unit = {}

    private fun root(c: Context) = File(c.filesDir, "sysbk")
    private fun safe(s: String) = s.isNotEmpty() && !s.contains('/') && !s.contains('\\') && !s.contains("..")

    data class Version(val hash: String, val size: Long, val linux: String, val seen: Long)

    fun versions(c: Context, id: String): List<Version> =
        File(root(c), id).listFiles().orEmpty().mapNotNull { d ->
            runCatching { JSONObject(File(d, "info.json").readText()) }.getOrNull()
                ?.takeIf { it.optString("hash") == d.name }
                ?.let { Version(it.optString("hash"), it.optLong("size"), it.optString("linux"), it.optLong("seen")) }
        }.sortedByDescending { it.seen }

    /** unchanged/kept, changed, reset (it changed when Linux did): and the version from before a reset */
    fun status(cur: String, vs: List<Version>): Pair<String, Version?> {
        val i = vs.indexOfFirst { it.hash == cur }
        if (i < 0) return "not kept yet" to null
        if (i + 1 >= vs.size) return "kept" to null
        val prev = vs[i + 1]
        return if (vs[i].linux.isNotEmpty() && prev.linux.isNotEmpty() && vs[i].linux != prev.linux) "reset" to prev else "changed" to null
    }

    private fun sha256(f: File): String {
        val md = MessageDigest.getInstance("SHA-256")
        f.inputStream().use { s -> val b = ByteArray(1 shl 16); while (true) { val n = s.read(b); if (n <= 0) break; md.update(b, 0, n) } }
        return md.digest().joinToString("") { "%02x".format(it) }
    }

    private fun tell() = runCatching { onChange() }

    /** the list on the SS1 with each file's state here */
    suspend fun list(c: Context, api: PhosphorApi): JSONObject {
        val r = api.cmd(JSONObject().put("cmd", "sysbk"))
        val out = JSONObject().put("linux", r.optString("linux"))
        val items = JSONArray()
        val a = r.optJSONArray("items") ?: JSONArray()
        for (i in 0 until a.length()) {
            val it = a.getJSONObject(i)
            val id = it.optString("id")
            val vs = versions(c, id)
            val cur = if (it.isNull("hash")) "" else it.optString("hash")
            var (st, before) = status(cur, vs)
            if (!it.optBoolean("exists")) st = "not on the SS1"
            if (id == SKIP) st = "on the PC only"
            items.put(JSONObject().put("id", id).put("name", it.optString("name")).put("kind", it.optString("kind"))
                .put("exists", it.optBoolean("exists")).put("size", it.optLong("size")).put("hash", cur).put("status", st)
                .put("before", before?.let { b -> JSONObject().put("hash", b.hash).put("size", b.size).put("linux", b.linux).put("seen", b.seen) } ?: JSONObject.NULL)
                .put("versions", JSONArray(vs.map { v -> JSONObject().put("hash", v.hash).put("size", v.size).put("linux", v.linux).put("seen", v.seen) })))
        }
        return out.put("items", items)
    }

    fun state(c: Context): JSONObject = JSONObject().put("busy", busy).put("job", job).put("file", file).put("done", done).put("total", total)
        .put("error", error).put("restart", restart).put("last", Prefs.getString("sysbk_last")?.toLongOrNull() ?: 0L)

    /** fetch everything that changed (Linux itself excepted) */
    suspend fun save(c: Context, api: PhosphorApi) = withContext(Dispatchers.IO) {
        if (!lock.tryLock()) return@withContext
        try {
            busy = true; job = "save"; error = ""; tell()
            var r = api.cmd(JSONObject().put("cmd", "sysbk"))
            var tries = 0
            while (r.optBoolean("busy") && tries++ < 60) { kotlinx.coroutines.delay(2000); r = api.cmd(JSONObject().put("cmd", "sysbk")) }
            val linux = r.optString("linux")
            val a = r.optJSONArray("items") ?: JSONArray()
            for (i in 0 until a.length()) {
                val it = a.getJSONObject(i)
                val id = it.optString("id")
                if (id == SKIP || !safe(id) || !it.optBoolean("exists") || it.isNull("hash")) continue
                val hash = it.optString("hash")
                val dir = File(root(c), "$id/$hash")
                if (File(dir, "info.json").exists()) continue
                file = it.optString("name"); done = 0; total = it.optLong("size"); tell()
                val tmp = File(root(c), "$id/$hash.incomplete").apply { deleteRecursively(); mkdirs() }
                val content = File(tmp, "content")
                content.outputStream().use { o ->
                    var off = 0L
                    while (true) {
                        val p = api.cmd(JSONObject().put("cmd", "sysbk").put("get", id).put("offset", off))
                        val d = Base64.decode(p.optString("data"), Base64.DEFAULT)
                        o.write(d)
                        off += d.size
                        done = off; tell()
                        if (off >= p.optLong("size") || d.isEmpty()) {
                            if (p.optString("hash") != hash) throw PhosphorError("${it.optString("name")} changed while it was being copied: next time")
                            break
                        }
                    }
                }
                if (sha256(content) != hash) { tmp.deleteRecursively(); throw PhosphorError("${it.optString("name")} didn't arrive whole: next time") }
                File(tmp, "info.json").writeText(JSONObject().put("hash", hash).put("size", content.length()).put("linux", linux).put("seen", System.currentTimeMillis() / 1000).toString())
                tmp.renameTo(dir)
                if (it.optString("kind") == "settings") {
                    val (st, _) = status(hash, versions(c, id))
                    if (st == "reset") Notes.add(c, "sysbk-reset-$id-${hash.take(12)}", "warn", "${it.optString("name")} looks reset by an update",
                        "It changed when MiSTer Linux did. The version from before is on this phone: More > MiSTer & Linux backups.", "sysbk")
                }
            }
            Prefs.putString("sysbk_last", (System.currentTimeMillis() / 1000).toString())
        } catch (e: Exception) {
            error = e.message ?: "Something went wrong"
        } finally {
            busy = false; file = ""; lock.unlock(); tell()
        }
    }

    suspend fun restore(c: Context, api: PhosphorApi, id: String, hash: String) = withContext(Dispatchers.IO) {
        if (!safe(id) || !safe(hash)) throw PhosphorError("bad request")
        val f = File(root(c), "$id/$hash/content")
        if (!f.exists()) throw PhosphorError("That version isn't on this phone")
        if (!lock.tryLock()) throw PhosphorError("Busy: wait for the backup to finish")
        try {
            busy = true; job = "restore"; error = ""; file = id; tell()
            val data = f.readBytes()
            var off = 0
            do {
                val end = minOf(off + PIECE, data.size)
                val r = api.cmd(JSONObject().put("cmd", "sysbk").put("put", id).put("offset", off).put("size", data.size).put("hash", hash)
                    .put("data", Base64.encodeToString(data, off, end - off, Base64.NO_WRAP)), 60_000)
                done = end.toLong(); total = data.size.toLong(); tell()
                if (r.optBoolean("restart")) restart = true
                off = end
                if (r.optBoolean("done")) break
            } while (off < data.size)
        } catch (e: Exception) {
            error = e.message ?: "Something went wrong"
            throw e
        } finally {
            busy = false; file = ""; lock.unlock(); tell()
        }
    }

    /** from the watcher, while the MiSTer answers: a check every half hour */
    fun due(): Boolean = !busy && System.currentTimeMillis() / 1000 - (Prefs.getString("sysbk_last")?.toLongOrNull() ?: 0L) > 30 * 60
}
