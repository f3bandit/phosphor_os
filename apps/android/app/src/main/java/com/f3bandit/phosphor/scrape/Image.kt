// Phosphor for Android, box art on the phone: the picture made here, the way the MiSTer stores it
// (longest side 512 px, averaged down, see-through edges on the shell's dark panel colour, JPEG), so the
// MiSTer only has to save it. The same as Phosphor for Windows' scrape_image.go.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.ByteArrayOutputStream

private const val ART_BOX_MAX = 512

/** the finished JPEG, or null when the data isn't a picture (or is too big for box art) */
fun makeBoxArt(data: ByteArray): ByteArray? {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeByteArray(data, 0, data.size, bounds)
    val w0 = bounds.outWidth; val h0 = bounds.outHeight
    if (w0 < 1 || h0 < 1 || w0.toLong() * h0 > 5000L * 5000) return null
    // very big pictures are read at half (or a quarter) first: still well over 512 px, so the result is the same
    var sample = 1
    while (maxOf(w0, h0) / (sample * 2) >= 2048) sample *= 2
    val src = BitmapFactory.decodeByteArray(data, 0, data.size, BitmapFactory.Options().apply {
        inSampleSize = sample; inPreferredConfig = Bitmap.Config.ARGB_8888; inPremultiplied = false
    }) ?: return null
    val w = src.width; val h = src.height
    var nw = w; var nh = h
    if (w > ART_BOX_MAX || h > ART_BOX_MAX) {
        if (w >= h) { nw = ART_BOX_MAX; nh = h * ART_BOX_MAX / w } else { nh = ART_BOX_MAX; nw = w * ART_BOX_MAX / h }
        if (nw < 1) nw = 1
        if (nh < 1) nh = 1
    }
    val px = IntArray(w * h)
    src.getPixels(px, 0, w, 0, 0, w, h)   // not premultiplied: the colour as it is, and its alpha
    src.recycle()
    val out = IntArray(nw * nh)
    val bg = intArrayOf(24, 24, 28)
    for (y in 0 until nh) {
        val y0 = y * h / nh; var y1 = (y + 1) * h / nh
        if (y1 <= y0) y1 = y0 + 1
        for (x in 0 until nw) {
            val x0 = x * w / nw; var x1 = (x + 1) * w / nw
            if (x1 <= x0) x1 = x0 + 1
            var sr = 0; var sg = 0; var sb = 0; var sa = 0; var n = 0
            for (yy in y0 until y1) {
                var o = yy * w + x0
                for (xx in x0 until x1) {
                    val c = px[o++]
                    sr += (c shr 16) and 0xFF; sg += (c shr 8) and 0xFF; sb += c and 0xFF; sa += (c ushr 24)
                    n++
                }
            }
            val a = sa / n
            val r = (sr / n * a + bg[0] * (255 - a)) / 255
            val g = (sg / n * a + bg[1] * (255 - a)) / 255
            val b = (sb / n * a + bg[2] * (255 - a)) / 255
            out[y * nw + x] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
        }
    }
    val bmp = Bitmap.createBitmap(out, nw, nh, Bitmap.Config.ARGB_8888)
    val buf = ByteArrayOutputStream()
    bmp.compress(Bitmap.CompressFormat.JPEG, 92, buf)
    bmp.recycle()
    return buf.toByteArray()
}
