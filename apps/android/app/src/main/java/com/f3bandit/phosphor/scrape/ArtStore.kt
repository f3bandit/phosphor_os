// Phosphor for Android: box art the phone downloaded, kept in the app's own folder until the MiSTer has
// it. Once the MiSTer says it's stored, the picture is deleted (Settings > Storage > This phone: Auto
// delete, on unless turned off); one the MiSTer didn't get stays, so it can be sent again. Only this
// folder is ever touched: photos picked from the gallery are never deleted.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import com.f3bandit.phosphor.PhosphorApi
import com.f3bandit.phosphor.Prefs
import org.json.JSONObject
import java.io.File

data class StoredArt(val id: Long, val title: String, val system: String, val at: Long, val sent: Boolean, val bytes: Long)

object ArtStore {
    private var dir: File? = null
    /** bumps when the folder changes, for the screens */
    var changes by mutableIntStateOf(0)
        private set

    fun init(context: Context) { if (dir == null) dir = File(context.applicationContext.filesDir, "boxart").apply { mkdirs() } }

    private fun d() = dir ?: error("ArtStore.init first")
    private fun jpg(id: Long) = File(d(), "$id.jpg")
    private fun info(id: Long) = File(d(), "$id.json")

    @Synchronized
    fun save(id: Long, title: String, system: String, data: ByteArray) {
        jpg(id).writeBytes(data)
        info(id).writeText(JSONObject().put("title", title).put("system", system).put("at", System.currentTimeMillis()).put("sent", false).toString())
        changes++
    }

    /** the MiSTer has it: gone (auto delete), or kept as sent */
    @Synchronized
    fun sent(id: Long) {
        if (Prefs.autoDelete) { jpg(id).delete(); info(id).delete() }
        else runCatching { info(id).writeText(JSONObject(info(id).readText()).put("sent", true).toString()) }
        changes++
    }

    @Synchronized
    fun list(): List<StoredArt> = d().listFiles { f -> f.name.endsWith(".jpg") }.orEmpty().mapNotNull { f ->
        val id = f.name.removeSuffix(".jpg").toLongOrNull() ?: return@mapNotNull null
        val j = runCatching { JSONObject(info(id).readText()) }.getOrNull()
        StoredArt(id, j?.optString("title") ?: "Game $id", j?.optString("system") ?: "", j?.optLong("at") ?: f.lastModified(), j?.optBoolean("sent") ?: false, f.length())
    }.sortedByDescending { it.at }

    fun size(): Long = d().listFiles().orEmpty().sumOf { it.length() }

    @Synchronized
    fun deleteAll() { d().listFiles().orEmpty().forEach { it.delete() }; changes++ }

    /** the ones the MiSTer hasn't got, sent again; how many went */
    suspend fun sendWaiting(api: PhosphorApi): Int {
        var n = 0
        for (a in list().filter { !it.sent }) {
            val data = runCatching { jpg(a.id).readBytes() }.getOrNull() ?: continue
            try { api.setArt(a.id, data); sent(a.id); n++ } catch (e: Exception) { if (e is kotlinx.coroutines.CancellationException) throw e }
        }
        return n
    }
}
