// The LaunchBox source on its own, over the real internet: downloads Metadata.zip (~110 MB), reads it into
// the cache database, and looks up a few games by official name. Slow: set PHOSPHOR_TEST_LAUNCHBOX=1 to run.
package com.f3bandit.phosphor

import com.f3bandit.phosphor.scrape.LaunchBox
import com.f3bandit.phosphor.scrape.SGame
import com.f3bandit.phosphor.scrape.SRes
import com.f3bandit.phosphor.scrape.ScrapeRun
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class LaunchBoxTest {
    @Test
    fun lookups() = runBlocking {
        assumeTrue(System.getenv("PHOSPHOR_TEST_LAUNCHBOX") == "1")
        val ctx: android.content.Context = RuntimeEnvironment.getApplication()
        val dir = File(System.getenv("PHOSPHOR_TEST_LAUNCHBOX_DIR") ?: ctx.cacheDir.path)
        val lb = LaunchBox(dir)
        val run = ScrapeRun(ctx, PhosphorApi("127.0.0.1:1", null))
        fun game(sys: String, official: String) = SGame(1, sys, sys, official, "", "", 0, false, sys == "Arcade", false, emptyList()).also { it.official = official }
        val t0 = System.currentTimeMillis()
        for ((sys, name) in listOf("SNES" to "Super Mario World (USA)", "PSX" to "Final Fantasy Tactics (USA)", "Genesis" to "Sonic the Hedgehog (USA, Europe)",
            "NES" to "Legend of Zelda, The (USA)", "Arcade" to "Street Fighter II: The World Warrior")) {
            val r = SRes()
            val rc = lb.lookup(run, game(sys, name), r)
            println("$name -> rc=$rc year=${r.f[4]} dev=${r.f[1]} genre=${r.f[3]} players=${r.f[5]} desc=${r.f[0]?.take(60)} art=${r.art}")
            if (sys != "Arcade") assertEquals(1, rc)
        }
        println("in ${(System.currentTimeMillis() - t0) / 1000} s, database ${lb.size() / 1048576} MB")
        assertTrue(lb.size() > 0)
    }
}
