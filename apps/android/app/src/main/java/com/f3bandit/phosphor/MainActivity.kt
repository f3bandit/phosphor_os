// Phosphor for Android: the screens are a web page (assets/web: SS1 Tool for Android's layout in
// Phosphor's look) in a WebView. The page asks the MiSTer for things through PhosNative.call (sent to
// phosphord's remote access with this phone's key, which the page never sees) and the phone for its
// own things (pairing, box art lookups on the phone, notifications, its storage) through PhosNative.app.
// Box art comes through art/<id> on the page's own address, fetched here with the key.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.util.LruCache
import android.view.View
import android.webkit.JavascriptInterface
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.snapshotFlow
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.webkit.WebViewAssetLoader
import com.f3bandit.phosphor.scrape.ArtStore
import com.f3bandit.phosphor.scrape.PhoneScrape
import com.f3bandit.phosphor.scrape.PhoneScrapeState
import com.f3bandit.phosphor.scrape.ScrapeService
import com.f3bandit.phosphor.scrape.AutoMatch
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.File

const val BuildConfigVersion = "0.9.1"

class MainActivity : ComponentActivity() {
    private lateinit var web: WebView
    private lateinit var model: AppModel
    private var ready = false
    private var pendingGo: String? = null

    /** box art for the page: a few dozen pictures kept while the app runs */
    private val art = object : LruCache<String, ByteArray>(8 shl 20) { override fun sizeOf(key: String, value: ByteArray) = value.size }
    private val noArt = HashSet<String>()

    private var picked: CompletableDeferred<Uri?>? = null
    private val picker = registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri -> picked?.complete(uri) }
    private var asked: CompletableDeferred<Boolean>? = null
    private val askNotify = registerForActivityResult(ActivityResultContracts.RequestPermission()) { ok -> asked?.complete(ok) }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Prefs.init(this)
        ArtStore.init(this)
        Notes.channels(this)
        model = AppModel(KeystorePairingStore(applicationContext))
        model.onChange = { runOnUiThread { event("state", stateJson()) } }
        SysBackup.onChange = { event("sysbk", SysBackup.state(this).toString()) }
        pendingGo = intent?.getStringExtra("go")

        // edge to edge: the page sits inside the system bars and the keyboard; the bars show the app bar's colour
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val root = FrameLayout(this).apply { setBackgroundColor(Color.rgb(0x14, 0x14, 0x17)) }
        web = WebView(this)
        web.setBackgroundColor(Color.rgb(0x0e, 0x0e, 0x10))
        root.addView(web, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        ViewCompat.setOnApplyWindowInsetsListener(root) { v, insets ->
            val b = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout() or WindowInsetsCompat.Type.ime())
            v.setPadding(b.left, b.top, b.right, b.bottom)
            WindowInsetsCompat.CONSUMED
        }
        setContentView(root)

        val loader = WebViewAssetLoader.Builder()
            .addPathHandler("/assets/web/art/") { path -> artResponse(path) }
            .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(this))
            .build()
        web.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            // the phone's text size, within limits the layout copes with (SS1 Tool for Android's)
            textZoom = (resources.configuration.fontScale * 100).toInt().coerceIn(85, 135)
        }
        web.overScrollMode = View.OVER_SCROLL_NEVER
        web.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? = loader.shouldInterceptRequest(request.url)
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                val u = request.url
                if (u.host == "appassets.androidplatform.net") return false
                runCatching { startActivity(Intent(Intent.ACTION_VIEW, u)) }   // links out open in the browser
                return true
            }
            override fun onPageFinished(view: WebView, url: String) { ready = true }
        }
        web.addJavascriptInterface(Bridge(), "PhosNative")
        web.loadUrl("https://appassets.androidplatform.net/assets/web/index.html")

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                web.evaluateJavascript("window.__phosBack ? window.__phosBack() : false") { r ->
                    if (r != "true") { isEnabled = false; onBackPressedDispatcher.onBackPressed(); isEnabled = true }
                }
            }
        })

        // the phone's box art lookup: its progress to the page
        lifecycleScope.launch { snapshotFlow { PhoneScrape.state }.distinctUntilChanged().collect { event("phone", phoneJson(it)) } }
        if (model.pairing != null) startWatching()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        intent.getStringExtra("go")?.let { event("go", JSONObject.quote(it)) }
    }

    private fun startWatching() {
        model.watch(lifecycleScope, this)
        if (Prefs.notify) {
            Notes.schedule(this)
            if (Build.VERSION.SDK_INT >= 33 && !Notes.canPost(this) && !Prefs.getBool("asked_notify")) {
                Prefs.putBool("asked_notify", true)
                askNotify.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }

    // ---------------------------------------------------------------- to the page

    private fun js(code: String) = runOnUiThread { web.evaluateJavascript(code, null) }
    private fun reply(id: String, ok: Boolean, payload: String) = js("window.__phos(${JSONObject.quote(id)},$ok,${JSONObject.quote(payload)})")
    private fun event(name: String, json: String) = js("window.__phosEvent && window.__phosEvent(${JSONObject.quote(name)},${JSONObject.quote(json)})")

    private fun stateJson(): String {
        val p = model.pairing
        return JSONObject()
            .put("paired", p != null).put("address", p?.address ?: "").put("reachable", model.reachable ?: JSONObject.NULL)
            .put("version", model.version).put("appVersion", BuildConfigVersion)
            .put("notesUnseen", Notes.unseen(this))
            .put("look", JSONObject().put("accent", Prefs.accent).put("texture", Prefs.texture))
            .apply { pendingGo?.let { put("go", it); pendingGo = null } }
            .toString()
    }

    private fun phoneJson(s: PhoneScrapeState) = JSONObject()
        .put("running", s.running).put("status", s.status).put("total", s.total).put("done", s.done).put("found", s.found)
        .put("notFound", s.notFound).put("later", s.later).put("game", s.game).put("system", s.system)
        .put("notes", JSONArray(s.notes)).put("started", s.started).put("ended", s.ended).put("matched", s.matched).toString()

    // ---------------------------------------------------------------- box art for the page

    private fun artResponse(path: String): WebResourceResponse {
        val id = path.substringBefore('?').substringBefore('/').toLongOrNull()
        val api = model.api
        if (id == null || api == null) return notFound()
        // the page adds ?v=n after a change; the loader passes the path only, so the cache is per id and cleared on a change
        val key = "$id"
        art.get(key)?.let { return WebResourceResponse("image/jpeg", null, ByteArrayInputStream(it)) }
        synchronized(noArt) { if (key in noArt) return notFound() }
        val bytes = runCatching { runBlocking { api.art(id) } }.getOrNull()
        if (bytes == null) { synchronized(noArt) { noArt.add(key) }; return notFound() }
        art.put(key, bytes)
        return WebResourceResponse("image/jpeg", null, ByteArrayInputStream(bytes))
    }

    private fun notFound() = WebResourceResponse("image/jpeg", null, 404, "Not found", emptyMap(), ByteArrayInputStream(ByteArray(0)))

    private fun forgetArt(id: Long) { art.remove("$id"); synchronized(noArt) { noArt.remove("$id") } }

    // ---------------------------------------------------------------- the page's calls

    private fun dirSize(f: File): Long = if (f.isDirectory) f.listFiles().orEmpty().sumOf { dirSize(it) } else f.length()

    inner class Bridge {
        /** a phosphord command */
        @JavascriptInterface
        fun call(id: String, json: String) {
            lifecycleScope.launch {
                val api = model.api
                if (api == null) { reply(id, false, "Not paired with a MiSTer"); return@launch }
                try {
                    val req = JSONObject(json)
                    val slow = req.optString("cmd") in setOf("launch", "system", "storage", "update")
                    val r = api.cmd(req, if (req.optString("cmd") == "scrape_todo") 60_000 else if (slow) 30_000 else 20_000)
                    model.answered()
                    if (req.optString("cmd") == "look") Prefs.seeLook(r.optString("accent"), r.optInt("texture"))
                    reply(id, true, r.toString())
                } catch (e: Exception) {
                    reply(id, false, model.handle(e))
                }
            }
        }

        /** something only the phone does */
        @JavascriptInterface
        fun app(id: String, action: String, json: String) {
            lifecycleScope.launch {
                try {
                    reply(id, true, appAction(action, runCatching { JSONObject(json) }.getOrDefault(JSONObject())))
                } catch (e: Exception) {
                    reply(id, false, model.handle(e))
                }
            }
        }
    }

    private suspend fun appAction(action: String, a: JSONObject): String {
        val ctx = this
        return when (action) {
            "state" -> stateJson()
            "look" -> { Prefs.seeLook(a.optString("accent"), a.optInt("texture")); "{}" }
            "discover" -> {
                val ip = localIPv4(ctx) ?: return JSONObject().put("found", JSONArray()).put("error", "Connect the phone to WiFi (the same network as the MiSTer)").toString()
                val found = runCatching { PhosphorApi.discover(ip) }.getOrDefault(emptyList())
                JSONObject().put("found", JSONArray(found.map { JSONObject().put("address", it.address).put("version", it.version) })).toString()
            }
            "pair" -> {
                model.pair(a.optString("address"), a.optString("code"))
                startWatching()
                "{}"
            }
            "forget" -> { Notes.unschedule(ctx); model.forget(); "{}" }
            "find" -> {
                val p = model.pairing ?: throw PhosphorError("Not paired")
                val ip = localIPv4(ctx)
                val msg = if (ip == null) "Connect the phone to WiFi (the same network as the MiSTer)" else {
                    // the one that knows this phone's key is ours
                    val found = runCatching { PhosphorApi.discover(ip) }.getOrDefault(emptyList())
                    val mine = found.firstOrNull { f -> runCatching { PhosphorApi(f.address, p.token).cmd(JSONObject().put("cmd", "status")); true }.getOrDefault(false) }
                    when {
                        mine == null && found.isEmpty() -> "No MiSTer with Remote access found on this WiFi"
                        mine == null -> "Found ${found.size}, but none knows this phone: pair again"
                        mine.address == p.address -> "It's at the same address"
                        else -> { model.moveTo(mine.address); "Found it at ${mine.address}" }
                    }
                }
                JSONObject().put("message", msg).toString()
            }
            "pickArt" -> {
                val gameId = a.optLong("id")
                val api = model.api ?: throw PhosphorError("Not paired")
                val d = CompletableDeferred<Uri?>()
                picked = d
                picker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
                val uri = d.await() ?: return "{}"
                val bytes = loadPhotoForUpload(ctx, uri)
                val r = api.setArtJson(gameId, bytes)
                forgetArt(gameId)
                JSONObject().put("game", r.getJSONObject("game")).toString()
            }
            "phoneScrape" -> {
                if (a.has("start")) {
                    if (!a.optBoolean("start")) ScrapeService.stop(ctx)
                    else {
                        if (Build.VERSION.SDK_INT >= 33 && !Notes.canPost(ctx)) {
                            val d = CompletableDeferred<Boolean>()
                            asked = d
                            askNotify.launch(Manifest.permission.POST_NOTIFICATIONS)
                            d.await()
                        }
                        ScrapeService.start(ctx, a.optBoolean("only"))
                    }
                }
                phoneJson(PhoneScrape.state)
            }
            // games matched to their base games (Match.kt): list, keep, undo
            "automatch" -> {
                AutoMatch.init(ctx)
                val id = a.optLong("id")
                when (a.optString("do")) {
                    "keep" -> AutoMatch.keep(id)
                    "keep_all" -> AutoMatch.keep(null)
                    "forget" -> AutoMatch.forget(id)
                    "undo" -> AutoMatch.undo(model.api ?: throw PhosphorError("Not paired"), id)
                }
                AutoMatch.list().toString()
            }
            "notGame" -> {
                val api = model.api ?: throw PhosphorError("Not paired")
                api.notGame(a.optLong("id"), a.optBoolean("on"))
                "{}"
            }
            "phoneStorage" -> {
                val lists = File(cacheDir, "boxart-lists")
                JSONObject()
                    .put("art", JSONArray(ArtStore.list().map { JSONObject().put("id", it.id).put("title", it.title).put("system", it.system).put("at", it.at).put("sent", it.sent).put("bytes", it.bytes) }))
                    .put("artBytes", ArtStore.size()).put("listsBytes", dirSize(lists)).put("autoDelete", Prefs.autoDelete).toString()
            }
            "setAutoDelete" -> { Prefs.chooseAutoDelete(a.optBoolean("on")); "{}" }
            "sendWaiting" -> {
                val api = model.api ?: throw PhosphorError("Not paired")
                JSONObject().put("sent", ArtStore.sendWaiting(api)).toString()
            }
            "deleteArt" -> { ArtStore.deleteAll(); "{}" }
            "deleteLists" -> { File(cacheDir, "boxart-lists").deleteRecursively(); "{}" }
            "sysbk" -> {
                val api = model.api ?: throw PhosphorError("Not paired")
                SysBackup.list(ctx, api).put("state", SysBackup.state(ctx)).toString()
            }
            "sysbkSave" -> {
                val api = model.api ?: throw PhosphorError("Not paired")
                lifecycleScope.launch { SysBackup.save(applicationContext, api) }
                SysBackup.state(ctx).toString()
            }
            "sysbkRestore" -> {
                val api = model.api ?: throw PhosphorError("Not paired")
                SysBackup.restore(ctx, api, a.optString("id"), a.optString("hash"))
                lifecycleScope.launch { SysBackup.save(applicationContext, api) }   // the file as it is now is a version too
                SysBackup.state(ctx).toString()
            }
            "notes" -> { Notes.unseen(ctx); JSONObject()
                .put("list", JSONArray(Notes.list.map { JSONObject().put("id", it.id).put("at", it.at).put("level", it.level).put("title", it.title).put("body", it.body).put("seen", it.seen).put("go", it.go) }))
                .put("notify", Prefs.notify).put("allowed", Notes.canPost(ctx)).toString() }
            "setNotify" -> {
                val on = a.optBoolean("on")
                Prefs.chooseNotify(on)
                if (on) Notes.schedule(ctx) else Notes.unschedule(ctx)
                "{}"
            }
            "allowNotify" -> {
                if (Build.VERSION.SDK_INT >= 33) {
                    val d = CompletableDeferred<Boolean>()
                    asked = d
                    askNotify.launch(Manifest.permission.POST_NOTIFICATIONS)
                    d.await()
                    Notes.schedule(ctx)
                }
                "{}"
            }
            "markSeen" -> { Notes.markSeen(ctx); event("state", stateJson()); "{}" }
            "clearNotes" -> { Notes.clear(ctx); event("state", stateJson()); "{}" }
            else -> throw PhosphorError("Unknown: $action")
        }
    }
}
