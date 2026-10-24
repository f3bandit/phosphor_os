// Phosphor for Android: line icons for the navigation (24 x 24, 1.8 stroke, round ends), drawn the way
// SS1 Tool for Android draws its own.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathParser
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

private val PATHS = mapOf(
    "home" to "M3.5 11L12 4l8.5 7M5.5 9.5V20h4.5v-5.5h4V20h4.5V9.5",
    "library" to "M4 4.5h4v15H4zM10 4.5h4v15h-4zM16 6l3.6-1 3 13.5-3.6 1z",
    "saves" to "M6 3.5h10l3.5 3.5v12a1.5 1.5 0 0 1-1.5 1.5H6a1.5 1.5 0 0 1-1.5-1.5V5A1.5 1.5 0 0 1 6 3.5zM8 3.5v5h7v-5M8 20.5v-6h8v6",
    "video" to "M5 4.5h14a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-8a2 2 0 0 1 2-2zM8 20.5h8M12 16.5v4",
    "boxart" to "M5 3.5h14a1.5 1.5 0 0 1 1.5 1.5v14a1.5 1.5 0 0 1-1.5 1.5H5A1.5 1.5 0 0 1 3.5 19V5A1.5 1.5 0 0 1 5 3.5zM3.5 16l5-5 4 4 2.5-2.5 5.5 5.5M14 8.5a1.5 1.5 0 1 0 3 0a1.5 1.5 0 1 0-3 0",
    "look" to "M12 3a9 9 0 1 0 0 18c1.4 0 2-1 2-2s-.8-1.6-.8-2.5c0-1 .8-1.8 1.8-1.8H17a4 4 0 0 0 4-4C21 6.5 17 3 12 3zM6.5 11a1 1 0 1 0 2 0a1 1 0 1 0-2 0M8.5 7a1 1 0 1 0 2 0a1 1 0 1 0-2 0M13.5 7a1 1 0 1 0 2 0a1 1 0 1 0-2 0",
    "power" to "M12 3v8M7.5 6.2a7.5 7.5 0 1 0 9 0",
    "storage" to "M5.5 6h13a1.5 1.5 0 0 1 1.5 1.5v9a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 16.5v-9A1.5 1.5 0 0 1 5.5 6zM4 13h16M16.5 15.5h.01",
    "bluetooth" to "M7 7.5l10 9-5 4.5V3l5 4.5-10 9",
    "diag" to "M2.5 12h4l2.5-6 4 12 2.5-6h6",
    "notes" to "M6 16.5V11a6 6 0 0 1 12 0v5.5l1.5 2h-15zM10 20.5a2 2 0 0 0 4 0",
    "connection" to "M12 18.5v.01M8.5 15a5 5 0 0 1 7 0M5.5 12a9 9 0 0 1 13 0M2.5 9a13 13 0 0 1 19 0",
    "about" to "M3 12a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 11v6M12 7.5v.01",
    "more" to "M6 12v.01M12 12v.01M18 12v.01",
    "back" to "M19 12H5M11 6l-6 6 6 6",
    "up" to "M6 15l6-6 6 6",
    "down" to "M6 9l6 6 6-6",
    "search" to "M10.5 4a6.5 6.5 0 1 0 0 13a6.5 6.5 0 1 0 0-13zM15.5 15.5L20 20",
    "play" to "M8 5.5v13l10.5-6.5z",
)

private val cache = HashMap<String, ImageVector>()

fun icon(name: String): ImageVector = cache.getOrPut(name) {
    val width = if (name == "more") 3.2f else 1.8f
    ImageVector.Builder(name, 24.dp, 24.dp, 24f, 24f).addPath(
        pathData = PathParser().parsePathString(PATHS[name] ?: PATHS.getValue("about")).toNodes(),
        fill = null, stroke = SolidColor(Color.White), strokeLineWidth = width,
        strokeLineCap = StrokeCap.Round, strokeLineJoin = StrokeJoin.Round,
    ).build()
}

@Composable
fun Icon(name: String, color: Color, size: Dp = 22.dp, modifier: Modifier = Modifier) =
    Image(icon(name), contentDescription = null, colorFilter = ColorFilter.tint(color), modifier = modifier.size(size))
