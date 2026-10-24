// Phosphor for Android, box art on the phone: downloads, with the same answers phosphord's net.c gives
// (0 ok, 1 not there, 2 refused, -1 trouble), and a gentle pace per service.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

const val SCRAPE_UA = "Phosphor/0.4 (MiSTer frontend; Phosphor for Android)"

/** the body (null on trouble), the result code, the HTTP status */
class NetResult(val body: ByteArray?, val rc: Int, val http: Int)

fun httpResult(code: Int) = when {
    code in 200..299 -> 0
    code == 404 -> 1
    code == 401 || code == 403 -> 2
    else -> -1
}

suspend fun netRequest(method: String, url: String, headers: Map<String, String> = emptyMap(), body: ByteArray? = null, max: Long = 8L shl 20): NetResult =
    withContext(Dispatchers.IO) {
        val c = try { URL(url).openConnection() as HttpURLConnection } catch (e: Exception) { return@withContext NetResult(null, -1, 0) }
        try {
            c.requestMethod = method
            c.connectTimeout = 15_000
            c.readTimeout = 60_000
            c.instanceFollowRedirects = true
            c.setRequestProperty("User-Agent", SCRAPE_UA)
            headers.forEach { (k, v) -> c.setRequestProperty(k, v) }
            if (body != null) {
                c.doOutput = true
                c.setFixedLengthStreamingMode(body.size)
                c.outputStream.use { it.write(body) }
            }
            val code = c.responseCode
            val stream = if (code in 200..299) c.inputStream else c.errorStream
            val out = ByteArrayOutputStream()
            if (stream != null) stream.use { s ->
                val buf = ByteArray(64 shl 10)
                var total = 0L
                while (true) {
                    ensureActive()
                    val n = s.read(buf)
                    if (n < 0) break
                    total += n
                    if (total > max) break
                    out.write(buf, 0, n)
                }
            }
            NetResult(out.toByteArray(), httpResult(code), code)
        } catch (e: IOException) {
            NetResult(null, -1, 0)
        } finally {
            c.disconnect()
        }
    }

/** a download into a file (through .part); 0 ok, 1 not there, -1 trouble */
suspend fun netGetFile(url: String, dest: File, progress: (Long, Long) -> Unit): Int = withContext(Dispatchers.IO) {
    val c = try { URL(url).openConnection() as HttpURLConnection } catch (e: Exception) { return@withContext -1 }
    val part = File(dest.path + ".part")
    try {
        c.connectTimeout = 15_000
        c.readTimeout = 60_000
        c.setRequestProperty("User-Agent", SCRAPE_UA)
        val res = httpResult(c.responseCode)
        if (res != 0) return@withContext res
        val total = c.contentLengthLong
        var done = 0L
        part.outputStream().use { out ->
            c.inputStream.use { s ->
                val buf = ByteArray(256 shl 10)
                while (true) {
                    ensureActive()
                    val n = s.read(buf)
                    if (n < 0) break
                    out.write(buf, 0, n)
                    done += n
                    progress(done, total)
                }
            }
        }
        if (!part.renameTo(dest)) -1 else 0
    } catch (e: IOException) {
        part.delete()
        -1
    } finally {
        c.disconnect()
    }
}

/** at most one request every gap, for services that ask for it (ArcadeDB, IGDB: 4 a second) */
class Pace(private val gapMs: Long) {
    private val mu = Mutex()
    private var next = 0L
    suspend fun await() {
        val at = mu.withLock {
            val now = System.currentTimeMillis()
            val a = maxOf(next, now)
            next = a + gapMs
            a
        }
        val d = at - System.currentTimeMillis()
        if (d > 0) delay(d)
    }
}
