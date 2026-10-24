// Phosphor for Android: the navigation (SS1 Tool for Android's: app bar, bottom bar or rail, More) and
// the loaders that connect the screens to the MiSTer.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.Manifest
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.f3bandit.phosphor.scrape.ArtStore
import com.f3bandit.phosphor.scrape.PhoneScrape
import com.f3bandit.phosphor.scrape.ScrapeService
import com.f3bandit.phosphor.ui.AboutContent
import com.f3bandit.phosphor.ui.AddForm
import com.f3bandit.phosphor.ui.AppShell
import com.f3bandit.phosphor.ui.ArtCache
import com.f3bandit.phosphor.ui.BOXART_TABS
import com.f3bandit.phosphor.ui.BackupsContent
import com.f3bandit.phosphor.ui.BasedOnContent
import com.f3bandit.phosphor.ui.BluetoothContent
import com.f3bandit.phosphor.ui.BoxArtLookupContent
import com.f3bandit.phosphor.ui.BoxArtSourcesContent
import com.f3bandit.phosphor.ui.CardsContent
import com.f3bandit.phosphor.ui.CloudContent
import com.f3bandit.phosphor.ui.CloudMode
import com.f3bandit.phosphor.ui.Conn
import com.f3bandit.phosphor.ui.ConnectContent
import com.f3bandit.phosphor.ui.ConnectionContent
import com.f3bandit.phosphor.ui.DiagContent
import com.f3bandit.phosphor.ui.EditContent
import com.f3bandit.phosphor.ui.GameContent
import com.f3bandit.phosphor.ui.GamesContent
import com.f3bandit.phosphor.ui.GuardContent
import com.f3bandit.phosphor.ui.HomeContent
import com.f3bandit.phosphor.ui.LibraryContent
import com.f3bandit.phosphor.ui.LocalTabs
import com.f3bandit.phosphor.ui.LookContent
import com.f3bandit.phosphor.ui.NotesContent
import com.f3bandit.phosphor.ui.POWER_TABS
import com.f3bandit.phosphor.ui.PhoneStorageContent
import com.f3bandit.phosphor.ui.PhosphorTheme
import com.f3bandit.phosphor.ui.PowerContent
import com.f3bandit.phosphor.ui.SOURCE_FIELDS
import com.f3bandit.phosphor.ui.STORAGE_TABS
import com.f3bandit.phosphor.ui.Section
import com.f3bandit.phosphor.ui.StorageDrivesContent
import com.f3bandit.phosphor.ui.TabsSpec
import com.f3bandit.phosphor.ui.UpdateContent
import com.f3bandit.phosphor.ui.VideoListContent
import com.f3bandit.phosphor.ui.VideoSlot
import com.f3bandit.phosphor.ui.VideoSysContent
import com.f3bandit.phosphor.ui.gameValues
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File

class MainActivity : ComponentActivity() {
    private var model: AppModel? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        Prefs.init(this)
        ArtStore.init(this)
        Notes.channels(this)
        val store = KeystorePairingStore(applicationContext)
        val first = intent?.getStringExtra("go")
        setContent {
            PhosphorTheme {
                val m: AppModel = viewModel(factory = object : ViewModelProvider.Factory {
                    @Suppress("UNCHECKED_CAST")
                    override fun <T : ViewModel> create(modelClass: Class<T>): T = AppModel(store) as T
                })
                LaunchedEffect(m) { model = m; if (first != null && savedInstanceState == null) m.go = first }
                PhosphorApp(m)
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        intent.getStringExtra("go")?.let { model?.go = it }
    }
}

private fun enc(s: String) = Uri.encode(s)

const val BuildConfigVersion = "0.8.0"

/** "power/2" -> the route for Power & updates' third tab */
private fun goRoute(go: String): String {
    val s = go.substringBefore('/')
    val tab = go.substringAfter('/', "").toIntOrNull()
    return if (tab != null) "$s?tab=$tab" else s
}

private fun NavHostController.openSection(s: Section) = navigate(s.route) { launchSingleTop = true }

@Composable
fun PhosphorApp(model: AppModel) {
    val nav = rememberNavController()
    val ctx = LocalContext.current
    val paired = model.pairing != null
    val entry by nav.currentBackStackEntryAsState()
    var section by rememberSaveable { mutableStateOf("home") }
    val route = entry?.destination?.route
    LaunchedEffect(route) { Section.of(route)?.let { section = it.route } }
    val askNotify = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { Notes.schedule(ctx) }
    LaunchedEffect(paired) {
        if (!paired) { Notes.unschedule(ctx); nav.navigate("connect") { popUpTo(0) }; return@LaunchedEffect }
        model.watch(ctx)
        if (Prefs.notify) {
            Notes.schedule(ctx)
            if (Build.VERSION.SDK_INT >= 33 && !Notes.canPost(ctx) && !Prefs.getBool("asked_notify")) {
                Prefs.putBool("asked_notify", true)
                askNotify.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }
    LaunchedEffect(model.go, paired) {
        val g = model.go ?: return@LaunchedEffect
        if (!paired) return@LaunchedEffect
        model.go = null
        nav.navigate(goRoute(g)) { launchSingleTop = true }
    }
    val onConnect = route == "connect" || !paired
    val conn = when { !paired -> null; model.reachable == true -> Conn("Connected", true); model.reachable == false -> Conn("Not answering", false); else -> Conn("Connecting", null) }
    val badge = if (Notes.list.any { !it.seen }) setOf(Section.NOTES) else emptySet()
    AppShell(
        section = if (onConnect) null else Section.of(section), conn = conn, texture = Prefs.texture, badge = badge,
        onSection = { nav.openSection(it) }, onConn = { nav.openSection(Section.CONNECTION) },
    ) {
        NavHost(nav, startDestination = if (paired) "home" else "connect") {
            composable("connect") { ConnectRoute(model) { nav.navigate("home") { popUpTo(0) } } }
            composable("home") { HomeRoute(model, nav) }
            composable("library") { LibraryRoute(model, nav) }
            composable(
                "games?system={system}&title={title}&filter={filter}&search={search}",
                arguments = listOf("system", "title", "filter", "search").map { navArgument(it) { type = NavType.StringType; nullable = true; defaultValue = null } },
            ) { e ->
                val a = e.arguments
                GamesRoute(model, nav, a?.getString("system"), a?.getString("title") ?: "Games", a?.getString("filter"), a?.getString("search") == "1")
            }
            composable("game/{id}", arguments = listOf(navArgument("id") { type = NavType.LongType })) { e -> GameRoute(model, nav, e.arguments!!.getLong("id")) }
            composable("edit/{id}", arguments = listOf(navArgument("id") { type = NavType.LongType })) { e -> EditRoute(model, nav, e.arguments!!.getLong("id")) }
            composable("based/{id}", arguments = listOf(navArgument("id") { type = NavType.LongType })) { e -> BasedRoute(model, nav, e.arguments!!.getLong("id")) }
            tabbed("saves") { tab -> SavesRoute(model, tab) }
            composable("video") { VideoListRoute(model, nav) }
            composable("video/{sys}", arguments = listOf(navArgument("sys") { type = NavType.StringType })) { e -> VideoSysRoute(model, nav, e.arguments!!.getString("sys")!!) }
            tabbed("boxart") { tab -> BoxArtRoute(model, tab) }
            composable("look") { LookContent(Prefs.accent, Prefs.texture, onAccent = { Prefs.chooseAccent(it) }, onTexture = { Prefs.chooseTexture(it) }) }
            tabbed("power") { tab -> PowerRoute(model, tab) }
            tabbed("storage") { tab -> StorageRoute(model, tab) }
            composable("bluetooth") { BluetoothRoute(model) }
            composable("diag") { DiagRoute(model) }
            composable("notes") { NotesRoute(model, nav) }
            composable("connection") { ConnectionRoute(model) }
            composable("about") { AboutContent(BuildConfigVersion) }
        }
    }
}

/** a section with tabs: its route takes ?tab=n (a notification opens the right one) */
private fun androidx.navigation.NavGraphBuilder.tabbed(route: String, content: @Composable (MutableTabs) -> Unit) {
    composable("$route?tab={tab}", arguments = listOf(navArgument("tab") { type = NavType.IntType; defaultValue = 0 })) { e ->
        val start = e.arguments?.getInt("tab") ?: 0
        val t = remember { MutableTabs(start) }
        content(t)
    }
}

class MutableTabs(start: Int) { var tab by mutableIntStateOf(start) }

@Composable
private fun Tabs(names: List<String>, t: MutableTabs, content: @Composable () -> Unit) =
    CompositionLocalProvider(LocalTabs provides TabsSpec(names, t.tab) { t.tab = it }) { content() }

// ---------- Saves: memory cards, backups, cloud ----------

private val SAVES_TABS = listOf("Memory cards", "Backups", "Cloud")

@Composable
fun SavesRoute(model: AppModel, t: MutableTabs) = Tabs(SAVES_TABS, t) {
    when (t.tab) {
        0 -> CardsRoute(model)
        1 -> BackupsRoute(model)
        else -> CloudRoute(model)
    }
}

@Composable
fun CardsRoute(model: AppModel) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var st by remember { mutableStateOf<CardState?>(null) }
    var pads by remember { mutableStateOf(listOf<Pad>()) }
    var error by remember { mutableStateOf<String?>(null) }
    var pick by remember { mutableStateOf<CardBackup?>(null) }
    fun handle(s: CardState, wasBusy: Boolean) {
        st = s
        if (wasBusy && !s.busy && (s.job != "prepare" || !s.jobOk) && s.message.isNotEmpty())
            Toast.makeText(ctx, s.message, Toast.LENGTH_LONG).show()
    }
    LaunchedEffect(api.address) {
        var n = 0
        while (true) {
            // the card state every 2 s (busy: 0.8 s); the controllers only for the analog switch, every 4 s
            if (n % 8 == 0 || (st?.busy == true && n % 3 == 0)) {
                val was = st?.busy == true
                try { handle(api.memcard(), was); error = null } catch (e: Exception) { error = model.handle(e) }
            }
            if (st?.available == true && n % 16 == 0) pads = try { api.pads() } catch (e: Exception) { emptyList() }
            n++
            delay(250)
        }
    }
    fun start(what: String, port: Int? = null, name: String? = null) = scope.launch {
        try { st = api.memcard(what, port, name); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    CardsContent(st, pads, error, pick,
        onLoad = { start("prepare") },
        onBackup = { start("backup", it) },
        onAnalog = { scope.launch { pads.filter { it.connected && it.id == 0x41 }.forEach { p -> try { api.padAnalog(p.port) } catch (_: Exception) {} }; pads = try { api.pads() } catch (e: Exception) { pads } } },
        onPick = { pick = it },
        onRestore = { port -> pick?.let { start("restore", port, it.name) }; pick = null },
        onBack = { pick = null })
}

@Composable
fun BackupsRoute(model: AppModel) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var st by remember { mutableStateOf<BackupState?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var preview by remember { mutableStateOf<Snapshot?>(null) }
    var files by remember { mutableStateOf(listOf<String>()) }
    var seen by remember { mutableStateOf(-1L) }
    fun handle(s: BackupState, wasBusy: Boolean) {
        st = s
        if (!wasBusy || s.busy || s.at == seen) return
        seen = s.at
        when (s.job) {
            "preview" -> files = if (s.jobOk) s.output.lines().filter { it.isNotBlank() && !it.endsWith("/") && !it.startsWith("Would restore") } else emptyList()
            "now" -> Toast.makeText(ctx, if (s.jobOk) "Backed up" else "Backup failed", Toast.LENGTH_SHORT).show()
            "restore" -> { Toast.makeText(ctx, if (s.jobOk) "Restored" else "Restore failed: nothing changed", Toast.LENGTH_SHORT).show(); preview = null }
        }
    }
    LaunchedEffect(api.address) {
        while (true) {
            val was = st?.busy == true
            try { handle(api.backup(), was); error = null } catch (e: Exception) { error = model.handle(e) }
            delay(if (st?.busy == true) 800 else 5000)
        }
    }
    fun start(what: String, name: String? = null, only: String? = null) = scope.launch {
        try { st = api.backup(what, name, only); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    BackupsContent(st, error, preview, files,
        onNow = { start("now") },
        onPick = { preview = it; files = emptyList(); start("preview", it.name, "saves|savestates") },
        onRestoreSaves = { preview?.let { start("restore", it.name, "saves|savestates") } },
        onRestoreAll = { preview?.let { start("restore", it.name) } },
        onBack = { preview = null })
}

@Composable
fun CloudRoute(model: AppModel) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var st by remember { mutableStateOf<CloudState?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var mode by remember { mutableStateOf(CloudMode.MAIN) }
    var lines by remember { mutableStateOf(listOf<String>()) }
    var from by remember { mutableStateOf("") }
    var form by remember { mutableStateOf(AddForm()) }
    var seen by remember { mutableStateOf(-1L) }
    fun toast(t: String) = Toast.makeText(ctx, t, Toast.LENGTH_SHORT).show()
    // a finished job: show what it found, or say how it went
    fun handle(s: CloudState, wasBusy: Boolean) {
        st = s
        if (!wasBusy || s.busy || s.at == seen) return
        seen = s.at
        val out = s.output.lines().filter { it.isNotBlank() }
        when (s.job) {
            "devices" -> if (s.jobOk) { lines = out; mode = CloudMode.DEVICES }
            "preview" -> { lines = out; mode = CloudMode.PREVIEW }
            "add" -> if (s.jobOk) scope.launch {
                val f = form
                val extra = if (f.kind == "smb") mapOf<String, Any>("name" to f.kind, "folder" to "${f.share}/Phosphor") else mapOf<String, Any>("name" to f.kind)
                runCatching { api.cloud("use", extra) }
                    .onSuccess { st = it; mode = CloudMode.MAIN; form = AddForm() }
                    .onFailure { error = model.handle(it) }
            } else toast(out.firstOrNull() ?: "Couldn't add it")
            "push" -> toast(if (s.jobOk) "Saves sent" else out.lastOrNull() ?: "Sending failed")
            "pull" -> { toast(if (s.jobOk) "Saves are back" else "Couldn't get them back"); mode = CloudMode.MAIN }
            "test" -> toast(if (s.jobOk) "Connection OK" else out.lastOrNull() ?: "Couldn't reach it")
            "setup" -> toast(if (s.jobOk) "rclone is ready" else out.lastOrNull() ?: "Download failed")
            "use" -> if (!s.jobOk) toast(out.lastOrNull() ?: "Couldn't choose it")
        }
    }
    LaunchedEffect(api.address) {
        while (true) {
            val was = st?.busy == true
            try { handle(api.cloud(), was); error = null } catch (e: Exception) { error = model.handle(e) }
            delay(if (st?.busy == true) 1000 else 5000)
        }
    }
    fun start(what: String, extra: Map<String, Any> = emptyMap()) = scope.launch {
        try { st = api.cloud(what, extra); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    CloudContent(st, error, mode, lines, from, form,
        onSend = { start("push") }, onGetBack = { start("devices") },
        onDevice = { from = it; start("preview", mapOf("from" to it)) },
        onConfirm = { start("pull", mapOf("from" to from)) },
        onUse = { start("use", mapOf("name" to it)) },
        onAdd = { mode = CloudMode.ADD }, onForm = { form = it },
        onSubmit = {
            val f = form
            val settings: Map<String, String> = when (f.kind) {
                "drive", "dropbox", "onedrive" -> mapOf("token" to f.token.trim())
                "webdav" -> mapOf("url" to f.address.trim(), "vendor" to "other", "user" to f.user, "pass" to f.password)
                else -> mapOf("host" to f.address.trim(), "user" to f.user, "pass" to f.password)
            }.filterValues { it.isNotEmpty() }
            start("add", mapOf("name" to f.kind, "type" to f.kind, "settings" to settings))
        },
        onSetup = { start("setup") }, onTest = { start("test") },
        onBack = { mode = CloudMode.MAIN })
}

// ---------- Video ----------

@Composable
fun VideoListRoute(model: AppModel, nav: NavHostController) {
    val api = model.api ?: return
    var list by remember { mutableStateOf<List<VideoSys>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address) { try { list = api.videoSystems(); error = null } catch (e: Exception) { error = model.handle(e) } }
    VideoListContent(list, error, onPick = { nav.navigate("video/${Uri.encode(it.system)}") }, onBack = {})
}

@Composable
fun VideoSysRoute(model: AppModel, nav: NavHostController, sys: String) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var v by remember { mutableStateOf<VideoSys?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var picking by remember { mutableStateOf<VideoSlot?>(null) }
    var choices by remember { mutableStateOf<List<String>?>(null) }
    LaunchedEffect(sys) { try { v = api.videoSystems().firstOrNull { it.system == sys }; error = if (v == null) "No core installed for that system" else null } catch (e: Exception) { error = model.handle(e) } }
    fun open(slot: VideoSlot) {
        picking = slot; choices = null
        scope.launch {
            choices = try {
                if (slot.what == "maskmode") listOf("1x", "2x", "1x rotated", "2x rotated") else api.videoFiles(slot.kind)
            } catch (e: Exception) { error = model.handle(e); emptyList() }
        }
    }
    fun change(block: suspend () -> VideoSys, done: String) = scope.launch {
        try { v = block(); error = null; Toast.makeText(ctx, done, Toast.LENGTH_SHORT).show() } catch (e: Exception) { error = model.handle(e) }
    }
    VideoSysContent(v, error, picking, choices,
        onSlot = { open(it) },
        onPresets = { open(VideoSlot("Presets", "preset", "presets")) },
        onChoose = { c ->
            val p = picking ?: return@VideoSysContent
            picking = null
            if (p.what == "preset") { if (c != null) change({ api.video(sys, preset = c) }, "Preset applied") }
            else change({ api.video(sys, p.what, c) }, "Saved: shows next time the core starts")
        },
        onUndo = { change({ api.video(sys, undo = true) }, "Put back") },
        onBack = { if (picking != null) picking = null else nav.popBackStack() })
}

// ---------- Box art ----------

@Composable
fun BoxArtRoute(model: AppModel, t: MutableTabs) = Tabs(BOXART_TABS, t) {
    val api = model.api ?: return@Tabs
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var error by remember { mutableStateOf<String?>(null) }
    if (t.tab == 0) {
        var ss1 by remember { mutableStateOf<ScrapeProgress?>(null) }
        LaunchedEffect(api.address) {
            while (true) {
                try { ss1 = api.scrapeProgress(); error = null } catch (e: Exception) { error = model.handle(e) }
                delay(if (ss1?.running == true) 2000 else 6000)
            }
        }
        val askNotify = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ScrapeService.start(ctx) }
        BoxArtLookupContent(PhoneScrape.state, ss1, error,
            onPhone = {
                if (PhoneScrape.state.running) ScrapeService.stop(ctx)
                else if (Build.VERSION.SDK_INT >= 33 && !Notes.canPost(ctx)) askNotify.launch(Manifest.permission.POST_NOTIFICATIONS)
                else ScrapeService.start(ctx)
            },
            onSs1 = { scope.launch { try { api.scrapeOn(); Prefs.putBool("ss1_scraping", true); Toast.makeText(ctx, "Looking up box art on the MiSTer (slower)", Toast.LENGTH_SHORT).show(); ss1 = api.scrapeProgress() } catch (e: Exception) { error = model.handle(e) } } })
    } else {
        var sources by remember { mutableStateOf<List<ScrapeSource>?>(null) }
        var accounts by remember { mutableStateOf(listOf<ScrapeAccount>()) }
        var selected by remember { mutableIntStateOf(0) }
        val values = remember { mutableStateMapOf<String, String>() }
        var busy by remember { mutableStateOf(false) }
        fun fill() {
            values.clear()
            val x = sources?.getOrNull(selected) ?: return
            // user names and ids are shown; secrets stay empty (type to change)
            SOURCE_FIELDS[x.id]?.forEach { (k, _, secret) -> values[k] = if (secret) "" else accounts.firstOrNull { it.sec == x.id && it.key == k }?.value ?: "" }
        }
        LaunchedEffect(api.address) {
            try { sources = api.scrapeSources(); accounts = api.scrapeAccounts(); error = null; fill() } catch (e: Exception) { error = model.handle(e) }
        }
        fun save(list: List<ScrapeSource>, sel: Int) = scope.launch {
            try { sources = api.scrapeSources(list.map { it.id }, list.filter { !it.on }.map { it.id }); selected = sel; fill(); error = null } catch (e: Exception) { error = model.handle(e) }
        }
        BoxArtSourcesContent(sources, accounts, selected, values, error, busy,
            onSelect = { selected = it; fill() },
            onToggle = { i -> sources?.let { l -> save(l.mapIndexed { j, x -> if (j == i) x.copy(on = !x.on) else x }, i) } },
            onMove = { i, d -> sources?.let { l -> val j = i + d; if (j in l.indices) save(l.toMutableList().apply { val a = this[i]; this[i] = this[j]; this[j] = a }, j) } },
            onValue = { k, v -> values[k] = v },
            onSave = {
                val x = sources?.getOrNull(selected) ?: return@BoxArtSourcesContent
                scope.launch {
                    busy = true
                    try {
                        SOURCE_FIELDS[x.id]?.forEach { (k, _, secret) ->
                            val v = values[k] ?: ""
                            if (secret && v.isEmpty()) return@forEach   // empty: keep what's saved
                            accounts = api.scrapeAccounts(x.id, k, v)
                        }
                        sources = api.scrapeSources(); fill(); error = null
                        Toast.makeText(ctx, "Saved on the MiSTer", Toast.LENGTH_SHORT).show()
                    } catch (e: Exception) { error = model.handle(e) }
                    busy = false
                }
            })
    }
}

// ---------- Power & updates ----------

@Composable
fun PowerRoute(model: AppModel, t: MutableTabs) = Tabs(POWER_TABS, t) {
    val api = model.api ?: return@Tabs
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var error by remember { mutableStateOf<String?>(null) }
    fun toast(s: String) = Toast.makeText(ctx, s, Toast.LENGTH_LONG).show()
    when (t.tab) {
        0 -> {
            var p by remember { mutableStateOf<PowerState?>(null) }
            LaunchedEffect(api.address) { try { p = api.power(); error = null } catch (e: Exception) { error = model.handle(e) } }
            PowerContent(p, error,
                onAck = { scope.launch { try { p = api.power(ack = true) } catch (e: Exception) { error = model.handle(e) } } },
                onSystem = { what ->
                    scope.launch {
                        try {
                            api.system(what)
                            toast(when (what) { "reboot" -> "Restarting the MiSTer..."; "shutdown" -> "Shutting down: the TV says when it's safe to switch off"; else -> "Reloading the TV screen..." })
                        } catch (e: Exception) { error = model.handle(e) }
                    }
                })
        }
        1 -> {
            var u by remember { mutableStateOf<UpdateState?>(null) }
            LaunchedEffect(api.address) {
                while (true) {
                    try { u = api.update(); error = null } catch (e: Exception) { error = model.handle(e) }
                    delay(if (u?.state == "working") 2000 else 8000)
                }
            }
            UpdateContent(u, error,
                onInstall = { f -> scope.launch { try { u = api.update("install", f); toast("Updating: the TV closes Phosphor and opens it again") } catch (e: Exception) { error = model.handle(e) } } },
                onRollback = { v -> scope.launch { try { u = api.update("rollback", v); toast("Rolling back to Phosphor $v") } catch (e: Exception) { error = model.handle(e) } } })
        }
        else -> {
            var g by remember { mutableStateOf<GuardState?>(null) }
            LaunchedEffect(api.address) {
                while (true) {
                    try { g = api.guard(); error = null } catch (e: Exception) { error = model.handle(e) }
                    delay(if (g?.job?.busy == true) 2000 else 8000)
                }
            }
            GuardContent(g, error,
                onRestore = {
                    val reset = g?.files?.filter { it.reset }?.map { it.path } ?: emptyList()
                    val items = reset.filter { !Regex("dhcpcd|hostname|/etc/hosts|network|wpa_").containsMatchIn(it) }
                    if (items.isEmpty()) toast("Only network settings were reset: put them back on the MiSTer (Settings > Updates)")
                    else scope.launch { try { g = api.guard("restore", items); toast("Putting your settings back") } catch (e: Exception) { error = model.handle(e) } }
                },
                onKeep = { scope.launch { try { g = api.guard("keep") } catch (e: Exception) { error = model.handle(e) } } },
                onRollback = { name -> scope.launch { try { g = api.guard("rollback", listOf(name)); toast("Rolling back...") } catch (e: Exception) { error = model.handle(e) } } })
        }
    }
}

// ---------- Storage ----------

private fun dirSize(f: File): Long = if (f.isDirectory) f.listFiles().orEmpty().sumOf { dirSize(it) } else f.length()

@Composable
fun StorageRoute(model: AppModel, t: MutableTabs) = Tabs(STORAGE_TABS, t) {
    val api = model.api ?: return@Tabs
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var error by remember { mutableStateOf<String?>(null) }
    if (t.tab < 2) {
        var s by remember { mutableStateOf<StorageState?>(null) }
        LaunchedEffect(api.address) {
            while (true) {
                try { s = api.storage(); error = null } catch (e: Exception) { error = model.handle(e) }
                delay(if (s?.last?.busy == true) 2000 else 10000)
            }
        }
        StorageDrivesContent(s, t.tab == 0, error,
            onCheck = { test, target -> scope.launch { try { s = api.storage("check", test, target); Toast.makeText(ctx, "Checking...", Toast.LENGTH_SHORT).show() } catch (e: Exception) { error = model.handle(e) } } },
            onMount = { target -> scope.launch { try { s = api.storage("mount", target = target); Toast.makeText(ctx, "Mounting...", Toast.LENGTH_SHORT).show() } catch (e: Exception) { error = model.handle(e) } } })
    } else {
        val lists = File(ctx.cacheDir, "boxart-lists")
        var busy by remember { mutableStateOf(false) }
        var listsBytes by remember { mutableStateOf(0L) }
        val ch = ArtStore.changes
        val art = remember(ch) { ArtStore.list() }
        val artBytes = remember(ch) { ArtStore.size() }
        LaunchedEffect(ch, busy) { listsBytes = dirSize(lists) }
        PhoneStorageContent(art, artBytes, listsBytes, Prefs.autoDelete, busy || PhoneScrape.state.running,
            onAuto = { Prefs.chooseAutoDelete(it) },
            onSend = { scope.launch { busy = true; val n = ArtStore.sendWaiting(api); Toast.makeText(ctx, "$n sent", Toast.LENGTH_SHORT).show(); busy = false } },
            onDeleteArt = { ArtStore.deleteAll() },
            onDeleteLists = { busy = true; lists.deleteRecursively(); busy = false })
    }
}

// ---------- Bluetooth, Diagnostics, Notifications, Connection ----------

@Composable
fun DiagRoute(model: AppModel) {
    val api = model.api ?: return
    val scope = rememberCoroutineScope()
    var st by remember { mutableStateOf<DiagState?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address) {
        while (true) {
            try { st = api.diag(); error = null } catch (e: Exception) { error = model.handle(e) }
            delay(if (st?.busy == true) 1500 else 10000)
        }
    }
    DiagContent(st, error, onRun = { scope.launch { try { st = api.diag(run = true) } catch (e: Exception) { error = model.handle(e) } } }, onBack = {})
}

@Composable
fun BluetoothRoute(model: AppModel) {
    val api = model.api ?: return
    var st by remember { mutableStateOf<BtState?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address) {
        while (true) {
            try { st = api.bluetooth(); error = null } catch (e: Exception) { error = model.handle(e) }
            delay(if (st?.busy == true) 800 else 3000)
        }
    }
    BluetoothContent(st, error, onBack = {})
}

@Composable
fun NotesRoute(model: AppModel, nav: NavHostController) {
    val ctx = LocalContext.current
    var allowed by remember { mutableStateOf(Notes.canPost(ctx)) }
    val ask = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { allowed = it; Notes.schedule(ctx) }
    val notes = Notes.list
    LaunchedEffect(Unit) { delay(1500); Notes.markSeen(ctx) }
    NotesContent(notes, Prefs.notify, allowed,
        onNotify = { on -> Prefs.chooseNotify(on); if (on) Notes.schedule(ctx) else Notes.unschedule(ctx) },
        onAllow = { if (Build.VERSION.SDK_INT >= 33) ask.launch(Manifest.permission.POST_NOTIFICATIONS) },
        onOpen = { n -> nav.navigate(goRoute(n.go)) { launchSingleTop = true } },
        onClear = { Notes.clear(ctx) })
}

@Composable
fun ConnectionRoute(model: AppModel) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var searching by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    val p = model.pairing ?: return
    ConnectionContent(p.address, model.reachable, model.version, searching, message,
        onFind = {
            scope.launch {
                searching = true; message = null
                val ip = localIPv4(ctx)
                if (ip == null) message = "Connect the phone to WiFi (the same network as the MiSTer)"
                else {
                    // the one that knows this phone's key is ours
                    val found = runCatching { PhosphorApi.discover(ip) }.getOrDefault(emptyList())
                    val mine = found.firstOrNull { f -> runCatching { PhosphorApi(f.address, p.token).cmd(org.json.JSONObject().put("cmd", "status")); true }.getOrDefault(false) }
                    message = when {
                        mine == null && found.isEmpty() -> "No MiSTer with Remote access found on this WiFi"
                        mine == null -> "Found ${found.size}, but none knows this phone: pair again"
                        mine.address == p.address -> "It's at the same address"
                        else -> { model.moveTo(mine.address); "Found it at ${mine.address}" }
                    }
                }
                searching = false
            }
        },
        onForget = { model.forget() })
}

@Composable
fun ConnectRoute(model: AppModel, onPaired: () -> Unit) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var found by remember { mutableStateOf(listOf<Info>()) }
    var searching by remember { mutableStateOf(false) }
    var address by remember { mutableStateOf("") }
    var code by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    var round by remember { mutableIntStateOf(0) }
    LaunchedEffect(round) {
        val ip = localIPv4(ctx)
        if (ip == null) { error = "Connect the phone to WiFi (the same network as the MiSTer)"; return@LaunchedEffect }
        searching = true
        found = runCatching { PhosphorApi.discover(ip) }.getOrDefault(emptyList())
        searching = false
        if (address.isBlank() && found.size == 1) address = found[0].address
    }
    ConnectContent(found, searching, address, code, busy, error,
        onAddress = { address = it; error = null }, onCode = { code = it; error = null },
        onSearch = { round++ }, onPick = { address = it.address },
        onPair = {
            scope.launch {
                busy = true
                error = null
                try { model.pair(address, code); onPaired() } catch (e: Exception) { error = e.message }
                busy = false
            }
        })
}

// ---------- Home and Library ----------

@Composable
fun HomeRoute(model: AppModel, nav: NavHostController) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf<Status?>(null) }
    var last by remember { mutableStateOf<Game?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address, model.changes) {
        while (true) {
            try {
                status = api.status()
                last = runCatching { api.last() }.getOrNull()
                error = null
            } catch (e: Exception) { error = model.handle(e) }
            delay(if (status?.let { it.indexing || it.identifying || it.scraperRunning } == true) 3000 else 10000)
        }
    }
    val cover = last?.let { g -> if (g.hasArt) rememberArt(api, g.id, model.changes) else null }
    HomeContent(status, last, cover, error,
        onPlayLast = { last?.let { g -> scope.launch { try { api.launch(g.id); Toast.makeText(ctx, "Starting on the TV", Toast.LENGTH_SHORT).show() } catch (e: Exception) { error = model.handle(e) } } } },
        onOpenLast = { last?.let { nav.navigate("game/${it.id}") } },
        onFilter = { f, t -> nav.navigate("games?title=${enc(t)}&filter=$f") },
        onLibrary = { nav.openSection(Section.LIBRARY) })
}

@Composable
fun LibraryRoute(model: AppModel, nav: NavHostController) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var cats by remember { mutableStateOf(listOf<Category>()) }
    var cat by rememberSaveable { mutableIntStateOf(0) }
    var systems by remember { mutableStateOf<List<SystemItem>?>(null) }
    var query by rememberSaveable { mutableStateOf("") }
    var results by remember { mutableStateOf<List<GameItem>?>(null) }
    var total by remember { mutableIntStateOf(0) }
    var status by remember { mutableStateOf<Status?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address, model.changes) {
        var gen = -1
        while (true) {
            try {
                val s = api.status()
                status = s
                if (s.generation != gen) { cats = api.categories().filter { it.games > 0 }; gen = s.generation; systems = api.systemsIn(cats.getOrNull(cat - 1)?.id) }
                error = null
            } catch (e: Exception) { error = model.handle(e) }
            delay(if (status?.let { it.indexing || it.identifying } == true) 3000 else 15000)
        }
    }
    LaunchedEffect(cat, cats) {
        if (cats.isEmpty() && cat > 0) return@LaunchedEffect
        try { systems = api.systemsIn(cats.getOrNull(cat - 1)?.id) } catch (e: Exception) { error = model.handle(e) }
    }
    LaunchedEffect(query) {
        if (query.isBlank()) { results = null; return@LaunchedEffect }
        delay(300)
        try { val (t, l) = api.games(q = query); total = t; results = l } catch (e: Exception) { error = model.handle(e) }
    }
    LibraryContent(cats, cat, systems, query, results, total, status, error,
        art = { g -> if (g.art) rememberArt(api, g.id, model.changes) else null },
        onCat = { cat = it; systems = null },
        onSystem = { nav.navigate("games?system=${enc(it.id)}&title=${enc(it.name)}") },
        onQuery = { query = it },
        onGame = { nav.navigate("game/${it.id}") },
        onRescan = { scope.launch { try { api.rescan(); Toast.makeText(ctx, "Looking for games", Toast.LENGTH_SHORT).show(); status = api.status() } catch (e: Exception) { error = model.handle(e) } } })
}

@Composable
fun rememberArt(api: PhosphorApi, id: Long, version: Int): ImageBitmap? {
    val v by produceState(ArtCache.peek("$id@$version"), api.address, id, version) { value = ArtCache.get(api, id, version) }
    return v
}

@Composable
fun GamesRoute(model: AppModel, nav: NavHostController, system: String?, title: String, startFilter: String?, search: Boolean) {
    val api = model.api ?: return
    var filter by remember { mutableStateOf(startFilter) }
    var query by remember { mutableStateOf("") }
    var games by remember { mutableStateOf<List<GameItem>?>(null) }
    var total by remember { mutableIntStateOf(0) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(filter, query, model.changes) {
        if (query.isNotEmpty()) delay(300)
        if (system == null && filter == null && query.isBlank()) { games = emptyList(); total = 0; return@LaunchedEffect }
        try {
            val (t, list) = api.games(system, filter, query)
            total = t
            games = list
            error = null
        } catch (e: Exception) { error = model.handle(e) }
    }
    GamesContent(title, system == null, filter, query, total, games, error,
        art = { g -> if (g.art) rememberArt(api, g.id, model.changes) else null },
        onFilter = { filter = it; games = null }, onQuery = { query = it },
        onGame = { nav.navigate("game/${it.id}") }, onBack = { nav.popBackStack() })
}

@Composable
fun GameRoute(model: AppModel, nav: NavHostController, id: Long) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var game by remember { mutableStateOf<Game?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf<String?>(null) }
    var artVersion by remember { mutableIntStateOf(0) }
    LaunchedEffect(id, model.changes) {
        try { game = api.game(id); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    val cover = if (game?.hasArt == true) rememberArt(api, id, model.changes * 1000 + artVersion) else null
    fun act(what: String, block: suspend () -> Unit) = scope.launch {
        busy = what
        error = null
        try { block() } catch (e: Exception) { error = model.handle(e) }
        busy = null
    }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) act("Sending the picture...") {
            val bytes = loadPhotoForUpload(ctx, uri)
            game = api.setArt(id, bytes)
            ArtCache.drop(id)
            artVersion++
            model.changes++
            Toast.makeText(ctx, "Box art changed", Toast.LENGTH_SHORT).show()
        }
    }
    GameContent(game, cover, error, busy,
        onBack = { nav.popBackStack() },
        onLaunch = { act("Starting it on the TV...") { api.launch(id); Toast.makeText(ctx, "Starting on the TV", Toast.LENGTH_SHORT).show() } },
        onFavorite = { game?.let { g -> act("") { api.favorite(id, !g.favorite); game = api.game(id); model.changes++ } } },
        onEdit = { nav.navigate("edit/$id") },
        onArt = { picker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)) },
        onBasedOn = { nav.navigate("based/$id") },
        onRescrape = { act("Asking the MiSTer to look it up again...") { api.rescrape(id); Toast.makeText(ctx, "Queued: it fills only empty fields", Toast.LENGTH_SHORT).show() } })
}

@Composable
fun EditRoute(model: AppModel, nav: NavHostController, id: Long) {
    val api = model.api ?: return
    val scope = rememberCoroutineScope()
    val values = remember { mutableStateMapOf<String, String>() }
    var original by remember { mutableStateOf<Map<String, String>>(emptyMap()) }
    var title by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(id) {
        try {
            val g = api.game(id)
            title = g.title
            original = gameValues(g)
            values.putAll(original)
        } catch (e: Exception) { error = model.handle(e) }
    }
    EditContent(title, values, busy, error, onChange = { k, v -> values[k] = v }, onBack = { nav.popBackStack() }, onSave = {
        val changed = values.filter { (k, v) -> original[k] != v }.mapValues { it.value.trim() }
        if (changed.isEmpty()) { nav.popBackStack(); return@EditContent }
        scope.launch {
            busy = true
            try { api.edit(id, changed); model.changes++; nav.popBackStack() } catch (e: Exception) { error = model.handle(e) }
            busy = false
        }
    })
}

@Composable
fun BasedRoute(model: AppModel, nav: NavHostController, id: Long) {
    val api = model.api ?: return
    val scope = rememberCoroutineScope()
    var game by remember { mutableStateOf<Game?>(null) }
    var query by remember { mutableStateOf("") }
    var results by remember { mutableStateOf<List<Candidate>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(id) { try { game = api.game(id) } catch (e: Exception) { error = model.handle(e) } }
    LaunchedEffect(query) {
        if (query.isNotEmpty()) delay(300)
        try { results = api.candidates(id, query); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    BasedOnContent(game?.title ?: "", game?.basedOnTitle, query, results, error,
        onQuery = { query = it },
        onPick = { c -> scope.launch { try { api.link(id, c.id); model.changes++; nav.popBackStack() } catch (e: Exception) { error = model.handle(e) } } },
        onUnlink = { scope.launch { try { api.unlink(id); model.changes++; nav.popBackStack() } catch (e: Exception) { error = model.handle(e) } } },
        onBack = { nav.popBackStack() })
}
