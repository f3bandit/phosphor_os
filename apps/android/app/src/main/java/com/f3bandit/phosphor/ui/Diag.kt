// Phosphor for Android: Diagnostics (the MiSTer's whole-system checks; reading only).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.layout.Column
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
import com.f3bandit.phosphor.DiagState
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private val ORDER = listOf("fail", "warn", "info", "ok")

@Composable
fun DiagContent(st: DiagState?, error: String?, onRun: () -> Unit, onBack: () -> Unit) {
    Screen("Diagnostics", "Diagnostics") {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (!st.installed) { Card { Row2("Not installed", sub = "The diag module comes with Phosphor", dim = true) }; return@Column }
            val when_ = if (st.at > 0) SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date(st.at * 1000)) else "never"
            Card { Row2(st.summary.ifEmpty { if (st.at > 0) "Checked" else "Not checked yet" }, sub = if (st.busy) "Checking..." else "Last checked $when_", accent = st.level == "fail" || st.level == "warn") }
            Button(if (st.busy) "Checking..." else "Check now", Modifier.fillMaxWidth(), primary = true, enabled = !st.busy, onClick = onRun)
            if (st.error.isNotEmpty()) Text(st.error, style = T.small.copy(color = P.bad))
            st.items.sortedBy { ORDER.indexOf(it.level).let { i -> if (i < 0) 9 else i } }.groupBy { it.area }.forEach { (area, items) ->
                Label(area.ifEmpty { "System" }, Modifier.padding(top = 8.dp))
                Card { items.forEach { d -> Row2(d.message, value = d.level, accent = d.level == "fail" || d.level == "warn", dim = d.level == "ok") } }
            }
            Text("Support bundles are made on the MiSTer itself (Settings › Diagnostics), not from the phone.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
