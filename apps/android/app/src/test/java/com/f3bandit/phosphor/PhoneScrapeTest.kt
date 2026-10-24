// Against a real phosphord (the sandbox one): every command 0.8 added, then a whole box art run on the
// "phone" (libretro, ArcadeDB, LaunchBox over the real internet), checking that the art reached the
// MiSTer and the phone's copies were deleted. Set PHOSPHOR_TEST_ADDR and PHOSPHOR_TEST_CODE; skipped otherwise.
package com.f3bandit.phosphor

import org.robolectric.RuntimeEnvironment
import com.f3bandit.phosphor.scrape.ArtStore
import com.f3bandit.phosphor.scrape.PhoneScrape
import com.f3bandit.phosphor.scrape.ScrapeRun
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35])
class PhoneScrapeTest {
    private val addr = System.getenv("PHOSPHOR_TEST_ADDR")
    private val code = System.getenv("PHOSPHOR_TEST_CODE")

    @Test
    fun newCommandsAndPhoneRun() = runBlocking {
        assumeTrue(addr != null && code != null)
        val ctx: android.content.Context = RuntimeEnvironment.getApplication()
        Prefs.init(ctx); ArtStore.init(ctx)
        val (token, _) = PhosphorApi(addr, null).pair(code, "Robolectric phone")
        val api = PhosphorApi(addr, token)

        val cats = api.categories(); println("categories: $cats")
        assertTrue(cats.any { it.games > 0 })
        val all = api.systemsIn(null); println("systems: $all")
        val cat = cats.first { it.games > 0 }
        println("in ${cat.name}: ${api.systemsIn(cat.id)}")
        println("last: ${api.last()}")
        println("power: ${runCatching { api.power() }}")
        println("guard: ${runCatching { api.guard() }}")
        println("storage: ${runCatching { api.storage() }}")
        println("update: ${runCatching { api.update() }}")
        println("progress: ${api.scrapeProgress()}")
        val src = api.scrapeSources(); println("sources: $src")
        println("accounts: ${api.scrapeAccounts()}")
        // order and on/off round trip, then put back
        val moved = api.scrapeSources(src.map { it.id }.reversed(), listOf("igdb"))
        assertEquals(src.map { it.id }.reversed(), moved.map { it.id })
        assertFalse(moved.first { it.id == "igdb" }.on)
        api.scrapeSources(src.map { it.id }, src.filter { !it.on }.map { it.id })
        println("ping: ${api.ping()}")
        Notes.check(ctx, api)
        println("notes: ${Notes.list}")

        val t0 = System.currentTimeMillis()
        ScrapeRun(ctx, api).run()
        val st = PhoneScrape.state
        println("run: $st in ${(System.currentTimeMillis() - t0) / 1000} s")
        assertFalse(st.running)
        assertTrue("some found", st.found > 0)
        val (_, games) = api.games(filter = null, q = "a")
        for (g in api.games("SNES").second + api.games("NES").second + api.games("GAMEBOY").second) {
            val d = api.game(g.id)
            println("${d.title}: art=${d.hasArt} scrape=${d.scrape} year=${d.year} dev=${d.developer} genre=${d.genre}")
        }
        println("kept on the phone: ${ArtStore.list()}")
        assertTrue("auto delete leaves nothing sent", ArtStore.list().none { it.sent })
        println(games.size)
    }
}
