// Phosphor for Android: the same look as the MiSTer shell (dark, amber accent, Inter + mono labels).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.f3bandit.phosphor.R

/** the accent colours, the same five as the MiSTer shell and Phosphor for Windows */
val ACCENTS = linkedMapOf(
    "amber" to Color(0xFFFFB347), "red" to Color(0xFFFF6B5E), "green" to Color(0xFF5CF08A),
    "blue" to Color(0xFF6CB6FF), "purple" to Color(0xFFC792FF),
)

object P {
    val bg = Color(0xFF0E0E10)
    val bar = Color(0xFF141417)          // app bar, bottom bar and rail (SS1 Tool's --ab, in Phosphor's greys)
    val panel = Color(0xFF17171B)
    val panel2 = Color(0xFF212127)
    val line = Color(0xFF2A2A31)
    val text = Color(0xFFECE8E1)
    val dim = Color(0xFF8D8A85)
    val faint = Color(0xFF5C5A57)
    /** the chosen accent (Settings > Look); amber unless changed */
    var accent by mutableStateOf(ACCENTS.getValue("amber"))
    val acc: Color get() = accent
    val panelOn: Color get() = lerp(Color(0xFF141416), accent, 0.08f)
    /** the selected item's pill in the bottom bar and rail */
    val navOn: Color get() = lerp(bar, accent, 0.22f)
    val bad = Color(0xFFFF6B5E)
    val warn = Color(0xFFFFC857)
    val good = Color(0xFF5CF08A)
}

val Inter = FontFamily(
    Font(R.font.inter_regular, FontWeight.Normal),
    Font(R.font.inter_medium, FontWeight.Medium),
    Font(R.font.inter_semibold, FontWeight.SemiBold),
)
val Mono = FontFamily(Font(R.font.dejavu_mono))

object T {
    val heading = TextStyle(fontFamily = Inter, fontWeight = FontWeight.SemiBold, fontSize = 26.sp, color = P.text)
    val title = TextStyle(fontFamily = Inter, fontWeight = FontWeight.SemiBold, fontSize = 19.sp, color = P.text)
    val name = TextStyle(fontFamily = Inter, fontWeight = FontWeight.Medium, fontSize = 16.sp, color = P.text)
    val body = TextStyle(fontFamily = Inter, fontSize = 15.sp, lineHeight = 22.sp, color = P.text)
    val small = TextStyle(fontFamily = Inter, fontSize = 13.sp, lineHeight = 18.sp, color = P.dim)
    val label = TextStyle(fontFamily = Mono, fontSize = 11.sp, letterSpacing = 2.5.sp, color = P.dim)
    val word: TextStyle get() = TextStyle(fontFamily = Mono, fontSize = 17.sp, letterSpacing = 3.sp, color = P.acc)
    val bar = TextStyle(fontFamily = Inter, fontWeight = FontWeight.Medium, fontSize = 20.sp, color = P.text)
}

@Composable
fun PhosphorTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = P.acc, onPrimary = P.bg, background = P.bg, surface = P.panel, onSurface = P.text,
            onBackground = P.text, surfaceVariant = P.panel, onSurfaceVariant = P.dim, outline = P.line, error = P.bad,
        ),
        content = content,
    )
}

/** spaced capitals above a heading: SETTINGS, LIBRARY */
@Composable
fun Label(text: String, modifier: Modifier = Modifier, color: Color = P.dim) =
    Text(text.uppercase(), style = T.label.copy(color = color), modifier = modifier)

/** a row like the shell's lists: name on the left, value on the right, amber bar when it matters */
@Composable
fun Row2(
    title: String, value: String? = null, sub: String? = null, dim: Boolean = false, accent: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .padding(horizontal = 16.dp, vertical = 13.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (accent) {
            Box(Modifier.width(3.dp).height(22.dp).clip(RoundedCornerShape(2.dp)).background(P.acc))
            Spacer(Modifier.width(12.dp))
        }
        Column(Modifier.weight(1f)) {
            Text(title, style = T.name.copy(color = if (dim) P.dim else P.text), maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (sub != null) Text(sub, style = T.small, maxLines = 3, overflow = TextOverflow.Ellipsis)
        }
        if (value != null) {
            Spacer(Modifier.width(12.dp))
            Text(value, style = T.small.copy(color = if (accent) P.acc else P.dim), maxLines = 1)
        }
    }
}

@Composable
fun Card(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Column(
        modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(P.panel).padding(vertical = 6.dp),
    ) { content() }
}

/** small outlined tag: "no art", "linked" */
@Composable
fun Tag(text: String, color: Color = P.dim) {
    Box(
        Modifier.border(1.dp, color.copy(alpha = 0.5f), RoundedCornerShape(6.dp)).padding(horizontal = 6.dp, vertical = 1.dp),
    ) { Text(text, style = T.small.copy(fontSize = 11.sp, color = color)) }
}

@Composable
fun Pill(text: String, on: Boolean, onClick: () -> Unit) {
    Box(
        Modifier
            .clip(RoundedCornerShape(20.dp))
            .background(if (on) P.acc else P.panel)
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 7.dp),
    ) { Text(text, style = T.small.copy(color = if (on) P.bg else P.text, fontWeight = FontWeight.Medium)) }
}

@Composable
fun Button(text: String, modifier: Modifier = Modifier, primary: Boolean = false, enabled: Boolean = true, onClick: () -> Unit) {
    Box(
        modifier
            .clip(RoundedCornerShape(12.dp))
            .background(if (primary && enabled) P.acc else P.panel)
            .border(1.dp, if (primary) Color.Transparent else P.line, RoundedCornerShape(12.dp))
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 18.dp, vertical = 12.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(text, style = T.name.copy(fontSize = 15.sp, color = if (primary && enabled) P.bg else if (enabled) P.text else P.faint))
    }
}

@Composable
fun Dot(color: Color) = Box(Modifier.size(8.dp).clip(RoundedCornerShape(4.dp)).background(color))

val Gap = Arrangement.spacedBy(10.dp)
