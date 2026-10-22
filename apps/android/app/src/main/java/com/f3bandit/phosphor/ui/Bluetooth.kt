// Phosphor for Android: Bluetooth controllers paired with the MiSTer (pairings stay on the MiSTer).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.f3bandit.phosphor.BtState

@Composable
fun BluetoothContent(st: BtState?, error: String?, onBack: () -> Unit) {
    Screen("Settings", "Bluetooth", onBack = onBack) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (!st.present) { Card { Row2("No Bluetooth adapter", sub = "Plug one into the MiSTer", dim = true) }; return@Column }
            if (st.busy) Text(if (st.found.isNotEmpty() && st.message.endsWith("...")) "${st.message.removeSuffix("...")}: ${st.found}..." else st.message, style = T.small.copy(color = P.acc))
            Label("Paired with the MiSTer")
            Card {
                if (st.devices.isEmpty()) Row2("Nothing paired yet", dim = true)
                st.devices.forEach { d -> Row2(d.name, value = if (d.connected) "Connected" else "Not connected", dim = !d.connected) }
            }
            Text("Pairing, disconnecting and forgetting are done on the TV (Settings › Bluetooth), so nothing pairs " +
                "with the MiSTer from somewhere else. Pairings stay on the MiSTer's SD card.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
