// Phosphor for Android: the app's own settings (kept on the phone only): the TV's look as last seen,
// box art downloads, notifications. Nothing here is a secret; the MiSTer's key is in Store.kt.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

object Prefs {
    private var sp: SharedPreferences? = null

    /** the TV's accent (amber, red, green, blue, purple) and texture number, as last seen: the app follows the TV */
    var accent by mutableStateOf("amber")
        private set
    var texture by mutableIntStateOf(0)
        private set
    /** Settings > Storage > This phone: delete a downloaded picture once the MiSTer has it (on unless turned off) */
    var autoDelete by mutableStateOf(true)
        private set
    /** Notifications: tell on the phone (Android notifications), not only in the app's list */
    var notify by mutableStateOf(true)
        private set

    fun init(context: Context) {
        if (sp != null) return
        val p = context.applicationContext.getSharedPreferences("settings", Context.MODE_PRIVATE)
        sp = p
        accent = p.getString("accent", "amber")!!.takeIf { it in ACCENTS } ?: "amber"
        texture = p.getInt("texture", 0)
        autoDelete = p.getBoolean("auto_delete", true)
        notify = p.getBoolean("notify", true)
    }

    val ACCENTS = setOf("amber", "red", "green", "blue", "purple")

    /** the TV's look, kept so the app opens in it next time */
    fun seeLook(name: String, n: Int) {
        if (name in ACCENTS && name != accent) { accent = name; sp?.edit()?.putString("accent", name)?.apply() }
        if (n != texture) { texture = n; sp?.edit()?.putInt("texture", n)?.apply() }
    }
    fun chooseAutoDelete(on: Boolean) { autoDelete = on; sp?.edit()?.putBoolean("auto_delete", on)?.apply() }
    fun chooseNotify(on: Boolean) { notify = on; sp?.edit()?.putBoolean("notify", on)?.apply() }

    // small values the background check keeps between runs
    fun getString(k: String): String? = sp?.getString(k, null)
    fun putString(k: String, v: String?) { sp?.edit()?.apply { if (v == null) remove(k) else putString(k, v) }?.apply() }
    fun getBool(k: String) = sp?.getBoolean(k, false) ?: false
    fun putBool(k: String, v: Boolean) { sp?.edit()?.putBoolean(k, v)?.apply() }
}
