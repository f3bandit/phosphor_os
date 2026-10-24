// Phosphor for Android: the app's own settings (kept on the phone only): look, box art downloads,
// notifications. Nothing here is a secret; the MiSTer's key is in Store.kt.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.f3bandit.phosphor.ui.ACCENTS
import com.f3bandit.phosphor.ui.P

object Prefs {
    private var sp: SharedPreferences? = null

    /** Settings > Look: one of ACCENTS' names, and a texture number (the console's numbers) */
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
        P.accent = ACCENTS.getValue(accent)
    }

    fun chooseAccent(name: String) {
        if (name !in ACCENTS) return
        accent = name
        P.accent = ACCENTS.getValue(name)
        sp?.edit()?.putString("accent", name)?.apply()
    }

    fun chooseTexture(n: Int) { texture = n; sp?.edit()?.putInt("texture", n)?.apply() }
    fun chooseAutoDelete(on: Boolean) { autoDelete = on; sp?.edit()?.putBoolean("auto_delete", on)?.apply() }
    fun chooseNotify(on: Boolean) { notify = on; sp?.edit()?.putBoolean("notify", on)?.apply() }

    // small values the background check keeps between runs
    fun getString(k: String): String? = sp?.getString(k, null)
    fun putString(k: String, v: String?) { sp?.edit()?.apply { if (v == null) remove(k) else putString(k, v) }?.apply() }
    fun getBool(k: String) = sp?.getBoolean(k, false) ?: false
    fun putBool(k: String, v: Boolean) { sp?.edit()?.putBoolean(k, v)?.apply() }
}
