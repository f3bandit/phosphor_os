// Phosphor for Android: Home (continue the last game, what needs attention, favorites) and Library (the
// shelf by category, search, rescan).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.f3bandit.phosphor.Category
import com.f3bandit.phosphor.Game
import com.f3bandit.phosphor.GameItem
import com.f3bandit.phosphor.Status
import com.f3bandit.phosphor.SystemItem

fun busyText(s: Status): String? = when {
    s.indexing -> "Looking for games..."
    s.identifying -> "Identifying games..."
    s.scraperRunning -> s.scraperStatus.ifEmpty { "Getting box art..." }
    else -> null
}

@Composable
fun HomeContent(
    status: Status?, last: Game?, lastCover: ImageBitmap?, error: String?,
    onPlayLast: () -> Unit, onOpenLast: () -> Unit, onFilter: (String, String) -> Unit, onLibrary: () -> Unit,
) {
    Screen("Home", "Home") {
        LazyColumn(contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 24.dp), verticalArrangement = Gap) {
            if (error != null) item { Card { Row2("Not reachable", sub = error, dim = true) } }
            if (status == null && error == null) item { Loading() }
            if (last != null) {
                item { Label("Continue", Modifier.padding(start = 4.dp, top = 4.dp)) }
                item {
                    Card {
                        Row(Modifier.padding(horizontal = 12.dp, vertical = 6.dp), horizontalArrangement = Arrangement.spacedBy(14.dp), verticalAlignment = Alignment.CenterVertically) {
                            Cover(lastCover, Modifier.width(72.dp).aspectRatio(0.75f), last.title)
                            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                Text(last.title, style = T.name, maxLines = 2, overflow = TextOverflow.Ellipsis)
                                Text(last.systemName, style = T.small)
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Button("Play on TV", Modifier.weight(1f), primary = true, onClick = onPlayLast)
                                    Button("Details", Modifier.weight(1f), onClick = onOpenLast)
                                }
                            }
                        }
                    }
                }
            }
            if (status != null && (status.missingArt + status.missingDetails + status.notFound) > 0) {
                item { Label("Needs attention", Modifier.padding(start = 4.dp, top = 8.dp)) }
                item {
                    Card {
                        if (status.missingArt > 0) Row2("No box art", value = "${status.missingArt}", accent = true, onClick = { onFilter("missing_art", "No box art") })
                        if (status.missingDetails > 0) Row2("No details", value = "${status.missingDetails}", accent = true, onClick = { onFilter("missing_details", "No details") })
                        if (status.notFound > 0) Row2("Not found by the lookup", value = "${status.notFound}", onClick = { onFilter("not_found", "Not found") })
                    }
                }
            }
            if (status != null) {
                item { Label("Your games", Modifier.padding(start = 4.dp, top = 8.dp)) }
                item {
                    Card {
                        Row2("Favorites", value = "›", onClick = { onFilter("favorites", "Favorites") })
                        Row2("Recently played", value = "›", onClick = { onFilter("recent", "Recently played") })
                        Row2("${status.games} games in the library", value = "›", sub = busyText(status) ?: "Phosphor ${status.version}", onClick = onLibrary)
                    }
                }
            }
        }
    }
}

@Composable
fun LibraryContent(
    categories: List<Category>, cat: Int, systems: List<SystemItem>?, query: String, results: List<GameItem>?, total: Int,
    status: Status?, error: String?, art: @Composable (GameItem) -> ImageBitmap?,
    onCat: (Int) -> Unit, onSystem: (SystemItem) -> Unit, onQuery: (String) -> Unit, onGame: (GameItem) -> Unit, onRescan: () -> Unit,
) {
    val tabs = listOf("All") + categories.map { it.name }
    Screen("Library", "Library", tabs = tabs, tab = cat, onTab = onCat) {
        LazyColumn(contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 24.dp), verticalArrangement = Gap) {
            item { Field(query, onQuery, "Search every system") }
            if (error != null) item { Message(error, true) }
            if (query.isNotBlank()) {
                when {
                    results == null -> item { Loading() }
                    results.isEmpty() -> item { Message("Nothing matches.") }
                    else -> {
                        item { Message(if (total > results.size) "${results.size} of $total" else "$total games") }
                        items(results, key = { it.id }) { g -> GameRow(g, true, art(g)) { onGame(g) } }
                    }
                }
                return@LazyColumn
            }
            when {
                systems == null -> item { Loading() }
                systems.isEmpty() -> item { Card { Row2("No games here yet", sub = "Rescan looks for them", dim = true) } }
                else -> item { Card { systems.forEach { s -> Row2(s.name, value = "${s.games}", onClick = { onSystem(s) }) } } }
            }
            item { Spacer(Modifier.height(4.dp)) }
            item {
                Card {
                    val busy = when { status?.indexing == true -> "Looking for games..."; status?.identifying == true -> "Identifying games..."; else -> null }
                    Row2("Rescan the library", value = if (status?.indexing == true || status?.identifying == true) null else "Rescan",
                        sub = busy ?: "Looks for new games and works out what they are. Runs only when you ask.", accent = true,
                        onClick = if (status?.indexing == true || status?.identifying == true) null else onRescan)
                }
            }
        }
    }
}
