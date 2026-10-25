// Phosphor for Android: app state (which MiSTer, its key, does it answer), shared by the page and the
// background work. Changes are told to the page (MainActivity sends them on as the "state" event).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.ConnectivityManager
import android.net.Uri
import android.os.Build
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.net.Inet4Address

class AppModel(private val store: PairingStore) {
    var pairing: Pairing? = store.load()
        private set
    val api: PhosphorApi? get() = pairing?.let { PhosphorApi(it.address, it.token) }

    /** does the MiSTer answer (null: not known yet), and its Phosphor version; for the connection pill */
    var reachable: Boolean? = null
        private set
    var version = ""
        private set

    /** told whenever any of the above changes */
    var onChange: () -> Unit = {}

    private var watching: Job? = null

    private fun changed() = onChange()

    /** while the app is open: is the MiSTer there and its look (every 30 s), and the notifications check (every minute) */
    fun watch(scope: CoroutineScope, context: Context) {
        if (watching?.isActive == true) return
        val app = context.applicationContext
        watching = scope.launch {
            var n = 0
            while (true) {
                val a = api
                if (a != null) {
                    val ok = runCatching { a.ping() }.getOrDefault(false)
                    var tell = ok != reachable
                    reachable = ok
                    if (ok) {
                        runCatching { a.cmd(JSONObject().put("cmd", "status")).optString("version") }.getOrNull()?.let { if (it != version) { version = it; tell = true } }
                        runCatching { a.cmd(JSONObject().put("cmd", "look")) }.getOrNull()?.let {
                            val before = Prefs.accent to Prefs.texture
                            Prefs.seeLook(it.optString("accent"), it.optInt("texture"))
                            if (before != (Prefs.accent to Prefs.texture)) tell = true
                        }
                        if (n % 2 == 0 && Prefs.notify) runCatching { Notes.check(app, a) }
                        if (SysBackup.due()) launch { SysBackup.save(app, a) }      // MiSTer & Linux backups: what changed
                    }
                    if (tell) changed()
                } else if (reachable != null) { reachable = null; changed() }
                n++
                delay(30_000)
            }
        }
    }

    /** the MiSTer moved (the router gave it a new address): the same key, a new address */
    fun moveTo(address: String) {
        val p = pairing ?: return
        val q = Pairing(PhosphorApi.normalize(address), p.token)
        store.save(q)
        pairing = q
        reachable = null
        changed()
    }

    suspend fun pair(address: String, code: String, deviceName: String = Build.MODEL ?: "Android phone") {
        val a = PhosphorApi.normalize(address)
        val (token, _) = PhosphorApi(a, null).pair(code.filter { it.isDigit() }, deviceName)
        val p = Pairing(a, token)
        store.save(p)
        pairing = p
        reachable = true
        changed()
    }

    fun forget() {
        store.clear()
        pairing = null
        version = ""
        reachable = null
        changed()
    }

    /** a 401 means the MiSTer forgot this phone: back to pairing */
    fun handle(e: Throwable): String {
        if (e is PhosphorError && e.code == 401) forget()
        if (e is PhosphorError && e.code == 0 && reachable != false) { reachable = false; changed() }
        if (e is PhosphorError && e.code != 0 && reachable != true) { reachable = true; changed() }
        return e.message ?: "Something went wrong"
    }

    /** it answered: the pill can say so straight away */
    fun answered() { if (reachable != true) { reachable = true; changed() } }
}

/** the phone's IPv4 address on WiFi/Ethernet, for finding the MiSTer */
fun localIPv4(context: Context): String? {
    val cm = context.getSystemService(ConnectivityManager::class.java) ?: return null
    val lp = cm.getLinkProperties(cm.activeNetwork) ?: return null
    return lp.linkAddresses.map { it.address }.filterIsInstance<Inet4Address>().firstOrNull { !it.isLoopbackAddress }?.hostAddress
}

/** a picked photo, made small enough to send quickly: about 1200 px on the long side, JPEG */
fun loadPhotoForUpload(context: Context, uri: Uri): ByteArray {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
    var sample = 1
    while (maxOf(bounds.outWidth, bounds.outHeight) / (sample * 2) >= 1200) sample *= 2
    val bmp = context.contentResolver.openInputStream(uri)?.use {
        BitmapFactory.decodeStream(it, null, BitmapFactory.Options().apply { inSampleSize = sample })
    } ?: throw PhosphorError("Couldn't read that picture")
    val out = ByteArrayOutputStream()
    bmp.compress(Bitmap.CompressFormat.JPEG, 92, out)
    return out.toByteArray()
}
