// Phosphor for Android: the screens. Each has a stateless part (data in, events out; used by the
// screenshot tests) and a small loader that talks to the MiSTer.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.f3bandit.phosphor.Candidate
import com.f3bandit.phosphor.Game
import com.f3bandit.phosphor.GameItem
import com.f3bandit.phosphor.Info
import com.f3bandit.phosphor.PhosphorApi
import com.f3bandit.phosphor.Status
import com.f3bandit.phosphor.SystemItem
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit

// ---------- shared bits ----------

@Composable
fun Screen(label: String, title: String, onBack: (() -> Unit)? = null, action: (@Composable () -> Unit)? = null, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize().background(P.bg).statusBarsPadding().navigationBarsPadding().imePadding()) {
        Row(Modifier.fillMaxWidth().padding(start = 20.dp, end = 12.dp, top = 14.dp), verticalAlignment = Alignment.CenterVertically) {
            if (onBack != null) {
                Text("←", style = T.title.copy(color = P.acc), modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onBack).padding(end = 14.dp, top = 2.dp, bottom = 2.dp))
            }
            Text("PHOSPHOR", style = T.word, modifier = Modifier.weight(1f))
            action?.invoke()
        }
        Column(Modifier.padding(horizontal = 20.dp).padding(top = 18.dp, bottom = 10.dp)) {
            Label(label)
            Spacer(Modifier.height(4.dp))
            Text(title, style = T.heading, maxLines = 2, overflow = TextOverflow.Ellipsis)
        }
        Box(Modifier.weight(1f)) { content() }
    }
}

@Composable
fun Field(value: String, onChange: (String) -> Unit, placeholder: String, modifier: Modifier = Modifier, singleLine: Boolean = true, number: Boolean = false, big: Boolean = false, password: Boolean = false) {
    BasicTextField(
        value = value, onValueChange = onChange, singleLine = singleLine,
        textStyle = if (big) T.heading.copy(fontFamily = Mono, letterSpacing = 8.sp, color = P.acc) else T.body,
        cursorBrush = SolidColor(P.acc),
        keyboardOptions = if (number) KeyboardOptions(keyboardType = KeyboardType.Number) else if (password) KeyboardOptions(keyboardType = KeyboardType.Password) else KeyboardOptions.Default,
        visualTransformation = if (password) androidx.compose.ui.text.input.PasswordVisualTransformation() else androidx.compose.ui.text.input.VisualTransformation.None,
        modifier = modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(P.panel).border(1.dp, P.line, RoundedCornerShape(12.dp)).padding(14.dp),
        decorationBox = { inner ->
            Box {
                if (value.isEmpty()) Text(placeholder, style = if (big) T.heading.copy(fontFamily = Mono, letterSpacing = 8.sp, color = P.faint) else T.body.copy(color = P.faint))
                inner()
            }
        },
    )
}

@Composable
fun Message(text: String, error: Boolean = false) =
    Text(text, style = T.small.copy(color = if (error) P.bad else P.dim), modifier = Modifier.padding(horizontal = 20.dp, vertical = 6.dp))

@Composable
fun Loading() = Box(Modifier.fillMaxWidth().padding(40.dp), contentAlignment = Alignment.Center) {
    CircularProgressIndicator(color = P.acc, strokeWidth = 2.dp, modifier = Modifier.size(28.dp))
}

/** box art from the MiSTer, a few at a time, kept while the app runs */
object ArtCache {
    private val cache = android.util.LruCache<String, ImageBitmap>(120)
    private val none = HashSet<String>()
    private val gate = Semaphore(4)
    fun peek(key: String) = cache.get(key)
    fun drop(id: Long) { synchronized(none) { none.removeAll { it.startsWith("$id@") } } ; cache.snapshot().keys.filter { it.startsWith("$id@") }.forEach { cache.remove(it) } }
    suspend fun get(api: PhosphorApi, id: Long, version: Int): ImageBitmap? {
        val key = "$id@$version"
        cache.get(key)?.let { return it }
        if (synchronized(none) { key in none }) return null
        return gate.withPermit {
            val bytes = runCatching { api.art(id) }.getOrNull()
            val bmp = bytes?.let { BitmapFactory.decodeByteArray(it, 0, it.size)?.asImageBitmap() }
            if (bmp != null) cache.put(key, bmp) else synchronized(none) { none.add(key) }
            bmp
        }
    }
}

@Composable
fun Cover(bitmap: ImageBitmap?, modifier: Modifier, title: String = "") {
    Box(modifier.clip(RoundedCornerShape(8.dp)).background(P.panel).border(1.dp, P.line, RoundedCornerShape(8.dp)), contentAlignment = Alignment.Center) {
        if (bitmap != null) Image(bitmap, contentDescription = title, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
        else Text(title.take(1).uppercase(), style = T.title.copy(color = P.faint))
    }
}

// ---------- Connect ----------

@Composable
fun ConnectContent(
    found: List<Info>, searching: Boolean, address: String, code: String, busy: Boolean, error: String?,
    onAddress: (String) -> Unit, onCode: (String) -> Unit, onSearch: () -> Unit, onPick: (Info) -> Unit, onPair: () -> Unit,
) {
    Screen("Connect", "Pair with your MiSTer") {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            Text("On the MiSTer: Settings › Remote access. Turn it on, then choose Pair a device. The TV shows the address and a 6-digit code.", style = T.small)
            Spacer(Modifier.height(4.dp))
            Label("On this WiFi")
            Card {
                if (found.isEmpty()) Row2(if (searching) "Looking for MiSTers..." else "None found", value = if (searching) null else "Search again", dim = true, onClick = if (searching) null else onSearch)
                found.forEach { Row2("Phosphor ${it.version}", value = it.address, accent = it.address == address, onClick = { onPick(it) }) }
            }
            Spacer(Modifier.height(4.dp))
            Label("Address")
            Field(address, onAddress, "192.168.1.23:8767")
            Label("Code from the TV")
            Field(code, { s -> onCode(s.filter { it.isDigit() }.take(6)) }, "000000", number = true, big = true)
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            Button(if (busy) "Pairing..." else "Pair", Modifier.fillMaxWidth(), primary = true, enabled = !busy && address.isNotBlank() && code.length == 6, onClick = onPair)
            Spacer(Modifier.height(20.dp))
        }
    }
}

// ---------- Home ----------

@Composable
fun HomeContent(
    address: String, status: Status?, systems: List<SystemItem>, error: String?,
    onFilter: (String, String) -> Unit, onSystem: (SystemItem) -> Unit, onSearch: () -> Unit, onSettings: () -> Unit,
) {
    Screen("Library", "Your MiSTer", action = {
        Text("Search", style = T.small.copy(color = P.acc), modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onSearch).padding(8.dp))
        Text("Settings", style = T.small.copy(color = P.acc), modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onSettings).padding(8.dp))
    }) {
        LazyColumn(contentPadding = PaddingValues(horizontal = 20.dp, vertical = 4.dp), verticalArrangement = Gap) {
            item {
                Card {
                    if (status == null && error == null) Loading()
                    if (error != null) Row2("Not reachable", sub = error, dim = true)
                    if (status != null) {
                        val busy = when {
                            status.indexing -> "Looking for games..."
                            status.identifying -> "Identifying games..."
                            status.scraperRunning -> status.scraperStatus.ifEmpty { "Getting box art..." }
                            else -> null
                        }
                        Row2("${status.games} games", value = address, sub = busy ?: "Phosphor ${status.version}  ·  box art ${if (status.scraperOn) "on" else "off"}")
                    }
                }
            }
            if (status != null && (status.missingArt + status.missingDetails + status.notFound) > 0) {
                item { Label("Needs attention", Modifier.padding(top = 8.dp)) }
                item {
                    Card {
                        if (status.missingArt > 0) Row2("No box art", value = "${status.missingArt}", accent = true, onClick = { onFilter("missing_art", "No box art") })
                        if (status.missingDetails > 0) Row2("No details", value = "${status.missingDetails}", accent = true, onClick = { onFilter("missing_details", "No details") })
                        if (status.notFound > 0) Row2("Not found by the scraper", value = "${status.notFound}", onClick = { onFilter("not_found", "Not found") })
                    }
                }
            }
            if (status != null) {
                item {
                    Card {
                        Row2("Favorites", value = "›", onClick = { onFilter("favorites", "Favorites") })
                        Row2("Recently played", value = "›", onClick = { onFilter("recent", "Recently played") })
                    }
                }
            }
            if (systems.isNotEmpty()) {
                item { Label("Systems", Modifier.padding(top = 8.dp)) }
                item { Card { systems.forEach { s -> Row2(s.name, value = "${s.games}", onClick = { onSystem(s) }) } } }
            }
            item { Spacer(Modifier.height(20.dp)) }
        }
    }
}

// ---------- Games ----------

val FILTERS = listOf(null to "All", "missing_art" to "No box art", "missing_details" to "No details", "not_found" to "Not found", "linked" to "Based on")

@Composable
fun GamesContent(
    title: String, showSystem: Boolean, filter: String?, query: String, total: Int, games: List<GameItem>?, error: String?,
    art: @Composable (GameItem) -> ImageBitmap?,
    onFilter: (String?) -> Unit, onQuery: (String) -> Unit, onGame: (GameItem) -> Unit, onBack: () -> Unit,
) {
    Screen("Games", title, onBack = onBack) {
        Column {
            Column(Modifier.padding(horizontal = 20.dp)) { Field(query, onQuery, "Search titles") }
            Row(Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp, vertical = 10.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FILTERS.forEach { (f, name) -> Pill(name, filter == f) { onFilter(f) } }
            }
            when {
                error != null -> Message(error, true)
                games == null -> Loading()
                games.isEmpty() -> Message(if (filter == null && query.isBlank()) "No games here." else "Nothing matches.")
                else -> LazyColumn(contentPadding = PaddingValues(start = 12.dp, end = 12.dp, bottom = 20.dp)) {
                    item { Message(if (total > games.size) "${games.size} of $total" else "$total games") }
                    items(games, key = { it.id }) { g -> GameRow(g, showSystem, art(g)) { onGame(g) } }
                }
            }
        }
    }
}

@Composable
fun GameRow(g: GameItem, showSystem: Boolean, bmp: ImageBitmap?, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).clickable(onClick = onClick).padding(horizontal = 8.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Cover(bmp, Modifier.width(42.dp).height(56.dp), g.title)
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(g.title, style = T.name.copy(fontSize = 15.sp), maxLines = 1, overflow = TextOverflow.Ellipsis)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 3.dp)) {
                if (showSystem) Text(g.system, style = T.small)
                if (g.favorite) Tag("favorite", P.acc)
                if (g.linked) Tag("based on")
                if (!g.art) Tag("no art", P.acc)
                if (!g.details) Tag("no details")
            }
        }
    }
}

// ---------- Game ----------

private fun size(n: Long) = when {
    n >= 1L shl 30 -> "%.1f GB".format(n / (1024.0 * 1024 * 1024))
    n >= 1L shl 20 -> "%.1f MB".format(n / (1024.0 * 1024))
    else -> "${n / 1024} KB"
}

@Composable
fun GameContent(
    game: Game?, cover: ImageBitmap?, error: String?, busy: String?,
    onBack: () -> Unit, onLaunch: () -> Unit, onFavorite: () -> Unit, onEdit: () -> Unit, onArt: () -> Unit,
    onBasedOn: () -> Unit, onRescrape: () -> Unit,
) {
    Screen(game?.systemName ?: "Game", game?.title ?: "", onBack = onBack) {
        if (game == null) { if (error != null) Message(error, true) else Loading(); return@Screen }
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Cover(cover, Modifier.width(132.dp).aspectRatio(0.75f), game.title)
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button("Play on TV", Modifier.fillMaxWidth(), primary = true, onClick = onLaunch)
                    Button(if (game.favorite) "★ Favorite" else "☆ Favorite", Modifier.fillMaxWidth(), onClick = onFavorite)
                    Button("Change box art", Modifier.fillMaxWidth(), onClick = onArt)
                }
            }
            if (busy != null) Text(busy, style = T.small.copy(color = P.acc))
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            Label("Details", Modifier.padding(top = 8.dp))
            Card {
                Row2("Year", game.year ?: "—", dim = game.year == null)
                Row2("Developer", game.developer ?: "—", dim = game.developer == null)
                Row2("Publisher", game.publisher ?: "—", dim = game.publisher == null)
                Row2("Genre", game.genre ?: "—", dim = game.genre == null)
                Row2("Players", game.players ?: "—", dim = game.players == null)
                if (game.region != null) Row2("Region", game.region)
                Row2("Edit details", value = "›", accent = true, onClick = onEdit)
            }
            Text(game.description ?: "No description yet.", style = T.body.copy(color = if (game.description == null) P.faint else P.text), modifier = Modifier.padding(horizontal = 4.dp, vertical = 6.dp))
            Label("Based on", Modifier.padding(top = 8.dp))
            Card {
                Row2(game.basedOnTitle ?: "Not based on another game", value = "›", sub = if (game.basedOnTitle != null) "Uses its box art and details unless this game has its own" else "For hacks, mods and translations",
                    dim = game.basedOnTitle == null, onClick = onBasedOn)
            }
            Label("Box art and details lookup", Modifier.padding(top = 8.dp))
            Card {
                val state = when (game.scrape) {
                    "found" -> "Found"; "not_found" -> "Not found by the sources"; "linked" -> "Uses the base game's"
                    "ready" -> "Waiting for the lookup"; "unidentified" -> "Not identified yet"; else -> game.scrape.replace('_', ' ')
                }
                Row2(state, value = "Look up again", onClick = onRescrape)
            }
            Label("Files", Modifier.padding(top = 8.dp))
            Card {
                game.files.forEach { f ->
                    Row2(f.name, sub = listOfNotNull(f.where, size(f.size), f.serial, f.crc32?.let { "CRC $it" }, if (!f.online) "offline" else null).joinToString("  ·  "), dim = !f.online)
                }
            }
            if (game.playCount > 0) Message("Played ${game.playCount} times, ${game.playSeconds / 3600} h ${game.playSeconds / 60 % 60} min")
            Spacer(Modifier.height(24.dp))
        }
    }
}

// ---------- Edit ----------

val EDIT_FIELDS = listOf("title" to "Title", "year" to "Year", "developer" to "Developer", "publisher" to "Publisher",
    "genre" to "Genre", "players" to "Players", "region" to "Region", "description" to "Description")

fun gameValues(g: Game) = mapOf("title" to g.title, "year" to (g.year ?: ""), "developer" to (g.developer ?: ""), "publisher" to (g.publisher ?: ""),
    "genre" to (g.genre ?: ""), "players" to (g.players ?: ""), "region" to (g.region ?: ""), "description" to (g.description ?: ""))

@Composable
fun EditContent(title: String, values: Map<String, String>, busy: Boolean, error: String?, onChange: (String, String) -> Unit, onSave: () -> Unit, onBack: () -> Unit) {
    Screen("Edit details", title, onBack = onBack) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            EDIT_FIELDS.forEach { (k, name) ->
                Label(name)
                Field(values[k] ?: "", { onChange(k, it) }, if (k == "description") "A few sentences about the game" else "", singleLine = k != "description",
                    modifier = if (k == "description") Modifier.height(150.dp) else Modifier)
            }
            if (error != null) Text(error, style = T.small.copy(color = P.bad))
            Button(if (busy) "Saving..." else "Save", Modifier.fillMaxWidth(), primary = true, enabled = !busy, onClick = onSave)
            Text("Empty fields are cleared. The MiSTer's box art lookup never overwrites what you write here.", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}

// ---------- Based on ----------

@Composable
fun BasedOnContent(title: String, current: String?, query: String, results: List<Candidate>?, error: String?,
                   onQuery: (String) -> Unit, onPick: (Candidate) -> Unit, onUnlink: () -> Unit, onBack: () -> Unit) {
    Screen("Based on", title, onBack = onBack) {
        Column {
            Text("For a hack, mod, translation or other version: pick the original, and this game uses its box art and details (unless it has its own).",
                style = T.small, modifier = Modifier.padding(horizontal = 20.dp))
            if (current != null) {
                Column(Modifier.padding(horizontal = 20.dp, vertical = 10.dp)) {
                    Card { Row2("Now: $current", value = "Remove", accent = true, onClick = onUnlink) }
                }
            }
            Column(Modifier.padding(horizontal = 20.dp, vertical = 8.dp)) { Field(query, onQuery, "Search for the original") }
            when {
                error != null -> Message(error, true)
                results == null -> Loading()
                results.isEmpty() -> Message("No matches. Try a shorter name.")
                else -> LazyColumn(contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)) {
                    items(results, key = { it.id }) { c -> Row2(c.title, value = if (c.score >= 400) "close match" else null, onClick = { onPick(c) }) }
                }
            }
        }
    }
}

// ---------- Settings ----------

@Composable
fun SettingsContent(address: String, version: String, onForget: () -> Unit, onBack: () -> Unit, onCloud: () -> Unit = {}, onBackups: () -> Unit = {}, onCards: () -> Unit = {}, onBluetooth: () -> Unit = {}, onVideo: () -> Unit = {}, onDiag: () -> Unit = {}) {
    Screen("Settings", "Phosphor app", onBack = onBack) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 20.dp), verticalArrangement = Gap) {
            Label("MiSTer")
            Card {
                Row2("Paired with", value = address)
                Row2("Cloud saves", value = "›", onClick = onCloud)
                Row2("Backups", value = "›", onClick = onBackups)
                Row2("Controllers & memory cards", value = "›", onClick = onCards)
                Row2("Bluetooth", value = "›", onClick = onBluetooth)
                Row2("Screen filters", value = "›", onClick = onVideo)
                Row2("Diagnostics", value = "›", onClick = onDiag)
                Row2("Forget this MiSTer", value = "›", accent = true, onClick = onForget)
            }
            Text("To remove this phone on the MiSTer too: Settings › Remote access, select the phone, press X.", style = T.small)
            Label("About", Modifier.padding(top = 8.dp))
            Card {
                Row2("Phosphor for Android", value = version)
                Row2("Works with", value = "phosphord 0.12.1 or later")
            }
            Text("The app only talks to your MiSTer, on your home network. It collects nothing and sends nothing anywhere else.\n\n" +
                "Fonts: Inter (SIL Open Font License 1.1), DejaVu Sans Mono (Bitstream Vera licence).", style = T.small)
            Spacer(Modifier.height(24.dp))
        }
    }
}
