// Phosphor for Android: the console's background textures (frontend/src/texture.c, and Phosphor for
// Windows' texture.js), drawn the same way: one tile from distance fields, in the accent colour at the
// same faint strength, repeated behind the screens. The numbers are the console's, so a texture picked
// here is the one picked on the TV. The circuit board is one whole picture (assets/circuit.json).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.ImageShader
import androidx.compose.ui.graphics.ShaderBrush
import androidx.compose.ui.graphics.TileMode
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import org.json.JSONObject
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

object Texture {
    private const val ZIGZAG = 0; private const val X = 1; private const val SQUARE = 2; private const val TRIANGLE = 3
    private const val HEX = 4; private const val TRIGRID = 5; private const val DIAMOND = 6; private const val DIAGRID = 7
    private const val GRID = 8; private const val BOARD = 9
    private const val LINE = 0; private const val FILL = 1; private const val INVERT = 2
    private const val STRENGTH = 0.13f

    private class T(val name: String, val sh: Int, val w: Float, val h: Float, val lw: Float, val mode: Int)

    private val LIST: Map<Int, T> = mapOf(
        1 to T("Zigzag", ZIGZAG, 16f, 16f, 1f, LINE),
        2 to T("X's", X, 40f, 40f, 1f, LINE),
        3 to T("Squares", SQUARE, 40f, 40f, 1f, LINE),
        4 to T("Triangles", TRIANGLE, 40f, 36f, 1f, LINE),
        6 to T("Hexagons", HEX, 25.980762f, 45f, 1f, LINE),
        8 to T("Zigzag, tight", ZIGZAG, 16f, 8f, 1f, LINE),
        9 to T("Zigzag, bold", ZIGZAG, 16f, 16f, 2.4f, LINE),
        10 to T("X's, packed", X, 14f, 14f, 1f, LINE),
        11 to T("Squares, packed", SQUARE, 17f, 17f, 1f, LINE),
        12 to T("Squares, filled", SQUARE, 26f, 26f, 1f, FILL),
        13 to T("Triangles, packed", TRIGRID, 20f, 34.641016f, 1f, LINE),
        14 to T("Triangles, bold", TRIANGLE, 40f, 36f, 2.4f, LINE),
        15 to T("Hexagons, bold", HEX, 25.980762f, 45f, 2.4f, LINE),
        16 to T("Honeycomb, inverted", HEX, 25.980762f, 45f, 2.0f, INVERT),
        17 to T("Squares, inverted", SQUARE, 17f, 17f, 1.6f, INVERT),
        18 to T("Diamonds", DIAMOND, 40f, 40f, 1f, LINE),
        19 to T("Diamonds, packed", DIAMOND, 20f, 20f, 1f, LINE),
        20 to T("Diamonds, filled", DIAMOND, 24f, 24f, 1f, FILL),
        21 to T("Diamond lattice", DIAGRID, 20f, 20f, 1f, LINE),
        22 to T("Square grid", GRID, 20f, 20f, 1f, LINE),
        23 to T("Square grid, bold", GRID, 20f, 20f, 2.4f, LINE),
        24 to T("Circuit board", BOARD, 1280f, 720f, 1f, LINE),
        25 to T("Circuit board, bold", BOARD, 1280f, 720f, 2f, LINE),
    )

    /** the order the console and Phosphor for Windows list them in */
    val ORDER = listOf(0, 1, 8, 9, 2, 10, 3, 11, 12, 17, 22, 23, 18, 19, 20, 21, 4, 13, 14, 6, 15, 16, 24, 25)

    fun name(n: Int) = LIST[n]?.name ?: "None"
    fun full(n: Int) = LIST[n]?.sh == BOARD

    private fun seg(px: Float, py: Float, ax: Float, ay: Float, bx: Float, by: Float): Float {
        val vx = bx - ax; val vy = by - ay; val wx = px - ax; val wy = py - ay; val l2 = vx * vx + vy * vy
        val t = (if (l2 > 0) (wx * vx + wy * vy) / l2 else 0f).coerceIn(0f, 1f)
        val dx = wx - t * vx; val dy = wy - t * vy
        return sqrt(dx * dx + dy * dy)
    }

    private val hexX = FloatArray(6) { cos(PI / 180 * (60 * it - 30)).toFloat() }
    private val hexY = FloatArray(6) { sin(PI / 180 * (60 * it - 30)).toFloat() }

    /** distance from (x, y) to the shape's lines; inside[0] says whether it's inside a filled shape */
    private fun shape(s: Int, x0: Float, y0: Float, w: Float, h: Float, inside: BooleanArray): Float {
        var x = x0; var y = y0
        val cx = w / 2; val cy = h / 2
        inside[0] = false
        when (s) {
            ZIGZAG -> { val m = h / 2; val a = min(3f, h / 3); return min(seg(x, y, 0f, m + a, w / 2, m - a), seg(x, y, w / 2, m - a, w, m + a)) }
            X -> return min(seg(x, y, cx - 5, cy - 5, cx + 5, cy + 5), seg(x, y, cx + 5, cy - 5, cx - 5, cy + 5))
            SQUARE -> { val m = max(abs(x - cx), abs(y - cy)); val r = 7f; inside[0] = m < r; return abs(m - r) }
            TRIANGLE -> {
                val r = 9f; val ax = cx; val ay = cy - r; val bx = cx - r * 0.866f; val by = cy + r * 0.5f; val qx = cx + r * 0.866f; val qy = by
                return min(seg(x, y, ax, ay, bx, by), min(seg(x, y, bx, by, qx, qy), seg(x, y, qx, qy, ax, ay)))
            }
            HEX -> {
                val r = 15f; val ww = 1.7320508f * r; val hh = 3 * r; var best = 1e9f
                for (j in -1..2) for (i in -1..2) {
                    val hx = i * ww + (if (j and 1 != 0) ww / 2 else 0f); val hy = j * hh / 2
                    for (k in 0 until 6) {
                        val k2 = (k + 1) % 6
                        best = min(best, seg(x, y, hx + r * hexX[k], hy + r * hexY[k], hx + r * hexX[k2], hy + r * hexY[k2]))
                    }
                }
                return best
            }
            TRIGRID -> {
                var d = min(abs(y), min(abs(y - h), abs(y - h / 2))); val s3 = 0.8660254f
                for (k in -2..2) { val ox = k * w; d = min(d, min(abs(s3 * (x - ox) + 0.5f * (y - h)), abs(s3 * (x - ox) - 0.5f * (y - h)))) }
                return d
            }
            DIAMOND -> { val m = abs(x - cx) + abs(y - cy); val r = min(9f, min(w, h) / 2 - 1.5f); inside[0] = m < r; return abs(m - r) * 0.70710678f }
            DIAGRID -> return 0.70710678f * min(abs((x + y + 4 * w) % w - w / 2), abs((x - y + 4 * w) % w - w / 2))
            GRID -> { x = (x + 4 * w) % w; y = (y + 4 * h) % h; return min(min(x, w - x), min(y, h - y)) }
        }
        return 1e9f
    }

    /** one tile in colour c, at scale screen pixels per console pixel (see-through: drawn over the background) */
    fun tile(n: Int, c: Color, scale: Float): ImageBitmap? {
        val t = LIST[n] ?: return null
        if (t.sh == BOARD) return null
        val pw = (t.w * scale).roundToInt(); val ph = (t.h * scale).roundToInt()
        if (pw < 4 || ph < 4) return null
        val rgb = c.toArgb() and 0xFFFFFF
        val px = IntArray(pw * ph)
        val half = 0.55f * scale * t.lw
        val strength = if (t.mode == LINE) STRENGTH else STRENGTH * 0.75f
        val inside = BooleanArray(1)
        for (y in 0 until ph) for (x in 0 until pw) {
            val fx = (x + 0.5f) * t.w / pw; val fy = (y + 0.5f) * t.h / ph
            var d = 1e9f; var anyIn = false
            for (oy in -1..1) for (ox in -1..1) {
                val dd = shape(t.sh, fx + ox * t.w, fy + oy * t.h, t.w, t.h, inside)
                if (dd < d) d = dd
                if (inside[0] && ox == 0 && oy == 0) anyIn = true
            }
            d *= pw / t.w
            val line = (half + 0.5f - d).coerceIn(0f, 1f)
            val a = when (t.mode) { FILL -> if (anyIn) 1f else line; INVERT -> 1 - line; else -> line }
            px[y * pw + x] = ((a * strength * 255).roundToInt() shl 24) or rgb
        }
        return Bitmap.createBitmap(px, pw, ph, Bitmap.Config.ARGB_8888).asImageBitmap()
    }

    private var circuit: JSONObject? = null

    /** the circuit board, a whole picture laid on the background colour, at scale */
    fun board(context: Context, n: Int, c: Color, bg: Color, scale: Float): ImageBitmap? {
        val t = LIST[n] ?: return null
        val cj = circuit ?: runCatching { JSONObject(context.assets.open("circuit.json").use { String(it.readBytes()) }) }.getOrNull()?.also { circuit = it } ?: return null
        val pts = cj.getJSONArray("pts"); val traces = cj.getJSONArray("traces"); val vias = cj.getJSONArray("vias")
        val cell = cj.optInt("cell", 8).toFloat(); val half = cell / 2
        val pw = (1280 * scale).roundToInt().coerceAtLeast(1); val ph = (720 * scale).roundToInt().coerceAtLeast(1)
        val bmp = Bitmap.createBitmap(pw, ph, Bitmap.Config.ARGB_8888)
        val g = Canvas(bmp)
        fun p(v: Int) = (v * cell + half) * scale
        val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE; color = android.graphics.Color.WHITE; strokeWidth = 1.4f * t.lw * scale
            strokeJoin = Paint.Join.ROUND; strokeCap = Paint.Cap.ROUND
        }
        val path = Path()
        var i = 0
        while (i + 1 < traces.length()) {
            val a = traces.getInt(i); val cnt = traces.getInt(i + 1)
            for (k in a until a + cnt) {
                val x = p(pts.getInt(k * 2)); val y = p(pts.getInt(k * 2 + 1))
                if (k == a) path.moveTo(x, y) else path.lineTo(x, y)
            }
            i += 2
        }
        g.drawPath(path, stroke)
        val ro = (2.8f + (t.lw - 1) * 0.6f) * scale; val rh = 1.5f * scale
        val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = android.graphics.Color.WHITE }
        val clear = Paint(Paint.ANTI_ALIAS_FLAG).apply { xfermode = android.graphics.PorterDuffXfermode(android.graphics.PorterDuff.Mode.CLEAR) }
        i = 0
        while (i + 1 < vias.length()) {
            val x = p(vias.getInt(i)); val y = p(vias.getInt(i + 1))
            g.drawCircle(x, y, ro, fill)
            g.drawCircle(x, y, rh, clear)
            i += 2
        }
        // laid on the background colour: a picture this faint loses its colour when left see-through
        val px = IntArray(pw * ph)
        bmp.getPixels(px, 0, pw, 0, 0, pw, ph)
        val br = (bg.red * 255).roundToInt(); val bgg = (bg.green * 255).roundToInt(); val bb = (bg.blue * 255).roundToInt()
        val cr = (c.red * 255).roundToInt(); val cg = (c.green * 255).roundToInt(); val cb = (c.blue * 255).roundToInt()
        for (k in px.indices) {
            val a = (px[k] ushr 24) / 255f * STRENGTH
            val r = (br * (1 - a) + cr * a).roundToInt(); val gg = (bgg * (1 - a) + cg * a).roundToInt(); val b = (bb * (1 - a) + cb * a).roundToInt()
            px[k] = (0xFF shl 24) or (r shl 16) or (gg shl 8) or b
        }
        bmp.setPixels(px, 0, pw, 0, 0, pw, ph)
        return bmp.asImageBitmap()
    }
}

/** the screens' background: Phosphor's dark colour with the chosen texture in the accent colour */
@Composable
fun TextureBackground(texture: Int, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    val ctx = LocalContext.current
    val density = LocalDensity.current.density
    val acc = P.acc
    BoxWithConstraints(modifier.background(P.bg)) {
        if (Texture.full(texture)) {
            val wpx = constraints.maxWidth.toFloat(); val hpx = constraints.maxHeight.toFloat()
            // the board is drawn at about screen size (capped, it's one big picture) and scaled to cover
            val scale = (max(wpx / 1280f, hpx / 720f)).coerceAtMost(2.5f)
            val img = remember(texture, acc, scale) { Texture.board(ctx, texture, acc, P.bg, scale) }
            if (img != null) Image(img, contentDescription = null, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
        } else {
            val tile = remember(texture, acc, density) { Texture.tile(texture, acc, density) }
            if (tile != null) Box(Modifier.fillMaxSize().background(ShaderBrush(ImageShader(tile, TileMode.Repeated, TileMode.Repeated))))
        }
        content()
    }
}
