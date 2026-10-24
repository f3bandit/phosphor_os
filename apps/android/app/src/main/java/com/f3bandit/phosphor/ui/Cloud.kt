// Phosphor for Android: Cloud saves (the MiSTer's cloud-sync module). Passwords and sign-in tokens are
// sent only to the MiSTer, on the home network; WiFi, Bluetooth pairings and passwords are never sent to the cloud.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
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
import com.f3bandit.phosphor.CloudState

enum class CloudMode { MAIN, DEVICES, PREVIEW, ADD }

val CLOUD_KINDS = listOf(
    "webdav" to "WebDAV (NAS, Nextcloud)", "sftp" to "SFTP", "smb" to "Windows share (SMB)",
    "drive" to "Google Drive", "dropbox" to "Dropbox", "onedrive" to "OneDrive",
)

data class AddForm(val kind: String = "webdav", val address: String = "", val user: String = "", val password: String = "",
                   val share: String = "", val token: String = "")

private fun busyText(job: String) = when (job) {
    "setup" -> "Downloading rclone on the MiSTer..."; "push" -> "Sending..."; "pull" -> "Getting saves back..."
    "preview" -> "Checking what would come back..."; "test" -> "Checking the connection..."; "devices" -> "Looking in the cloud..."
    "add" -> "Adding..."; "use" -> "Choosing..."; else -> "Working..."
}

@Composable
fun CloudContent(
    st: CloudState?, error: String?, mode: CloudMode, lines: List<String>, from: String, form: AddForm,
    onSend: () -> Unit, onGetBack: () -> Unit, onDevice: (String) -> Unit, onConfirm: () -> Unit, onUse: (String) -> Unit,
    onAdd: () -> Unit, onForm: (AddForm) -> Unit, onSubmit: () -> Unit, onSetup: () -> Unit, onTest: () -> Unit, onBack: () -> Unit,
) {
    Screen("Saves", when (mode) { CloudMode.DEVICES -> "Whose saves?"; CloudMode.PREVIEW -> "Get saves back"; CloudMode.ADD -> "Add a storage place"; else -> "Saves" }, onBack = if (mode != CloudMode.MAIN) onBack else null) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (st.busy) Text(busyText(st.job), style = T.small.copy(color = P.acc))
            when (mode) {
                CloudMode.DEVICES -> {
                    Text("Each MiSTer has its own copy in the cloud.", style = T.small)
                    Card { lines.forEach { l -> val me = l.endsWith("(this MiSTer)"); val name = l.removeSuffix("  (this MiSTer)")
                        Row2(name, value = if (me) "this MiSTer" else "another MiSTer", accent = me, onClick = { onDevice(name) }) } }
                }
                CloudMode.PREVIEW -> {
                    Text("From $from. Newer files win; anything replaced on the MiSTer is kept in phosphor/backup/cloud-sync first.", style = T.small)
                    Card { lines.forEach { Row2(it.trim(), dim = it.startsWith(" ")) } }
                    val nothing = lines.firstOrNull()?.startsWith("Nothing") != false
                    if (!nothing) Button("Get them back", Modifier.fillMaxWidth(), primary = true, enabled = !st.busy, onClick = onConfirm)
                }
                CloudMode.ADD -> AddPlace(form, onForm, onSubmit, st.busy)
                CloudMode.MAIN -> {
                    Card {
                        when {
                            !st.installed -> Row2("Not installed", sub = "Install the cloud-sync module on the MiSTer (modules/cloud-sync/install.sh)", dim = true)
                            !st.ready -> Row2("Download rclone", value = "›", sub = "Once, about 25 MB, checked against rclone's checksums", accent = true, onClick = onSetup)
                            st.remote.isEmpty() -> Row2("No storage place yet", sub = "Add one below", dim = true)
                            else -> Row2("Sending to ${st.remote}", sub = if (st.lastError.isNotEmpty()) st.lastError else if (st.lastPush.isNotEmpty()) "Last sent ${st.lastPush}  ·  ${st.device}" else "Not sent yet  ·  ${st.device}")
                        }
                    }
                    if (st.ready && st.remote.isNotEmpty()) {
                        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Button("Send now", Modifier.weight(1f), primary = true, enabled = !st.busy, onClick = onSend)
                            Button("Get saves back", Modifier.weight(1f), enabled = !st.busy, onClick = onGetBack)
                        }
                    }
                    if (st.ready) {
                        Label("Storage places", Modifier.padding(top = 8.dp))
                        Card {
                            st.places.forEach { p -> Row2(p.name, value = if (p.inUse) "in use" else "Use", sub = p.type, accent = p.inUse, onClick = { if (!p.inUse) onUse(p.name) }) }
                            Row2("Add a storage place", value = "›", onClick = onAdd)
                            if (st.remote.isNotEmpty()) Row2("Check the connection", value = "›", onClick = onTest)
                        }
                    }
                    Text("Sent after each game: saves, save states and core settings, with older versions kept in the cloud. " +
                        "WiFi networks, Bluetooth pairings and passwords are never sent.", style = T.small)
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
@Composable
private fun AddPlace(f: AddForm, onForm: (AddForm) -> Unit, onSubmit: () -> Unit, busy: Boolean) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        CLOUD_KINDS.forEach { (k, name) -> Pill(name, f.kind == k) { onForm(f.copy(kind = k)) } }
    }
    val oauth = f.kind in setOf("drive", "dropbox", "onedrive")
    if (oauth) {
        Text("Sign in once on a PC: install rclone (rclone.org/downloads), run  rclone authorize \"${f.kind}\" , sign in in the browser, " +
            "then copy the whole {\"access_token\":...} line it prints and paste it here. It's stored only on your MiSTer.", style = T.small)
        Label("Sign-in token")
        Field(f.token, { onForm(f.copy(token = it)) }, "{\"access_token\":...}", singleLine = false, modifier = Modifier.height(120.dp))
    } else {
        Label(if (f.kind == "webdav") "WebDAV address" else "Computer or NAS")
        Field(f.address, { onForm(f.copy(address = it)) }, if (f.kind == "webdav") "https://nas.local/remote.php/dav/files/me" else "192.168.1.5")
        if (f.kind == "smb") { Label("Share name"); Field(f.share, { onForm(f.copy(share = it)) }, "Backups") }
        Label("User name"); Field(f.user, { onForm(f.copy(user = it)) }, "")
        Label("Password"); Field(f.password, { onForm(f.copy(password = it)) }, "", password = true)
    }
    val ok = if (oauth) f.token.trim().startsWith("{") else f.address.isNotBlank() && (f.kind != "smb" || f.share.isNotBlank())
    Button("Add and use it", Modifier.fillMaxWidth(), primary = true, enabled = ok && !busy, onClick = onSubmit)
    Text("Passwords are stored on the MiSTer only (obscured by rclone), never in the cloud.", style = T.small)
}
