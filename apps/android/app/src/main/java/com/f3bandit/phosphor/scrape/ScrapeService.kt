// Phosphor for Android: the box art run on the phone keeps going with the app in the background, as a
// foreground service with a notification that shows how far it is (and a Stop button).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import com.f3bandit.phosphor.KeystorePairingStore
import com.f3bandit.phosphor.MainActivity
import com.f3bandit.phosphor.Notes
import com.f3bandit.phosphor.PhosphorApi
import com.f3bandit.phosphor.Prefs
import com.f3bandit.phosphor.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class ScrapeService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var run: Job? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) { if (run?.isActive == true) run?.cancel() else stopSelf(); return START_NOT_STICKY }
        channel(this)
        val n = progress("Starting")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) startForeground(ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC) else startForeground(ID, n)
        if (run?.isActive == true) return START_NOT_STICKY
        Prefs.init(this); ArtStore.init(this)
        val p = KeystorePairingStore(this).load()
        if (p == null) { stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); return START_NOT_STICKY }
        val api = PhosphorApi(p.address, p.token)
        val only = intent?.getBooleanExtra(EXTRA_ONLY, false) == true
        run = scope.launch {
            val ticker = launch {
                while (true) {
                    delay(2000)
                    val s = PhoneScrape.state
                    val nm = getSystemService(NotificationManager::class.java)
                    nm.notify(ID, progress(if (s.total > 0) "${s.done} of ${s.total}  ·  ${s.found} found" else s.status))
                }
            }
            try { ScrapeRun(applicationContext, api, only).run() } finally {
                ticker.cancel()
                val s = PhoneScrape.state
                Notes.add(this@ScrapeService, "phone-art-${s.started}", "info", "Box art is done",
                    if (only) "${s.status}: ${s.matched} matched to base games." else "${s.status}: ${s.found} found, ${s.notFound} not found, ${s.matched} matched to base games on the phone.", "boxart")
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_NOT_STICKY
    }

    /** Android 15: a data sync can run 6 hours a day; past that it's stopped, like Stop */
    override fun onTimeout(startId: Int, fgsType: Int) { run?.cancel() }

    override fun onDestroy() { scope.cancel(); super.onDestroy() }

    private fun progress(text: String): Notification {
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java).putExtra("go", "boxart").addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val stop = PendingIntent.getService(this, 1, Intent(this, ScrapeService::class.java).setAction(ACTION_STOP), PendingIntent.FLAG_IMMUTABLE)
        return Notification.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_notify)
            .setContentTitle("Looking up box art")
            .setContentText(text)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(open)
            .addAction(Notification.Action.Builder(null, "Stop", stop).build())
            .build()
    }

    companion object {
        const val CHANNEL = "boxart"
        private const val ID = 7
        private const val ACTION_STOP = "stop"

        fun channel(c: Context) {
            c.getSystemService(NotificationManager::class.java).createNotificationChannel(
                NotificationChannel(CHANNEL, "Box art lookup", NotificationManager.IMPORTANCE_LOW).apply { description = "Shows a box art lookup running on the phone" })
        }

        private const val EXTRA_ONLY = "only"
        /** only: just match the not-found games to their base games */
        fun start(c: Context, only: Boolean = false) { c.startForegroundService(Intent(c, ScrapeService::class.java).putExtra(EXTRA_ONLY, only)) }
        fun stop(c: Context) { c.startService(Intent(c, ScrapeService::class.java).setAction(ACTION_STOP)) }
    }
}
