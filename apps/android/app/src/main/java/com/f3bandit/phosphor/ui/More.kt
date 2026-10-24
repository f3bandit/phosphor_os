// Phosphor for Android: the sections in More (the rail on tablets): Box art, Look, Power & updates,
// Storage, Notifications, Connection and About. Like the other screens, each is data in, events out.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageShader
import androidx.compose.ui.graphics.ShaderBrush
import androidx.compose.ui.graphics.TileMode
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.f3bandit.phosphor.Drive
import com.f3bandit.phosphor.GuardState
import com.f3bandit.phosphor.Note
import com.f3bandit.phosphor.PowerState
import com.f3bandit.phosphor.ScrapeAccount
import com.f3bandit.phosphor.ScrapeProgress
import com.f3bandit.phosphor.ScrapeSource
import com.f3bandit.phosphor.StorageState
import com.f3bandit.phosphor.UpdateState
import com.f3bandit.phosphor.scrape.PhoneScrapeState
import com.f3bandit.phosphor.scrape.StoredArt
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

// ---------- shared bits ----------

/** a button that asks for a second press before doing something that can't be taken back */
@Composable
fun SureButton(text: String, again: String, modifier: Modifier = Modifier, primary: Boolean = false, enabled: Boolean = true, onConfirm: () -> Unit) {
    var armed by remember { mutableStateOf(false) }
    LaunchedEffect(armed) { if (armed) { delay(4000); armed = false } }
    Button(if (armed) again else text, modifier, primary = primary || armed, enabled = enabled) {
        if (armed) { armed = false; onConfirm() } else armed = true
    }
}

fun gb(kb: Long): String = when {
    kb >= 1048576L * 1024 -> "%.1f TB".format(kb / (1048576.0 * 1024))
    kb >= 1048576L -> "%.1f GB".format(kb / 1048576.0)
    kb >= 1024 -> "%.0f MB".format(kb / 1024.0)
    else -> "$kb KB"
}

fun sizeText(bytes: Long) = gb(bytes / 1024)

private val df = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
fun whenText(ms: Long) = if (ms <= 0) "" else df.format(Date(ms))

/** a job's output, short: the lines that say how it went */
@Composable
fun Output(text: String) {
    if (text.isBlank()) return
    Text(text.trim().lines().takeLast(14).joinToString("\n"), style = T.small.copy(fontFamily = Mono, fontSize = 12.sp, color = P.dim),
        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(P.panel).padding(12.dp))
}

@Composable
private fun Scroll(content: @Composable () -> Unit) {
    Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp), verticalArrangement = Gap) {
        content()
        Spacer(Modifier.height(24.dp))
    }
}

// ---------- Box art ----------

val BOXART_TABS = listOf("Look up", "Sources")

private val SOURCE_ABOUT = mapOf(
    "libretro" to "No account. Box art from libretro's thumbnails, matched by each file's content (CRC or product code), then by its exact name.",
    "arcadedb" to "No account. Arcade games by their MAME set name.",
    "launchbox" to "No account. LaunchBox's games database: on the phone it's downloaded once (about 110 MB) and kept in the app's cache.",
    "thegamesdb" to "Needs a free API key from thegamesdb.net.",
    "screenscraper" to "Your screenscraper.fr login is enough (a free account). A developer login works too, if you have one.",
    "igdb" to "Needs a free Twitch developer app (dev.twitch.tv): its Client ID and secret.",
)

/** the logins each source takes: key, label, secret */
val SOURCE_FIELDS = mapOf(
    "screenscraper" to listOf(Triple("user", "User name", false), Triple("password", "Password", true), Triple("devid", "Developer id (only if you have one)", false), Triple("devpassword", "Developer password", true)),
    "thegamesdb" to listOf(Triple("apikey", "API key (free, from thegamesdb.net)", true)),
    "igdb" to listOf(Triple("client_id", "Client ID (Twitch developer app)", false), Triple("client_secret", "Client secret", true)),
)

@Composable
fun BoxArtLookupContent(phone: PhoneScrapeState, ss1: ScrapeProgress?, error: String?, onPhone: () -> Unit, onSs1: () -> Unit) {
    Screen("Box art", "Box art") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            Label("On this phone", Modifier.padding(start = 4.dp))
            Card {
                val pct = if (phone.total > 0) phone.done * 100 / phone.total else 0
                when {
                    phone.running -> {
                        Row2(phone.status.ifEmpty { "Looking up box art" }, value = if (phone.total > 0) "$pct%" else null,
                            sub = "${phone.done} of ${phone.total}  ·  ${phone.found} found, ${phone.notFound} not found" + (if (phone.later > 0) ", ${phone.later} for later" else ""))
                        if (phone.game.isNotEmpty()) Row2(phone.game, sub = phone.system, dim = true)
                    }
                    phone.started > 0 -> Row2(phone.status.ifEmpty { "Done" }, sub = "${phone.found} found, ${phone.notFound} not found" + (if (phone.later > 0) ", ${phone.later} for later" else ""))
                    else -> Row2("Much faster than the MiSTer", sub = "The phone fetches the lists and pictures, makes the box art and sends it to the MiSTer. It keeps going with the app in the background.", dim = true)
                }
            }
            phone.notes.forEach { Text(it, style = T.small.copy(color = P.warn)) }
            Button(if (phone.running) "Stop" else "Look up on this phone", Modifier.fillMaxWidth(), primary = !phone.running, onClick = onPhone)
            Text("Each picture is kept on the phone until the MiSTer has it, then deleted (Storage > This phone). " +
                "The first run downloads the game lists (libretro, and LaunchBox's 110 MB once): use WiFi.", style = T.small)
            Label("On the MiSTer", Modifier.padding(start = 4.dp, top = 8.dp))
            Card {
                if (ss1 == null) Row2("Asking the MiSTer...", dim = true)
                else if (ss1.running) Row2(ss1.status.ifEmpty { "Getting box art" }, value = if (ss1.total > 0) "${ss1.done * 100 / ss1.total}%" else null,
                    sub = "${ss1.done} of ${ss1.total}  ·  ${ss1.found} found, ${ss1.notFound} not found" + (if (ss1.game.isNotEmpty()) "\n${ss1.system} › ${ss1.game}" else ""))
                else Row2("Not running", sub = ss1.status.ifEmpty { "Slower, but the phone can be put away" }, dim = true)
            }
            if (ss1?.running != true) Button("Look up on the MiSTer", Modifier.fillMaxWidth(), enabled = !phone.running, onClick = onSs1)
        }
    }
}

@Composable
fun BoxArtSourcesContent(
    sources: List<ScrapeSource>?, accounts: List<ScrapeAccount>, selected: Int, values: Map<String, String>, error: String?, busy: Boolean,
    onSelect: (Int) -> Unit, onToggle: (Int) -> Unit, onMove: (Int, Int) -> Unit, onValue: (String, String) -> Unit, onSave: () -> Unit,
) {
    Screen("Box art", "Box art") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (sources == null) { if (error == null) Loading(); return@Scroll }
            Text("Asked in this order, by the MiSTer and by the phone. Tap one for what it needs; the switch turns it on or off.", style = T.small)
            Card {
                sources.forEachIndexed { i, x ->
                    fun set(k: String) = accounts.firstOrNull { it.sec == x.id && it.key == k }?.set == true
                    val st = when { !x.on -> "Off"; x.ready -> "On"; x.id == "screenscraper" && set("user") -> "Needs the password"; else -> "Needs a login" }
                    Row(
                        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(if (i == selected) P.panelOn else Color.Transparent)
                            .clickable { onSelect(i) }.padding(start = 16.dp, end = 4.dp, top = 6.dp, bottom = 6.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(Modifier.weight(1f)) {
                            Text("${i + 1}. ${x.name}", style = T.name)
                            Text(st, style = T.small.copy(color = if (x.on && x.ready) P.good else if (x.on) P.warn else P.dim))
                        }
                        Toggle(x.on) { onToggle(i) }
                        Box(Modifier.size(40.dp).clip(RoundedCornerShape(20.dp)).clickable(enabled = i > 0) { onMove(i, -1) }, contentAlignment = Alignment.Center) { Icon("up", if (i > 0) P.text else P.faint, 20.dp) }
                        Box(Modifier.size(40.dp).clip(RoundedCornerShape(20.dp)).clickable(enabled = i < sources.size - 1) { onMove(i, 1) }, contentAlignment = Alignment.Center) { Icon("down", if (i < sources.size - 1) P.text else P.faint, 20.dp) }
                    }
                }
            }
            sources.getOrNull(selected)?.let { x ->
                Label(x.name, Modifier.padding(start = 4.dp, top = 8.dp))
                Text(SOURCE_ABOUT[x.id] ?: "", style = T.small)
                SOURCE_FIELDS[x.id]?.let { fields ->
                    fields.forEach { (k, label, secret) ->
                        val a = accounts.firstOrNull { it.sec == x.id && it.key == k }
                        Text(label, style = T.small.copy(color = P.text))
                        Field(values[k] ?: "", { onValue(k, it) }, if (secret) (if (a?.set == true) "Saved (type to change)" else "Not set") else "Not set", password = secret)
                    }
                    Button(if (busy) "Saving..." else "Save on the MiSTer", Modifier.fillMaxWidth(), primary = true, enabled = !busy, onClick = onSave)
                    Text("Saved on the MiSTer (phosphor/config/scraper.ini), sent over your home network only.", style = T.small)
                }
            }
        }
    }
}

/** an on/off switch in the accent colour */
@Composable
fun Toggle(on: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.padding(horizontal = 4.dp).width(44.dp).height(26.dp).clip(RoundedCornerShape(13.dp)).background(if (on) P.acc else P.panel2)
            .border(1.dp, if (on) P.acc else P.line, RoundedCornerShape(13.dp)).clickable(onClick = onClick).padding(3.dp),
        contentAlignment = if (on) Alignment.CenterEnd else Alignment.CenterStart,
    ) { Box(Modifier.size(20.dp).clip(RoundedCornerShape(10.dp)).background(if (on) P.bg else P.dim)) }
}

// ---------- Look ----------

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun LookContent(accent: String, texture: Int, onAccent: (String) -> Unit, onTexture: (Int) -> Unit) {
    val ctx = LocalContext.current
    val density = LocalDensity.current.density
    Screen("Look", "Look") {
        Scroll {
            Label("Accent colour", Modifier.padding(start = 4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(14.dp), modifier = Modifier.padding(4.dp)) {
                ACCENTS.forEach { (name, c) ->
                    Box(
                        Modifier.size(44.dp).clip(RoundedCornerShape(22.dp)).background(c)
                            .border(3.dp, if (name == accent) P.text else Color.Transparent, RoundedCornerShape(22.dp)).clickable { onAccent(name) },
                    )
                }
            }
            Label("Background texture", Modifier.padding(start = 4.dp, top = 8.dp))
            FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Texture.ORDER.forEach { n ->
                    val on = n == texture
                    Column(
                        Modifier.width(104.dp).clip(RoundedCornerShape(12.dp)).background(P.bg)
                            .border(if (on) 2.dp else 1.dp, if (on) P.acc else P.line, RoundedCornerShape(12.dp)).clickable { onTexture(n) },
                    ) {
                        Box(Modifier.fillMaxWidth().height(64.dp).clip(RoundedCornerShape(topStart = 12.dp, topEnd = 12.dp))) {
                            if (Texture.full(n)) {
                                val img = remember(n, P.acc) { Texture.board(ctx, n, P.acc, P.bg, 0.35f) }
                                if (img != null) Image(img, null, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
                            } else {
                                val tile = remember(n, P.acc, density) { Texture.tile(n, P.acc, density) }
                                if (tile != null) Box(Modifier.fillMaxSize().background(ShaderBrush(ImageShader(tile, TileMode.Repeated, TileMode.Repeated))))
                            }
                        }
                        Text(Texture.name(n), style = T.small.copy(fontSize = 12.sp, lineHeight = 15.sp, color = if (on) P.text else P.dim), maxLines = 2,
                            modifier = Modifier.height(42.dp).padding(horizontal = 8.dp, vertical = 6.dp))
                    }
                }
            }
            Text("Kept on this phone. The TV's own look is set on the MiSTer (Settings > Look); these are the same colours and textures.", style = T.small)
        }
    }
}

// ---------- Power & updates ----------

val POWER_TABS = listOf("Power", "Update", "Restore points")

@Composable
fun PowerContent(power: PowerState?, error: String?, onAck: () -> Unit, onSystem: (String) -> Unit) {
    Screen("Power & updates", "Power & updates") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            Label("Power check", Modifier.padding(start = 4.dp))
            Card {
                when {
                    power == null -> if (error == null) Loading()
                    !power.installed -> Row2("Not installed", sub = "The health-watch module isn't on the MiSTer", dim = true)
                    power.warn -> Row2("Power supply problem?", sub = "Lost power ${power.count} times in the middle of something. A weak or shared power adapter is the usual cause.", accent = true)
                    else -> Row2(if (power.count == 0) "No power cuts" else "Lost power ${power.count} time${if (power.count == 1) "" else "s"}",
                        sub = "Tells you after ${power.warnAt}: heavy work cut short with no trace", value = "OK")
                }
                power?.events?.take(5)?.forEach { Row2(it.what, sub = whenText(it.at * 1000), dim = true) }
            }
            if (power?.warn == true) Button("I've dealt with it", Modifier.fillMaxWidth(), onClick = onAck)
            Label("The MiSTer", Modifier.padding(start = 4.dp, top = 8.dp))
            Card { Row2("Restart", sub = "The drives are marked clean first, then it starts again. A game that's running is closed.") }
            SureButton("Restart", "Press again to restart", Modifier.fillMaxWidth()) { onSystem("reboot") }
            Card { Row2("Shut down safely", sub = "Writes everything and marks the drives clean. The TV says when it's safe to switch off.") }
            SureButton("Shut down", "Press again to shut down", Modifier.fillMaxWidth()) { onSystem("shutdown") }
            Card { Row2("Reload the TV screen", sub = "Restarts Phosphor's screen on the TV, not the MiSTer: for when it looks stuck. A running game keeps running.") }
            SureButton("Reload", "Press again to reload", Modifier.fillMaxWidth()) { onSystem("reload_ui") }
        }
    }
}

@Composable
fun UpdateContent(u: UpdateState?, error: String?, onInstall: (String) -> Unit, onRollback: (String) -> Unit) {
    Screen("Power & updates", "Power & updates") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (u == null) { if (error == null) Loading(); return@Scroll }
            if (!u.available) { Card { Row2("The updater isn't on the MiSTer", sub = "Install Phosphor 0.27.6 once with the update script, then updates work from here.", dim = true) }; return@Scroll }
            val busy = u.state == "working"
            Card { Row2("Phosphor ${u.version.ifEmpty { "?" }}", value = "installed") }
            if (u.msg.isNotEmpty() && (busy || System.currentTimeMillis() / 1000 - u.at < 600))
                Text(u.msg, style = T.small.copy(color = if (u.state == "failed") P.bad else if (busy) P.acc else P.good))
            val newer = u.packages.filter { com.f3bandit.phosphor.versionNewer(it.version, u.version) }
            val other = u.packages - newer.toSet()
            if (u.packages.isEmpty() && u.rollbacks.none { it.version != u.version })
                Text("No packages on the MiSTer yet, and no earlier version kept. Send one from Phosphor for Windows, or copy phosphor-<version>-armhf.tar.gz to the SD card.", style = T.small)
            (newer + other).forEach { p ->
                Card { Row2("Install Phosphor ${p.version}", sub = if (p.file.contains("/media/usb")) "On a USB drive" else "On the MiSTer", accent = p in newer) }
                SureButton("Install ${p.version}", "Press again: the TV closes Phosphor and opens it again", Modifier.fillMaxWidth(), primary = p in newer, enabled = !busy) { onInstall(p.file) }
            }
            u.rollbacks.filter { it.version != u.version }.forEach { r ->
                Card { Row2("Roll back to Phosphor ${r.version}", sub = r.date) }
                SureButton("Roll back", "Press again to roll back", Modifier.fillMaxWidth(), enabled = !busy) { onRollback(r.version) }
            }
            Text("A package is checked, the installed version is backed up, and the MiSTer restarts by itself to finish, with your library, box art and settings as they were.", style = T.small)
        }
    }
}

@Composable
fun GuardContent(g: GuardState?, error: String?, onRestore: () -> Unit, onKeep: () -> Unit, onRollback: (String) -> Unit) {
    Screen("Power & updates", "Power & updates") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (g == null) { if (error == null) Loading(); return@Scroll }
            if (!g.installed) { Card { Row2("Not installed", sub = "The update-guard module isn't on the MiSTer", dim = true) }; return@Scroll }
            Card {
                Row2("MiSTer Linux", value = g.linux.ifEmpty { "–" })
                if (g.updatedAt.isNotEmpty()) Row2("Last update", value = g.updatedAt, sub = if (g.updatedFrom.isNotEmpty()) "from ${g.updatedFrom}" else null)
            }
            val reset = g.files.filter { it.reset }
            if (g.awaiting && reset.isNotEmpty()) {
                Label("Reset by the last update", Modifier.padding(start = 4.dp, top = 8.dp))
                Card { reset.forEach { Row2(it.path.substringAfterLast('/'), sub = it.path) } }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Button("Put my settings back", Modifier.weight(1f), primary = true, enabled = g.job?.busy != true, onClick = onRestore)
                    SureButton("Keep the new ones", "Press again", Modifier.weight(1f), enabled = g.job?.busy != true, onConfirm = onKeep)
                }
                Text("Network settings are put back on the MiSTer itself (Settings > Updates).", style = T.small)
            }
            Label("Restore points", Modifier.padding(start = 4.dp, top = 8.dp))
            if (g.points.isEmpty()) Text("None yet. One is saved before every update.", style = T.small)
            g.points.forEach { p ->
                Card { Row2(p.label.ifEmpty { p.name }, value = p.created + if (p.complete) "" else " · partial", sub = "MiSTer Linux ${p.linux.ifEmpty { "–" }}") }
                SureButton("Roll back to this", "Press again to roll back", Modifier.fillMaxWidth(), enabled = g.job?.busy != true) { onRollback(p.name) }
            }
            g.job?.let { j ->
                Label(if (j.busy) "Working..." else "Last job: ${j.name}", Modifier.padding(start = 4.dp, top = 8.dp))
                Output(j.output)
            }
            Text("Rolling back puts MiSTer's system files as they were then (it may download that Linux release). Your games, saves and Phosphor settings aren't touched.", style = T.small)
        }
    }
}

// ---------- Storage ----------

val STORAGE_TABS = listOf("SD card", "NVMe / USB", "This phone")

private fun driveName(d: Drive) = if (d.kind == "sd") "SD card" else (if (d.kind == "nvme") "NVMe" else "USB") + if (d.model.isNotEmpty()) " · ${d.model}" else ""

private fun checksFor(d: Drive): List<Triple<String, String, String>> {
    val c = mutableListOf(Triple("quick", "Quick check", "Partitions, file system and the error log"),
        Triple("windows", "Will Windows complain?", "Whether the drive is marked dirty"),
        Triple("speed", "Speed test", "Reads and writes one temporary file"),
        Triple("fscheck", "Check the file system", "Unmounts it and checks it, read only. Games on it stop while it runs."))
    if (d.kind == "nvme") c.add(2, Triple("health", "Drive health", "The NVMe drive's own health report"))
    return c
}

@Composable
private fun DriveCard(d: Drive, s: StorageState, onCheck: (String, String) -> Unit, onMount: (String) -> Unit) {
    val l = s.last
    val mine = l.target == d.path && l.job.isNotEmpty()
    Card {
        Row2(driveName(d), value = if (d.mounted) "${gb(d.freeKb)} free" else "not mounted", sub = "${gb(d.sizeKb)}  ·  ${d.fs}" + if (d.kind == "nvme") "  ·  PCIe " + (when (d.gen) { "gen4" -> "Gen 4"; "gen3" -> "Gen 3"; else -> "not known" }) else "",
            accent = !d.mounted || d.gen == "gen4")
    }
    if (d.sizeKb > 0 && d.mounted) {
        val used = 1f - d.freeKb.toFloat() / d.sizeKb
        Box(Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)).background(P.panel2)) {
            Box(Modifier.fillMaxWidth(used.coerceIn(0f, 1f)).height(6.dp).background(if (used > 0.95f) P.bad else P.acc))
        }
    }
    if (d.gen == "gen4") Text("Gen 4 NVMe drive: these cause boot and file system trouble in the SS1 dock. A Gen 3 drive is the safe choice.", style = T.small.copy(color = P.bad))
    if ((d.watts.toFloatOrNull() ?: 0f) > 7f) Text("Rated at ${d.watts} W: more than the dock likes to give. If the MiSTer restarts under load, this drive is a likely cause.", style = T.small.copy(color = P.warn))
    if (!d.mounted) {
        Text("Not mounted: MiSTer can't see the games on it right now.", style = T.small.copy(color = P.warn))
        Button("Mount it again", Modifier.fillMaxWidth(), primary = true, enabled = !l.busy) { onMount(d.path) }
    } else {
        Label("Checks", Modifier.padding(start = 4.dp, top = 4.dp))
        Card { checksFor(d).forEach { (t, name, what) -> Row2(name, sub = what, value = if (l.busy) null else "Run", onClick = if (l.busy) null else ({ onCheck(t, d.path) })) } }
        SureButton("Repair the file system", "Press again: it unmounts and repairs", Modifier.fillMaxWidth(), enabled = !l.busy) { onCheck("fsrepair", d.path) }
    }
    if (mine) {
        Label(if (l.busy) "Checking: ${l.job}" else "Last: ${l.job} · ${whenText(l.at * 1000)}", Modifier.padding(start = 4.dp, top = 4.dp))
        Output(l.output)
    }
}

@Composable
fun StorageDrivesContent(s: StorageState?, sd: Boolean, error: String?, onCheck: (String, String) -> Unit, onMount: (String) -> Unit) {
    Screen("Storage", "Storage") {
        Scroll {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (s == null) { if (error == null) Loading(); return@Scroll }
            if (!s.installed) { Card { Row2("The storage module isn't on the MiSTer yet", sub = "It comes with Phosphor 0.27", dim = true) }; return@Scroll }
            val ds = s.drives.filter { (it.kind == "sd") == sd }
            if (ds.isEmpty()) Card { Row2(if (sd) "No SD card found" else "No NVMe or USB drive is plugged in", dim = true) }
            ds.forEach { DriveCard(it, s, onCheck, onMount) }
            if (sd && s.overlapTool.isNotEmpty())
                Text("Faulty overlap \"fix\" tool found (${s.overlapTool.joinToString { it.substringAfterLast('/') }}): remove it from Phosphor for Windows or on the MiSTer.", style = T.small.copy(color = P.bad))
            Text("The checks only read, except the speed test (one temporary file) and Repair, which asks first.", style = T.small)
        }
    }
}

@Composable
fun PhoneStorageContent(art: List<StoredArt>, artBytes: Long, listsBytes: Long, autoDelete: Boolean, busy: Boolean,
                        onAuto: (Boolean) -> Unit, onSend: () -> Unit, onDeleteArt: () -> Unit, onDeleteLists: () -> Unit) {
    Screen("Storage", "Storage") {
        Scroll {
            Label("Downloaded box art", Modifier.padding(start = 4.dp))
            val waiting = art.count { !it.sent }
            Card {
                Row2("${art.size} picture${if (art.size == 1) "" else "s"}", value = sizeText(artBytes),
                    sub = if (waiting > 0) "$waiting the MiSTer hasn't got yet" else if (art.isEmpty()) "Nothing kept on the phone" else "All on the MiSTer")
                Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 12.dp, top = 6.dp, bottom = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("Auto delete", style = T.name)
                        Text("Delete each picture once the MiSTer has it", style = T.small)
                    }
                    Toggle(autoDelete) { onAuto(!autoDelete) }
                }
            }
            if (waiting > 0) Button(if (busy) "Sending..." else "Send again ($waiting)", Modifier.fillMaxWidth(), primary = true, enabled = !busy, onClick = onSend)
            if (art.isNotEmpty()) SureButton("Delete all", "Press again to delete ${art.size}", Modifier.fillMaxWidth(), enabled = !busy, onConfirm = onDeleteArt)
            if (art.isNotEmpty()) Card { art.take(30).forEach { a -> Row2(a.title, sub = a.system, value = if (a.sent) "on the MiSTer" else "waiting", accent = !a.sent) } }
            Text("Only pictures this app downloaded are here, in its own folder. Photos you pick from your gallery are never touched.", style = T.small)
            Label("Game lists", Modifier.padding(start = 4.dp, top = 8.dp))
            Card { Row2("libretro and LaunchBox lists", value = sizeText(listsBytes), sub = "Kept so the next lookup is quick; fetched again after 30 days") }
            if (listsBytes > 0) SureButton("Delete the lists", "Press again (LaunchBox is 110 MB to get again)", Modifier.fillMaxWidth(), enabled = !busy, onConfirm = onDeleteLists)
        }
    }
}

// ---------- Notifications ----------

@Composable
fun NotesContent(notes: List<Note>, notify: Boolean, allowed: Boolean, onNotify: (Boolean) -> Unit, onAllow: () -> Unit, onOpen: (Note) -> Unit, onClear: () -> Unit) {
    Screen("Notifications", "Notifications") {
        Scroll {
            Card {
                Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 12.dp, top = 8.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("Tell me on the phone", style = T.name)
                        Text("Checked every minute with the app open, about every 15 minutes in the background (on WiFi)", style = T.small)
                    }
                    Toggle(notify) { onNotify(!notify) }
                }
            }
            if (notify && !allowed) {
                Text("Android's notifications are off for Phosphor.", style = T.small.copy(color = P.warn))
                Button("Allow notifications", Modifier.fillMaxWidth(), primary = true, onClick = onAllow)
            }
            if (notes.isEmpty()) Card { Row2("Nothing to tell", sub = "Power, storage and update warnings, and box art finishing, show here", dim = true) }
            else {
                Card {
                    notes.forEach { n ->
                        Row2(n.title, sub = n.body + "\n" + whenText(n.at), accent = n.level == "warn" && !n.seen, value = if (n.go.isNotEmpty()) "›" else null,
                            onClick = if (n.go.isNotEmpty()) ({ onOpen(n) }) else null)
                    }
                }
                Button("Clear", Modifier.fillMaxWidth(), onClick = onClear)
            }
        }
    }
}

// ---------- Connection and About ----------

@Composable
fun ConnectionContent(address: String, reachable: Boolean?, version: String, searching: Boolean, message: String?,
                      onFind: () -> Unit, onForget: () -> Unit) {
    Screen("Connection", "Connection") {
        Scroll {
            Card {
                Row2("Paired with", value = address)
                Row2(when (reachable) { true -> "Connected"; false -> "Not answering"; null -> "Checking..." }, value = if (version.isNotEmpty()) "Phosphor $version" else null,
                    accent = reachable == false)
            }
            if (reachable == false) Text("Is it on, on this WiFi, with Remote access turned on? If the router gave it a new address, look for it:", style = T.small)
            Button(if (searching) "Looking on this WiFi..." else "Find it on this WiFi", Modifier.fillMaxWidth(), enabled = !searching, onClick = onFind)
            if (message != null) Text(message, style = T.small.copy(color = P.acc))
            Label("Forget", Modifier.padding(start = 4.dp, top = 8.dp))
            SureButton("Forget this MiSTer", "Press again: you'll pair again", Modifier.fillMaxWidth(), onConfirm = onForget)
            Text("To remove this phone on the MiSTer too: Settings › Remote access, select the phone, press X.", style = T.small)
        }
    }
}

@Composable
fun AboutContent(version: String) {
    Screen("About", "About") {
        Scroll {
            Card {
                Row2("Phosphor for Android", value = version)
                Row2("Works with", value = "Phosphor 0.27.24 or later")
            }
            Text("The app only talks to your MiSTer, on your home network. Box art lookups on the phone go to the services you turned on " +
                "(libretro, ArcadeDB, LaunchBox, and with your logins TheGamesDB, ScreenScraper, IGDB). It collects nothing and sends nothing anywhere else. " +
                "Passwords, WiFi and Bluetooth pairings stay on the MiSTer.", style = T.small)
            Text("PolyForm Noncommercial 1.0.0, like the rest of Phosphor.\n" +
                "Fonts: Inter (SIL Open Font License 1.1), DejaVu Sans Mono (Bitstream Vera licence).", style = T.small)
        }
    }
}
