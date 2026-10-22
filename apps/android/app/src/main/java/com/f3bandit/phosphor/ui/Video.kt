// Phosphor for Android: screen filters per system (the MiSTer's own filter folders; backed up before each change).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.f3bandit.phosphor.VideoSys

/** a slot on a system's page: what it's called, the API name, the file kind for its picker */
data class VideoSlot(val label: String, val what: String, val kind: String)

val VIDEO_SLOTS = listOf(
    VideoSlot("Video filter", "filter", "filters"), VideoSlot("Vertical filter", "vertical", "filters"),
    VideoSlot("Scanlines", "scanlines", "filters"), VideoSlot("Interlaced", "interlace", "filters"),
    VideoSlot("Shadow mask", "mask", "masks"), VideoSlot("Mask size", "maskmode", ""), VideoSlot("Gamma", "gamma", "gamma"),
)

fun VideoSys.value(what: String): String? = when (what) {
    "filter" -> filter; "vertical" -> vertical; "scanlines" -> scanlines; "interlace" -> interlace
    "mask" -> mask; "maskmode" -> maskmode; "gamma" -> gamma; else -> null
}

private fun short(p: String?) = p?.substringAfterLast('/')?.substringBeforeLast('.') ?: "Off"

@Composable
fun VideoListContent(list: List<VideoSys>?, error: String?, onPick: (VideoSys) -> Unit, onBack: () -> Unit) {
    Screen("Settings", "Screen filters", onBack = onBack) {
        LazyColumn(contentPadding = PaddingValues(horizontal = 20.dp, vertical = 4.dp), verticalArrangement = Gap) {
            if (error != null) item { Text(error, style = T.small.copy(color = P.bad)) }
            if (list == null) { if (error == null) item { Loading() }; return@LazyColumn }
            item { Text("Per system, from the MiSTer's own Filters, Shadow_Masks and Gamma folders. A change shows the next time that core starts.", style = T.small) }
            item {
                Card {
                    if (list.isEmpty()) Row2("No systems with a core installed", dim = true)
                    list.forEach { v -> Row2(v.name, value = if (v.custom) "${short(v.filter)} ›" else "MiSTer defaults ›", dim = !v.custom, onClick = { onPick(v) }) }
                }
            }
            item { Spacer(Modifier.height(20.dp)) }
        }
    }
}

/** a system's page, or (picking != null) the list of choices for one slot */
@Composable
fun VideoSysContent(
    v: VideoSys?, error: String?, picking: VideoSlot?, choices: List<String>?,
    onSlot: (VideoSlot) -> Unit, onPresets: () -> Unit, onChoose: (String?) -> Unit, onUndo: () -> Unit, onBack: () -> Unit,
) {
    Screen(v?.name ?: "Screen filters", picking?.label ?: "Screen filters", onBack = onBack) {
        if (picking != null) {
            LazyColumn(contentPadding = PaddingValues(horizontal = 20.dp, vertical = 4.dp), verticalArrangement = Gap) {
                if (choices == null) { item { Loading() }; return@LazyColumn }
                item {
                    Card {
                        if (picking.what != "preset") Row2(if (picking.what == "maskmode") "off" else "Off", accent = v?.value(picking.what) == null || v.value(picking.what) == "off", onClick = { onChoose(null) })
                        choices.forEach { c -> Row2(if (picking.what == "maskmode") c else short(c), sub = if (picking.what == "maskmode") null else c.substringBeforeLast('/', "").ifEmpty { null },
                            accent = v?.value(picking.what) == c, onClick = { onChoose(c) }) }
                    }
                }
                item { Spacer(Modifier.height(20.dp)) }
            }
            return@Screen
        }
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (v == null) { Loading(); return@Column }
            Card { VIDEO_SLOTS.forEach { s -> Row2(s.label, value = if (s.what == "maskmode") v.maskmode else short(v.value(s.what)), onClick = { onSlot(s) }) } }
            Card { Row2("Apply a preset", value = "›", onClick = onPresets) }
            Button("Undo the last change", Modifier.fillMaxWidth(), onClick = onUndo)
            Text("Every change is backed up first on the MiSTer. It shows the next time ${v.name}'s core starts.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
