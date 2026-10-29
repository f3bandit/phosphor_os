// Phosphor for Android: watching the MiSTer for things worth knowing, like Phosphor for Windows' tray
// alerts (alerts.go): a power supply problem, settings an update reset, a Gen 4 NVMe drive in the dock,
// a storage check that failed, a drive nearly full, a newer Phosphor out or waiting on the MiSTer, box art
// finishing. Each becomes a notification in the app (More > Notifications) and, unless turned off, an
// Android notification. Checked every minute while the app is open, and about every 15 minutes in the
// background (Android's job scheduler, on WiFi only: the MiSTer is on the home network).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.job.JobInfo
import android.app.job.JobParameters
import android.app.job.JobScheduler
import android.app.job.JobService
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject

/** one notification: id is one per event, so the same event is never told twice; go: the section that deals with it */
data class Note(val id: String, val at: Long, val level: String, val title: String, val body: String, val seen: Boolean, val go: String)

object Notes {
    private const val CHANNEL_WARN = "warnings"
    private const val CHANNEL_INFO = "news"
    var list by mutableStateOf(listOf<Note>())
        private set
    private var loaded = false

    private fun load(c: Context) {
        if (loaded) return
        Prefs.init(c)
        val a = runCatching { JSONArray(Prefs.getString("notes") ?: "[]") }.getOrDefault(JSONArray())
        list = (0 until a.length()).map { a.getJSONObject(it) }.map {
            Note(it.optString("id"), it.optLong("at"), it.optString("level"), it.optString("title"), it.optString("body"), it.optBoolean("seen"), it.optString("go"))
        }
        loaded = true
    }

    private fun save() {
        val a = JSONArray()
        list.forEach { n -> a.put(JSONObject().put("id", n.id).put("at", n.at).put("level", n.level).put("title", n.title).put("body", n.body).put("seen", n.seen).put("go", n.go)) }
        Prefs.putString("notes", a.toString())
    }

    fun unseen(c: Context): Int { load(c); return list.count { !it.seen } }

    /** a new notification (nothing if that event was already told) */
    @Synchronized
    fun add(c: Context, id: String, level: String, title: String, body: String, go: String) {
        load(c)
        if (list.any { it.id == id }) return
        list = (listOf(Note(id, System.currentTimeMillis(), level, title, body, false, go)) + list).take(60)
        save()
        if (Prefs.notify) post(c, id, level, title, body, go)
    }

    @Synchronized
    fun markSeen(c: Context) { load(c); if (list.any { !it.seen }) { list = list.map { it.copy(seen = true) }; save() } }

    @Synchronized
    fun clear(c: Context) { load(c); list = emptyList(); save(); c.getSystemService(NotificationManager::class.java)?.cancelAll() }

    fun channels(c: Context) {
        val nm = c.getSystemService(NotificationManager::class.java) ?: return
        nm.createNotificationChannel(NotificationChannel(CHANNEL_WARN, "Warnings", NotificationManager.IMPORTANCE_HIGH).apply { description = "Power, storage and update problems on the MiSTer" })
        nm.createNotificationChannel(NotificationChannel(CHANNEL_INFO, "News", NotificationManager.IMPORTANCE_DEFAULT).apply { description = "Box art finished, a Phosphor update ready" })
    }

    fun canPost(c: Context) = Build.VERSION.SDK_INT < 33 || c.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    private fun post(c: Context, id: String, level: String, title: String, body: String, go: String) {
        if (!canPost(c)) return
        channels(c)
        val open = PendingIntent.getActivity(c, id.hashCode(), Intent(c, MainActivity::class.java).putExtra("go", go.ifEmpty { "notes" })
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val n = android.app.Notification.Builder(c, if (level == "warn") CHANNEL_WARN else CHANNEL_INFO)
            .setSmallIcon(R.drawable.ic_notify).setContentTitle(title).setContentText(body)
            .setStyle(android.app.Notification.BigTextStyle().bigText(body))
            .setContentIntent(open).setAutoCancel(true).build()
        c.getSystemService(NotificationManager::class.java)?.notify(id.hashCode(), n)
    }

    // ---------- the check ----------

    private fun gb(kb: Long) = "%.1f GB".format(kb / 1048576.0)

    /** one look at the MiSTer; quiet when it can't be reached (the phone may simply be away from home) */
    suspend fun check(c: Context, api: PhosphorApi) {
        load(c)
        val name = "Your MiSTer"
        runCatching { api.power() }.getOrNull()?.let { p ->
            if (p.installed && p.warn) add(c, "power-${p.count}", "warn", "Power supply problem?",
                "$name lost power ${p.count} times in the middle of something. A weak or shared power adapter is the usual cause.", "power")
        }
        runCatching { api.guard() }.getOrNull()?.let { g ->
            val n = g.files.count { it.reset }
            if (g.awaiting && n > 0) add(c, "guard-${g.updatedAt}-$n", "warn", "An update reset $n setting${if (n == 1) "" else "s"}",
                "Your own settings can be put back from the restore point taken before it (Power & updates > Restore points).", "power/2")
        }
        runCatching { api.storage() }.getOrNull()?.let { s ->
            if (!s.installed) return@let
            for (d in s.drives) {
                if (d.gen == "gen4") add(c, "gen4-${d.model}", "warn", "Gen 4 NVMe drive in the dock",
                    "${d.model}: Gen 4 drives cause boot and file system trouble in the SS1 dock. A Gen 3 drive is the safe choice.", "storage/1")
                // nearly full: told once, and again only after it had room again
                val key = "low-${d.path}"
                val low = d.mounted && d.sizeKb > 0 && (d.freeKb < d.sizeKb / 20 || d.freeKb < 1048576L)
                if (low && !Prefs.getBool(key)) {
                    Prefs.putBool(key, true)
                    add(c, "$key-${System.currentTimeMillis() / 86_400_000}", "warn", "${if (d.kind == "sd") "SD card" else d.kind.uppercase()} nearly full",
                        "${gb(d.freeKb)} free of ${gb(d.sizeKb)}. Games, saves and box art need room to be written.", if (d.kind == "sd") "storage" else "storage/1")
                } else if (!low && Prefs.getBool(key)) Prefs.putBool(key, false)
            }
            val l = s.last
            if (!l.busy) {
                val before = Prefs.getString("storage_at")
                if (before != null && before != l.at.toString() && l.output.contains("RESULT: FAIL"))
                    add(c, "storage-${l.at}", "warn", "A storage check found a problem", "${l.job} on ${l.target}: see Storage for what it found.", "storage")
                Prefs.putString("storage_at", l.at.toString())
            }
        }
        runCatching { api.update() }.getOrNull()?.let { u ->
            if (!u.available) return@let
            // a newer Phosphor on GitHub (the MiSTer looks every 30 minutes): download it from the app or the TV
            if (u.onlineNewer && !u.onlineDownloaded && u.onlineLatest.isNotEmpty())
                add(c, "online-${u.onlineLatest}", "info", "Phosphor ${u.onlineLatest} is out",
                    "$name has ${u.version}. Download and install it from Power & updates > Update.", "power/1")
            u.packages.filter { versionNewer(it.version, u.version) }.reduceOrNull { a, b -> if (versionNewer(b.version, a.version)) b else a }?.let { p ->
                add(c, "update-${p.version}", "info", "Phosphor ${p.version} is ready to install",
                    "It's on the MiSTer (installed: ${u.version}). Install it from Power & updates > Update.", "power/1")
            }
        }
        // the MiSTer's own box art run finishing (only a run this phone saw going)
        runCatching { api.scrapeProgress() }.getOrNull()?.let { sp ->
            if (Prefs.getBool("ss1_scraping") && !sp.running)
                add(c, "art-${System.currentTimeMillis()}", "info", "Box art is done", "${sp.found} found, ${sp.notFound} not found.", "boxart")
            Prefs.putBool("ss1_scraping", sp.running)
        }
    }

    // ---------- in the background ----------

    private const val JOB = 1

    fun schedule(c: Context) {
        val js = c.getSystemService(JobScheduler::class.java) ?: return
        if (!Prefs.notify) { js.cancel(JOB); return }
        if (js.getPendingJob(JOB) != null) return
        js.schedule(JobInfo.Builder(JOB, ComponentName(c, WatchJob::class.java))
            .setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED)
            .setPeriodic(15 * 60 * 1000L)
            .setPersisted(true)
            .build())
    }

    fun unschedule(c: Context) { c.getSystemService(JobScheduler::class.java)?.cancel(JOB) }
}

class WatchJob : JobService() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    override fun onStartJob(params: JobParameters): Boolean {
        Prefs.init(this)
        val p = KeystorePairingStore(this).load() ?: return false
        scope.launch {
            try { Notes.check(this@WatchJob, PhosphorApi(p.address, p.token)) } finally { jobFinished(params, false) }
        }
        return true
    }

    override fun onStopJob(params: JobParameters): Boolean = false
}
