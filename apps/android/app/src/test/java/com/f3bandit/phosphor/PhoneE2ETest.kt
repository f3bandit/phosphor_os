// End to end against a real phosphord (a PC build of it): the phone app's box art upload and its reports, from
// several lookups at once, while the app keeps asking for progress. That's more than the MiSTer serves from one
// address at a time, so some get "busy": they have to be sent again, not fail (0.27.58).
//   PHOSD_BIN=/path/to/phosphord ./gradlew testDebugUnitTest --tests '*PhoneE2ETest*'
// Skipped without PHOSD_BIN. CI (.github/workflows/tests.yml) builds phosphord for the PC and runs it.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.ByteArrayOutputStream
import java.io.File
import java.net.HttpURLConnection
import java.net.ServerSocket
import java.net.URL
import java.nio.file.Files
import java.security.MessageDigest
import java.security.SecureRandom
import java.util.zip.CRC32
import java.util.zip.Deflater
import java.util.concurrent.atomic.AtomicInteger

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class PhoneE2ETest {
    private val bin = System.getenv("PHOSD_BIN")
    private var proc: Process? = null
    private val games = 40

    @After
    fun stop() {
        proc?.destroyForcibly()?.waitFor()
    }

    /** a phosphord on a pretend SD card with `games` SNES games, remote access on, this test paired */
    private fun start(): Pair<String, String> {
        val root = Files.createTempDirectory("phos-e2e").toFile()
        val fat = File(root, "fat")
        val tmp = File(root, "tmp").apply { mkdirs() }
        val dir = File(fat, "games/SNES").apply { mkdirs() }
        File(fat, "phosphor/config").mkdirs()
        val rnd = SecureRandom()
        for (i in 1..games) File(dir, "E2E Game $i (USA).sfc").writeBytes(ByteArray(4096).also { rnd.nextBytes(it) })
        val port = ServerSocket(0).use { it.localPort }
        val token = ByteArray(32).also { rnd.nextBytes(it) }.joinToString("") { "%02x".format(it) }
        val hash = MessageDigest.getInstance("SHA-256").digest(token.toByteArray()).joinToString("") { "%02x".format(it) }
        val now = System.currentTimeMillis() / 1000
        File(fat, "phosphor/config/remote.ini").writeText("on=1\nport=$port\ndevice=$hash $now $now E2E test\n")
        val pb = ProcessBuilder(bin).directory(root).redirectErrorStream(true).redirectOutput(File(root, "phosphord.log"))
        pb.environment().putAll(mapOf("PHOS_FAT" to fat.path, "PHOS_TMP" to tmp.path, "PHOS_GAMES_ROOTS" to File(fat, "games").path,
            "PHOS_NO_REBOOT" to "1", "PHOS_CORENAME" to File(tmp, "CORENAME").path, "PHOS_MISTER_CMD" to File(tmp, "MiSTer_cmd").path))
        proc = pb.start()
        val addr = "127.0.0.1:$port"
        for (i in 0..100) {
            val up = runCatching { (URL("http://$addr/info").openConnection() as HttpURLConnection).run { responseCode.also { disconnect() } } }.getOrNull()
            if (up == 200) return addr to token
            Thread.sleep(100)
        }
        error("phosphord didn't start")
    }

    /** a picture well over 16 KB (noise doesn't compress): a busy MiSTer drops most of it unread. A plain PNG
     *  made by hand (no java.awt here) */
    private fun picture(w: Int = 300, h: Int = 420): ByteArray {
        val rnd = SecureRandom()
        val raw = ByteArrayOutputStream()
        val row = ByteArray(w * 3)
        repeat(h) { raw.write(0); rnd.nextBytes(row); raw.write(row) }
        val z = Deflater(1).run { setInput(raw.toByteArray()); finish(); val o = ByteArrayOutputStream(); val b = ByteArray(65536)
            while (!finished()) o.write(b, 0, deflate(b)); end(); o.toByteArray() }
        val out = ByteArrayOutputStream()
        fun chunk(type: String, data: ByteArray) {
            val t = type.toByteArray()
            out.write(java.nio.ByteBuffer.allocate(4).putInt(data.size).array()); out.write(t); out.write(data)
            val crc = CRC32().apply { update(t); update(data) }.value.toInt()
            out.write(java.nio.ByteBuffer.allocate(4).putInt(crc).array())
        }
        out.write(byteArrayOf(0x89.toByte(), 'P'.code.toByte(), 'N'.code.toByte(), 'G'.code.toByte(), 13, 10, 26, 10))
        chunk("IHDR", java.nio.ByteBuffer.allocate(13).putInt(w).putInt(h).put(8).put(2).put(0).put(0).put(0).array())
        chunk("IDAT", z)
        chunk("IEND", ByteArray(0))
        return out.toByteArray()
    }

    @Test
    fun lookupsAtOnceWhileAskingProgress() = runBlocking {
        assumeTrue("PHOSD_BIN not set", bin != null)
        val (addr, token) = start()
        val api = PhosphorApi(addr, token)
        api.rescan()
        withTimeout(15_000) { while (api.scrapeTodo(true, 1).optInt("total") != games) delay(100) }
        val todo = api.scrapeTodo(true, games).getJSONArray("games")
        val pic = picture()
        assertTrue("picture ${pic.size} bytes", pic.size > 64 * 1024)

        val asked = AtomicInteger()
        val next = AtomicInteger()
        withTimeout(120_000) {
            // the app's progress, twice over, all the way through
            val pollers = List(2) {
                launch(Dispatchers.IO) { while (isActive) { api.scrapeProgress(); asked.incrementAndGet(); delay(5) } }
            }
            // 6 at once: more than the phone app's 4, so with the progress questions it's well over what's served
            (1..6).map {
                async(Dispatchers.IO) {
                    while (true) {
                        val i = next.getAndIncrement()
                        if (i >= todo.length()) break
                        val g: JSONObject = todo.getJSONObject(i)
                        api.setArtJson(g.getLong("id"), pic, "image/png")
                        api.scrapeResult(g.getLong("id"), true, false, "", mapOf("developer" to "E2E Dev"), g.optString("system"))
                    }
                }
            }.awaitAll()
            pollers.forEach { it.cancel() }
        }
        println("phone e2e: $games games, progress asked ${asked.get()} times")
        assertEquals(0, api.scrapeTodo(true, 1).optInt("total"))
    }
}
