// Phosphor for Android: navigation and the loaders that connect the screens to the MiSTer.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
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
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.f3bandit.phosphor.ui.AddForm
import com.f3bandit.phosphor.ui.ArtCache
import com.f3bandit.phosphor.ui.BackupsContent
import com.f3bandit.phosphor.ui.CardsContent
import com.f3bandit.phosphor.ui.BluetoothContent
import com.f3bandit.phosphor.ui.CloudContent
import com.f3bandit.phosphor.ui.CloudMode
import com.f3bandit.phosphor.ui.BasedOnContent
import com.f3bandit.phosphor.ui.ConnectContent
import com.f3bandit.phosphor.ui.EditContent
import com.f3bandit.phosphor.ui.GameContent
import com.f3bandit.phosphor.ui.GamesContent
import com.f3bandit.phosphor.ui.HomeContent
import com.f3bandit.phosphor.ui.PhosphorTheme
import com.f3bandit.phosphor.ui.SettingsContent
import com.f3bandit.phosphor.ui.gameValues
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val store = KeystorePairingStore(applicationContext)
        setContent {
            PhosphorTheme {
                val model: AppModel = viewModel(factory = object : ViewModelProvider.Factory {
                    @Suppress("UNCHECKED_CAST")
                    override fun <T : ViewModel> create(modelClass: Class<T>): T = AppModel(store) as T
                })
                PhosphorApp(model)
            }
        }
    }
}

private fun enc(s: String) = Uri.encode(s)

@Composable
fun PhosphorApp(model: AppModel) {
    val nav = rememberNavController()
    val paired = model.pairing != null
    LaunchedEffect(paired) {
        if (!paired) nav.navigate("connect") { popUpTo(0) }
    }
    NavHost(nav, startDestination = if (paired) "home" else "connect") {
        composable("connect") { ConnectRoute(model) { nav.navigate("home") { popUpTo(0) } } }
        composable("home") { HomeRoute(model, nav) }
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
        composable("settings") {
            SettingsContent(model.pairing?.address ?: "", BuildConfigVersion, onForget = { model.forget() }, onBack = { nav.popBackStack() },
                onCloud = { nav.navigate("cloud") }, onBackups = { nav.navigate("backups") }, onCards = { nav.navigate("cards") }, onBluetooth = { nav.navigate("bluetooth") })
        }
        composable("cloud") { CloudRoute(model, nav) }
        composable("backups") { BackupsRoute(model, nav) }
        composable("cards") { CardsRoute(model, nav) }
        composable("bluetooth") { BluetoothRoute(model, nav) }
    }
}

const val BuildConfigVersion = "0.5.0"

@Composable
fun BluetoothRoute(model: AppModel, nav: NavHostController) {
    val api = model.api ?: return
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var st by remember { mutableStateOf<BtState?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var pick by remember { mutableStateOf<BtDevice?>(null) }
    fun handle(s: BtState, wasBusy: Boolean) {
        st = s
        if (wasBusy && !s.busy && s.message.isNotEmpty()) Toast.makeText(ctx, s.message, Toast.LENGTH_LONG).show()
    }
    LaunchedEffect(api.address) {
        while (true) {
            val was = st?.busy == true
            try { handle(api.bluetooth(), was); error = null } catch (e: Exception) { error = model.handle(e) }
            delay(if (st?.busy == true) 800 else 3000)
        }
    }
    fun run(what: String, mac: String? = null) = scope.launch {
        try { st = api.bluetooth(what, mac); error = null } catch (e: Exception) { error = model.handle(e) }
    }
    BluetoothContent(st, error, pick,
        onPair = { run("pair") }, onCancel = { run("cancel") }, onPick = { pick = it },
        onDisconnect = { pick?.let { run("disconnect", it.mac) }; pick = null },
        onForget = { pick?.let { run("forget", it.mac) }; pick = null },
        onBack = { if (pick != null) pick = null else nav.popBackStack() })
}

@Composable
fun CardsRoute(model: AppModel, nav: NavHostController) {
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
            // the card state every 2 s (busy: 0.8 s), the controllers 4 times a second
            if (n % 8 == 0 || (st?.busy == true && n % 3 == 0)) {
                val was = st?.busy == true
                try { handle(api.memcard(), was); error = null } catch (e: Exception) { error = model.handle(e) }
            }
            if (st?.available == true) pads = try { api.pads() } catch (e: Exception) { emptyList() }
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
        onAnalog = { scope.launch { pads.filter { it.connected && it.id == 0x41 }.forEach { p -> try { api.padAnalog(p.port) } catch (_: Exception) {} } } },
        onPick = { pick = it },
        onRestore = { port -> pick?.let { start("restore", port, it.name) }; pick = null },
        onBack = { if (pick != null) pick = null else nav.popBackStack() })
}

@Composable
fun BackupsRoute(model: AppModel, nav: NavHostController) {
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
        onBack = { if (preview != null) preview = null else nav.popBackStack() })
}

@Composable
fun CloudRoute(model: AppModel, nav: NavHostController) {
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
        onBack = { if (mode != CloudMode.MAIN) mode = CloudMode.MAIN else nav.popBackStack() })
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

@Composable
fun HomeRoute(model: AppModel, nav: NavHostController) {
    val api = model.api ?: return
    var status by remember { mutableStateOf<Status?>(null) }
    var systems by remember { mutableStateOf(listOf<SystemItem>()) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(api.address, model.changes) {
        var gen = -1
        while (true) {
            try {
                val s = api.status()
                status = s
                error = null
                if (s.generation != gen || systems.isEmpty()) { systems = api.systems(); gen = s.generation }
            } catch (e: Exception) {
                error = model.handle(e)
            }
            delay(if (status?.let { it.indexing || it.identifying || it.scraperRunning } == true) 3000 else 8000)
        }
    }
    HomeContent(api.address, status, systems, error,
        onFilter = { f, t -> nav.navigate("games?title=${enc(t)}&filter=$f") },
        onSystem = { nav.navigate("games?system=${enc(it.id)}&title=${enc(it.name)}") },
        onSearch = { nav.navigate("games?title=${enc("Search")}&search=1") },
        onSettings = { nav.navigate("settings") })
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
