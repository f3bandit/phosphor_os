// The phone's name handling against Phosphor for Windows' (scrape_text.go): PHOSPHOR_TEST_GOTEXT names a JSON
// file the Go functions wrote for the same inputs (see the comment in it); skipped otherwise.
package com.f3bandit.phosphor

import com.f3bandit.phosphor.scrape.discNo
import com.f3bandit.phosphor.scrape.nameBase
import com.f3bandit.phosphor.scrape.nameKey
import com.f3bandit.phosphor.scrape.netEscape
import com.f3bandit.phosphor.scrape.regionOf
import com.f3bandit.phosphor.scrape.serialNorm
import com.f3bandit.phosphor.scrape.thumbName
import com.f3bandit.phosphor.scrape.titleNorm
import com.f3bandit.phosphor.scrape.trimField
import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TextTest {
    @Test
    fun sameAsWindows() {
        val f = System.getenv("PHOSPHOR_TEST_GOTEXT")
        assumeTrue(f != null)
        val a = JSONArray(File(f!!).readText())
        for (i in 0 until a.length()) {
            val o = a.getJSONObject(i)
            val s = o.getString("in")
            assertEquals(s, o.getString("titleNorm"), titleNorm(s))
            assertEquals(s, o.getString("nameBase"), nameBase(s))
            assertEquals(s, o.getString("serialNorm"), serialNorm(s))
            assertEquals(s, o.getString("nameKey"), nameKey(s))
            assertEquals(s, o.getString("regionOf"), regionOf(s))
            assertEquals(s, o.getInt("discNo"), discNo(s))
            assertEquals(s, o.getString("thumbName"), thumbName(s))
            assertEquals(s, o.getString("netEscape"), netEscape(s))
            assertEquals(s, o.getString("trimField"), trimField(s))
        }
        println("${a.length()} names: the same as Phosphor for Windows")
    }
}

/** The phone's box art against Phosphor for Windows' for the same picture (PHOSPHOR_TEST_GOIMG: a folder with
 *  alpha.png and the Go program's go_alpha.jpg): same size, and the same pixels within JPEG noise. */
@RunWith(RobolectricTestRunner::class)
@org.robolectric.annotation.GraphicsMode(org.robolectric.annotation.GraphicsMode.Mode.NATIVE)
@Config(sdk = [35])
class BoxArtTest {
    @Test
    fun sameAsWindows() {
        val d = System.getenv("PHOSPHOR_TEST_GOIMG")
        assumeTrue(d != null)
        val mine = com.f3bandit.phosphor.scrape.makeBoxArt(File(d, "alpha.png").readBytes())!!
        File(d, "phone_alpha.jpg").writeBytes(mine)
        val a = android.graphics.BitmapFactory.decodeByteArray(mine, 0, mine.size)
        val g = File(d, "go_alpha.jpg").readBytes().let { android.graphics.BitmapFactory.decodeByteArray(it, 0, it.size) }
        assertEquals(g.width, a.width); assertEquals(g.height, a.height)
        var sum = 0L; var worst = 0
        for (y in 0 until a.height) for (x in 0 until a.width) {
            val p = a.getPixel(x, y); val q = g.getPixel(x, y)
            for (s in intArrayOf(16, 8, 0)) { val e = kotlin.math.abs(((p shr s) and 255) - ((q shr s) and 255)); sum += e; if (e > worst) worst = e }
        }
        val mean = sum.toDouble() / (a.width * a.height * 3)
        println("box art ${a.width}x${a.height}: mean difference $mean, worst $worst (of 255)")
        org.junit.Assert.assertTrue("mean $mean", mean < 3.0)
    }
}
