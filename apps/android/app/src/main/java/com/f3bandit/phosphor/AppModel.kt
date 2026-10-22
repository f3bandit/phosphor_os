// Phosphor for Android: app state (which MiSTer, its key) shared by the screens.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.ConnectivityManager
import android.net.Uri
import android.os.Build
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import java.io.ByteArrayOutputStream
import java.net.Inet4Address

class AppModel(private val store: PairingStore) : ViewModel() {
    var pairing by mutableStateOf(store.load())
        private set
    val api: PhosphorApi? get() = pairing?.let { PhosphorApi(it.address, it.token) }

    /** bumps when something changed that lists should reload for */
    var changes by mutableStateOf(0)

    suspend fun pair(address: String, code: String, deviceName: String = Build.MODEL ?: "Android phone") {
        val a = PhosphorApi.normalize(address)
        val (token, _) = PhosphorApi(a, null).pair(code.filter { it.isDigit() }, deviceName)
        val p = Pairing(a, token)
        store.save(p)
        pairing = p
    }

    fun forget() {
        store.clear()
        pairing = null
    }

    /** a 401 means the MiSTer forgot this phone: back to pairing */
    fun handle(e: Throwable): String {
        if (e is PhosphorError && e.code == 401) forget()
        return e.message ?: "Something went wrong"
    }
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
