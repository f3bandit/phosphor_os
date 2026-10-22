// Phosphor for Android: Bluetooth controllers paired with the MiSTer (pairings stay on the MiSTer).
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
import com.f3bandit.phosphor.BtDevice
import com.f3bandit.phosphor.BtState

@Composable
fun BluetoothContent(
    st: BtState?, error: String?, pick: BtDevice?,
    onPair: () -> Unit, onCancel: () -> Unit, onPick: (BtDevice) -> Unit, onDisconnect: () -> Unit, onForget: () -> Unit, onBack: () -> Unit,
) {
    Screen("Settings", pick?.name ?: "Bluetooth", onBack = onBack) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (!st.present) { Card { Row2("No Bluetooth adapter", sub = "Plug one into the MiSTer", dim = true) }; return@Column }
            if (pick != null) {
                Text("Disconnect: it reconnects when you turn it on again. Forget: pair it again to use it.", style = T.small)
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Button("Disconnect", Modifier.weight(1f), enabled = pick.connected && !st.busy, onClick = onDisconnect)
                    Button("Forget", Modifier.weight(1f), primary = true, enabled = !st.busy, onClick = onForget)
                }
                return@Column
            }
            if (st.busy) {
                Text(if (st.found.isNotEmpty() && st.message.endsWith("...")) "${st.message.removeSuffix("...")}: ${st.found}..." else st.message, style = T.small.copy(color = P.acc))
                Button("Stop looking", Modifier.fillMaxWidth(), onClick = onCancel)
            } else {
                Button("Pair a controller", Modifier.fillMaxWidth(), primary = true, onClick = onPair)
                Text("Put the controller in pairing mode first. The MiSTer pairs the first one it finds.", style = T.small)
            }
            Label("Paired with the MiSTer", Modifier.padding(top = 8.dp))
            Card {
                if (st.devices.isEmpty()) Row2("Nothing paired yet", dim = true)
                st.devices.forEach { d -> Row2(d.name, value = if (d.connected) "Connected" else "Not connected", dim = !d.connected, onClick = { onPick(d) }) }
            }
            Text("Pairings stay on the MiSTer's SD card.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
