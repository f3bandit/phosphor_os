// Phosphor for Android: the navigation, the same system as SS1 Tool for Android: an app bar with the
// section's name and the connection pill, a bottom bar with the four main sections and More (a sheet
// with the rest) on phones, a navigation rail with every section on tablets (600 dp and wider), and
// a row of chips for a section's tabs. Back closes the sheet, then goes back through what you opened.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsBottomHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.structuralEqualityPolicy
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** a section of the app: its route, name, icon, a line for the More sheet, and whether it's on the bottom bar */
enum class Section(val route: String, val title: String, val icon: String, val desc: String, val primary: Boolean = false) {
    HOME("home", "Home", "home", "", true),
    LIBRARY("library", "Library", "library", "", true),
    SAVES("saves", "Saves", "saves", "", true),
    VIDEO("video", "Video", "video", "", true),
    BOXART("boxart", "Box art", "boxart", "Look up box art and details, sources and logins"),
    LOOK("look", "Look", "look", "Accent colour and background texture"),
    POWER("power", "Power & updates", "power", "Power check, restart, Phosphor updates, restore points"),
    STORAGE("storage", "Storage", "storage", "SD card, NVMe and USB drives, and this phone"),
    BLUETOOTH("bluetooth", "Bluetooth", "bluetooth", "Controllers paired with the MiSTer"),
    DIAG("diag", "Diagnostics", "diag", "The MiSTer's whole-system checks"),
    NOTES("notes", "Notifications", "notes", "Warnings and news from the MiSTer"),
    CONNECTION("connection", "Connection", "connection", "The paired MiSTer, find it again, forget it"),
    ABOUT("about", "About", "about", "Version, privacy and licences");

    companion object {
        fun of(route: String?): Section? {
            val r = route?.substringBefore('/')?.substringBefore('?') ?: return null
            return entries.firstOrNull { it.route == r }
        }
    }
}

/** what the screen on show puts in the app bar */
class BarState {
    var title by mutableStateOf("")
    var onBack by mutableStateOf<(() -> Unit)?>(null, structuralEqualityPolicy())
    var action by mutableStateOf<(@Composable RowScope.() -> Unit)?>(null, structuralEqualityPolicy())
}

val LocalBar = compositionLocalOf<BarState?> { null }

/** a section's tabs, handed to the screens inside it (a screen that has its own Back hides them) */
class TabsSpec(val tabs: List<String>, val tab: Int, val onTab: (Int) -> Unit)

val LocalTabs = compositionLocalOf<TabsSpec?> { null }

/**
 * A screen's content. Inside the app it fills the area under the app bar (which shows title, a back
 * arrow when onBack is set, and action); on its own (the screenshot tests' single screens) it draws
 * a plain header instead. tabs: the section's chips.
 */
@Composable
fun Screen(
    label: String, title: String, onBack: (() -> Unit)? = null, action: (@Composable RowScope.() -> Unit)? = null,
    tabs: List<String> = emptyList(), tab: Int = 0, onTab: (Int) -> Unit = {},
    content: @Composable () -> Unit,
) {
    val bar = LocalBar.current
    val outer = LocalTabs.current
    @Suppress("NAME_SHADOWING") val tabs = if (tabs.isEmpty() && outer != null && onBack == null) outer.tabs else tabs
    @Suppress("NAME_SHADOWING") val tab = if (outer != null && tabs === outer.tabs) outer.tab else tab
    @Suppress("NAME_SHADOWING") val onTab = if (outer != null && tabs === outer.tabs) outer.onTab else onTab
    if (bar != null) {
        SideEffect { bar.title = title; bar.onBack = onBack; bar.action = action }
        if (onBack != null) BackHandler(onBack = onBack)
        Column(Modifier.fillMaxSize().imePadding()) {
            if (tabs.isNotEmpty()) Chips(tabs, tab, onTab)
            else Spacer(Modifier.height(12.dp))
            Box(Modifier.weight(1f)) { content() }
        }
        return
    }
    Column(Modifier.fillMaxSize().background(P.bg).statusBarsPadding().navigationBarsPadding().imePadding()) {
        AppBar(title.ifEmpty { label }, onBack, action, null, null)
        if (tabs.isNotEmpty()) Chips(tabs, tab, onTab) else Spacer(Modifier.height(12.dp))
        Box(Modifier.weight(1f)) { content() }
    }
}

/** the section's tabs: a scrolling row of chips, the chosen one filled with the accent */
@Composable
fun Chips(tabs: List<String>, tab: Int, onTab: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        tabs.forEachIndexed { i, t ->
            val on = i == tab
            Box(
                Modifier.clip(RoundedCornerShape(8.dp))
                    .background(if (on) P.acc else P.panel2)
                    .border(1.dp, if (on) P.acc else P.acc.copy(alpha = 0.55f), RoundedCornerShape(8.dp))
                    .clickable { onTab(i) }
                    .padding(horizontal = 14.dp, vertical = 8.dp),
            ) { Text(t, style = T.small.copy(fontSize = 14.sp, color = if (on) P.bg else P.text, fontWeight = if (on) FontWeight.SemiBold else FontWeight.Normal)) }
        }
    }
}

/** the connection pill on the right of the app bar */
data class Conn(val text: String, val ok: Boolean?)

@Composable
private fun AppBar(title: String, onBack: (() -> Unit)?, action: (@Composable RowScope.() -> Unit)?, conn: Conn?, onConn: (() -> Unit)?) {
    Row(
        Modifier.fillMaxWidth().height(56.dp).background(P.bar).padding(start = if (onBack != null) 4.dp else 16.dp, end = 8.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        if (onBack != null) {
            Box(Modifier.size(48.dp).clip(RoundedCornerShape(24.dp)).clickable(onClick = onBack), contentAlignment = Alignment.Center) {
                Icon("back", P.text, 24.dp)
            }
        }
        Text(title, style = T.bar, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
        action?.invoke(this)
        if (conn != null && action == null) {
            Row(
                Modifier.widthIn(max = 200.dp).clip(RoundedCornerShape(16.dp)).background(P.panel2)
                    .then(if (onConn != null) Modifier.clickable(onClick = onConn) else Modifier)
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                Dot(when (conn.ok) { true -> P.good; false -> P.bad; null -> P.faint })
                Text(conn.text, style = T.small.copy(color = P.text), maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
        }
    }
}

/**
 * The app around the screens. section: the one on show (null: a screen of its own, like pairing);
 * badge: sections with something unseen (a dot on the icon).
 */
@Composable
fun AppShell(
    section: Section?, conn: Conn?, texture: Int, badge: Set<Section>,
    onSection: (Section) -> Unit, onConn: () -> Unit, startMore: Boolean = false,
    content: @Composable () -> Unit,
) {
    val bar = remember { BarState() }
    var more by remember { mutableStateOf(startMore) }
    BoxWithConstraints(Modifier.fillMaxSize().background(P.bar)) {
        val wide = maxWidth >= 600.dp
        Column(Modifier.fillMaxSize()) {
            Column(Modifier.background(P.bar).statusBarsPadding()) {
                AppBar(bar.title.ifEmpty { section?.title ?: "Phosphor" }, bar.onBack, bar.action, conn, onConn)
            }
            Row(Modifier.weight(1f)) {
                if (wide && section != null) Rail(section, badge, onSection)
                TextureBackground(texture, Modifier.weight(1f).fillMaxHeight()) {
                    Box(Modifier.fillMaxSize().then(if (wide) Modifier else Modifier)) {
                        CompositionLocalProvider(LocalBar provides bar) {
                            Box(Modifier.fillMaxSize().then(if (wide) Modifier.padding(horizontal = 8.dp) else Modifier), contentAlignment = Alignment.TopCenter) {
                                Box(Modifier.widthIn(max = 960.dp).fillMaxSize()) { content() }
                            }
                        }
                    }
                }
            }
            if (!wide && section != null) BottomBar(section, more, badge, onSection = { more = false; onSection(it) }, onMore = { more = !more })
            else Spacer(Modifier.fillMaxWidth().background(P.bg).windowInsetsBottomHeight(WindowInsets.navigationBars))
        }
        if (!wide) MoreSheet(more, badge, onClose = { more = false }, onSection = { more = false; onSection(it) })
    }
    // after the screens, so Back closes the sheet before it goes back a screen
    BackHandler(enabled = more) { more = false }
}

@Composable
private fun NavItem(s: Section, on: Boolean, dot: Boolean, label: String = s.title, onClick: () -> Unit, modifier: Modifier = Modifier) {
    Column(
        modifier.clickable(interactionSource = remember { MutableInteractionSource() }, indication = null, onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
    ) {
        Box {
            Box(Modifier.width(56.dp).height(30.dp).clip(RoundedCornerShape(15.dp)).background(if (on) P.navOn else Color.Transparent), contentAlignment = Alignment.Center) {
                Icon(if (label == "More") "more" else s.icon, if (on) P.acc else P.dim, 22.dp)
            }
            if (dot) Box(Modifier.align(Alignment.TopEnd).padding(top = 3.dp, end = 14.dp).size(7.dp).clip(RoundedCornerShape(4.dp)).background(P.acc))
        }
        Spacer(Modifier.height(4.dp))
        Text(label, style = T.small.copy(fontSize = 12.sp, fontWeight = FontWeight.Medium, color = if (on) P.text else P.dim), maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun BottomBar(section: Section, moreOpen: Boolean, badge: Set<Section>, onSection: (Section) -> Unit, onMore: () -> Unit) {
    Column(Modifier.fillMaxWidth().background(P.bar).navigationBarsPadding()) {
        Row(Modifier.fillMaxWidth().height(72.dp).padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Section.entries.filter { it.primary }.forEach { s ->
                NavItem(s, s == section && !moreOpen, s in badge, onClick = { onSection(s) }, modifier = Modifier.weight(1f).fillMaxHeight())
            }
            NavItem(Section.ABOUT, moreOpen || !section.primary, badge.any { !it.primary }, label = "More", onClick = onMore, modifier = Modifier.weight(1f).fillMaxHeight())
        }
    }
}

@Composable
private fun Rail(section: Section, badge: Set<Section>, onSection: (Section) -> Unit) {
    Column(
        Modifier.width(88.dp).fillMaxHeight().background(P.bar).verticalScroll(rememberScrollState()).navigationBarsPadding().padding(vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Section.entries.forEach { s ->
            NavItem(s, s == section, s in badge, label = s.title.substringBefore(" &"), onClick = { onSection(s) }, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp))
        }
    }
}

@Composable
private fun MoreSheet(open: Boolean, badge: Set<Section>, onClose: () -> Unit, onSection: (Section) -> Unit) {
    AnimatedVisibility(open, enter = fadeIn(), exit = fadeOut()) {
        Box(Modifier.fillMaxSize().background(Color(0x99000000)).clickable(interactionSource = remember { MutableInteractionSource() }, indication = null, onClick = onClose))
    }
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.BottomCenter) {
        AnimatedVisibility(open, enter = slideInVertically { it / 3 } + fadeIn(), exit = slideOutVertically { it / 3 } + fadeOut()) {
            Column(
                Modifier.fillMaxWidth().heightIn(max = 640.dp).clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp)).background(P.panel)
                    .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}
                    .navigationBarsPadding().padding(bottom = 16.dp),
            ) {
                Box(Modifier.padding(top = 8.dp, bottom = 12.dp).width(32.dp).height(4.dp).clip(RoundedCornerShape(2.dp)).background(P.line).align(Alignment.CenterHorizontally))
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    Section.entries.filter { !it.primary }.forEach { s ->
                        Row(
                            Modifier.fillMaxWidth().clickable { onSection(s) }.padding(horizontal = 24.dp, vertical = 12.dp),
                            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp),
                        ) {
                            Box {
                                Icon(s.icon, P.dim, 24.dp)
                                if (s in badge) Box(Modifier.align(Alignment.TopEnd).size(7.dp).clip(RoundedCornerShape(4.dp)).background(P.acc))
                            }
                            Column(Modifier.weight(1f)) {
                                Text(s.title, style = T.name)
                                Text(s.desc, style = T.small, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            }
                        }
                    }
                }
            }
        }
    }
}
