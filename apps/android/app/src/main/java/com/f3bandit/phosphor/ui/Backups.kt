// Phosphor for Android: Backups (the MiSTer's backup-agent snapshots, kept on its SD card, NVMe or NAS).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.f3bandit.phosphor.BackupState
import com.f3bandit.phosphor.Snapshot

fun reasonText(r: String) = when {
    r == "manual" -> "Backed up by hand"; r == "daily" -> "Daily"; r == "pre-restore" -> "Before a restore"
    r.startsWith("exit-") -> "After ${r.removePrefix("exit-")}"; else -> r
}

@Composable
fun BackupsContent(
    st: BackupState?, error: String?, preview: Snapshot?, files: List<String>,
    onNow: () -> Unit, onPick: (Snapshot) -> Unit, onRestoreSaves: () -> Unit, onRestoreAll: () -> Unit, onBack: () -> Unit,
) {
    Screen("Saves", if (preview != null) "Restore" else "Saves", onBack = if (preview != null) onBack else null) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (st.busy) Text(when (st.job) { "now" -> "Backing up..."; "restore" -> "Restoring..."; else -> "Looking inside..." }, style = T.small.copy(color = P.acc))
            if (!st.installed) { Card { Row2("Not installed", sub = "Install the backup-agent module on the MiSTer", dim = true) }; return@Column }
            if (preview != null) {
                Text("${preview.whenText} (${reasonText(preview.reason)}). Restore saves puts back only saves and save states; " +
                    "Everything also puts back the settings in it. A backup is taken first, so either can be undone.", style = T.small)
                Card { if (files.isEmpty()) Row2("No saves in this backup", dim = true) else files.forEach { Row2(it) } }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Button("Restore saves", Modifier.weight(1f), primary = true, enabled = !st.busy && files.isNotEmpty(), onClick = onRestoreSaves)
                    Button("Everything", Modifier.weight(1f), enabled = !st.busy, onClick = onRestoreAll)
                }
            } else {
                Card { Row2("${st.snapshots.size} backups", sub = st.lastError.ifEmpty { st.dest }) }
                Button("Back up now", Modifier.fillMaxWidth(), primary = true, enabled = !st.busy, onClick = onNow)
                Label("Backups", Modifier.padding(top = 8.dp))
                Card { st.snapshots.forEach { s -> Row2(s.whenText, value = "${reasonText(s.reason)}  \u00B7  ${s.kb} KB", dim = s.reason == "pre-restore", onClick = { onPick(s) }) } }
                Text("Taken on the MiSTer when you leave a game and once a day. They stay on its drives, never in the cloud.", style = T.small)
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}
