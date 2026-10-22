// Against a real phosphord (the sandbox one, or a MiSTer): set PHOSPHOR_TEST_ADDR (host:port),
// PHOSPHOR_TEST_CODE (a pairing code that's open now) and PHOSPHOR_TEST_PNG (any image file). Skipped otherwise.
package com.f3bandit.phosphor

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ApiTest {
    private val addr = System.getenv("PHOSPHOR_TEST_ADDR")
    private val code = System.getenv("PHOSPHOR_TEST_CODE")
    private val png = System.getenv("PHOSPHOR_TEST_PNG")

    @Test
    fun fullRound() = runBlocking {
        assumeTrue(addr != null && code != null && png != null)
        assertNotNull(PhosphorApi.info(addr))
        val bad = runCatching { PhosphorApi(addr, null).pair("000000", "x") }.exceptionOrNull() as PhosphorError
        assertEquals(401, bad.code)
        val (token, _) = PhosphorApi(addr, null).pair(code, "Robolectric")
        val api = PhosphorApi(addr, token)

        val st = api.status()
        assertTrue(st.games > 0)
        val systems = api.systems()
        assertTrue(systems.isNotEmpty())
        val (total, games) = api.games(systems[0].id)
        assertEquals(total, games.size)
        val g = api.game(games[0].id)
        println("status=$st systems=${systems.size} first=${g.title} files=${g.files.size}")

        val edited = api.edit(g.id, mapOf("genre" to "Test genre"))
        assertEquals("Test genre", edited.genre)
        api.edit(g.id, mapOf("genre" to (g.genre ?: "")))

        val withArt = api.setArt(g.id, java.io.File(png).readBytes(), "image/png")
        assertTrue(withArt.hasArt)
        val jpg = api.art(g.id)
        assertNotNull(jpg)
        assertTrue(jpg!![0] == 0xFF.toByte() && jpg[1] == 0xD8.toByte())

        val missing = api.games(filter = "missing_art").second
        println("missing art: ${missing.map { it.title }}")
        val cands = api.candidates(g.id, null)
        println("candidates: ${cands.size}")

        val cl = api.cloud()
        println("cloud: installed=${cl.installed} ready=${cl.ready} remote=${cl.remote} places=${cl.places}")

        val bk = api.backup()
        println("backups: installed=${bk.installed} count=${bk.snapshots.size} dest=${bk.dest}")

        val refused = runCatching { api.cmd(JSONObject().put("cmd", "wifi")) }.exceptionOrNull()
        assertTrue(refused is PhosphorError)
        println("wifi refused: ${refused?.message}")
    }
}
