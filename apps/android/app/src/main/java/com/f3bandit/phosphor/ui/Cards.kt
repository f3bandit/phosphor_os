// Phosphor for Android: Controllers & memory cards (the SuperStation One's front ports).
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
import com.f3bandit.phosphor.CardBackup
import com.f3bandit.phosphor.CardState
import com.f3bandit.phosphor.Pad
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private val BUTTONS = listOf("Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left",
    "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square")

fun padType(id: Int) = when (id) {
    0x41 -> "Digital controller"; 0x73 -> "DualShock (analog)"; 0x53 -> "Analog joystick"; 0x23 -> "neGcon"
    0x12 -> "Mouse"; 0x63 -> "GunCon"; 0xF3 -> "Controller (setting up)"; else -> "Controller"
}

fun padButtons(b: Int) = BUTTONS.filterIndexed { i, _ -> b and (1 shl i) != 0 }.joinToString(" ")

/** "port1-20261022-133030.mcd" -> "Port 1 card · 2026-10-22 13:30" */
fun cardLabel(b: CardBackup): String {
    val m = Regex("""port(\d)-(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})""").find(b.name)
        ?: return b.name
    val g = m.groupValues
    return "Port ${g[1]} card  ·  ${g[2]}-${g[3]}-${g[4]} ${g[5]}:${g[6]}"
}

@Composable
fun CardsContent(
    st: CardState?, pads: List<Pad>, error: String?, pick: CardBackup?,
    onLoad: () -> Unit, onBackup: (Int) -> Unit, onAnalog: () -> Unit, onPick: (CardBackup) -> Unit,
    onRestore: (Int) -> Unit, onBack: () -> Unit,
) {
    Screen("Settings", if (pick != null) "Put back" else "Controllers & cards", onBack = onBack) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            if (st == null) { if (error == null) Loading(); return@Column }
            if (st.busy) {
                val pc = if (st.total > 0) st.done * 100 / st.total else 0
                Text(when (st.job) {
                    "prepare" -> "Loading the Phosphor menu core..."
                    "backup" -> "Backing up the card in port ${st.port}... $pc%"
                    "restore" -> (if (st.done < st.total / 2) "Backing up the card first... " else "Writing... ") + "$pc%"
                    else -> "Working..."
                }, style = T.small.copy(color = P.acc))
            }
            when {
                !st.mainApi -> { Card { Row2("Needs Phosphor's Main", sub = "phosphor-main isn't running on the MiSTer", dim = true) }; return@Column }
                st.inGame -> { Card { Row2("In a game", sub = "The front ports are read only from the menu", dim = true) }; return@Column }
                !st.available && !st.busy -> {
                    Card { Row2("Front ports off", sub = if (st.canLoad) "Loads the Phosphor menu core (a few seconds)" else "The Phosphor menu core isn't installed", dim = true) }
                    if (st.canLoad) Button("Turn on the front ports", Modifier.fillMaxWidth(), primary = true, onClick = onLoad)
                    return@Column
                }
            }
            if (pick != null) {
                Text("${cardLabel(pick)}: put it on the card in which port? The card is backed up first, " +
                    "so this can be undone from the list.", style = T.small)
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Button("Port 1", Modifier.weight(1f), primary = true, enabled = !st.busy, onClick = { onRestore(1) })
                    Button("Port 2", Modifier.weight(1f), enabled = !st.busy, onClick = { onRestore(2) })
                }
                Spacer(Modifier.height(24.dp))
                return@Column
            }
            Label("Controllers")
            Card {
                (1..2).forEach { n ->
                    val p = pads.firstOrNull { it.port == n }
                    if (p == null || !p.connected) Row2("Port $n", value = "nothing plugged in", dim = true)
                    else {
                        val sticks = if (p.id == 0x73 || p.id == 0x53) "L ${p.lx},${p.ly}  R ${p.rx},${p.ry}" else null
                        Row2("Port $n  ·  ${padType(p.id)}", sub = listOfNotNull(padButtons(p.buttons).ifEmpty { null }, sticks).joinToString("   ").ifEmpty { "No buttons held" })
                    }
                }
            }
            if (pads.any { it.connected && it.id == 0x41 }) Button("Switch DualShocks to analog", Modifier.fillMaxWidth(), onClick = onAnalog)
            Label("Memory cards", Modifier.padding(top = 8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Button("Back up port 1", Modifier.weight(1f), primary = true, enabled = !st.busy, onClick = { onBackup(1) })
                Button("Back up port 2", Modifier.weight(1f), enabled = !st.busy, onClick = { onBackup(2) })
            }
            if (st.backups.isNotEmpty()) {
                val df = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
                Card { st.backups.forEach { b -> Row2(cardLabel(b), sub = if (b.name.startsWith("port")) null else df.format(Date(b.at * 1000)), value = "Put back ›", onClick = { onPick(b) }) } }
            }
            Text("Backups are whole cards (.mcd, like MiSTer's PSX core) kept in phosphor/memcards on the MiSTer. " +
                "MT32-pi and MIDI are off while the front ports are in use.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
