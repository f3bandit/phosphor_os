// Draws every screen with sample data into app/build/screenshots/ (no phone or emulator needed).
package com.f3bandit.phosphor

import android.graphics.Bitmap
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.activity.ComponentActivity
import android.graphics.Canvas
import com.f3bandit.phosphor.ui.AddForm
import com.f3bandit.phosphor.ui.BackupsContent
import com.f3bandit.phosphor.ui.BasedOnContent
import com.f3bandit.phosphor.ui.CloudContent
import com.f3bandit.phosphor.ui.CloudMode
import com.f3bandit.phosphor.ui.ConnectContent
import com.f3bandit.phosphor.ui.EditContent
import com.f3bandit.phosphor.ui.GameContent
import com.f3bandit.phosphor.ui.GamesContent
import com.f3bandit.phosphor.ui.HomeContent
import com.f3bandit.phosphor.ui.PhosphorTheme
import com.f3bandit.phosphor.ui.SettingsContent
import com.f3bandit.phosphor.ui.gameValues
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class ScreenshotTest {
    @get:Rule val rule = createAndroidComposeRule<ComponentActivity>()

    private fun shot(name: String, content: @Composable () -> Unit) {
        rule.setContent { PhosphorTheme { content() } }
        rule.waitForIdle()
        val dir = File("build/screenshots").apply { mkdirs() }
        val v = rule.activity.window.decorView
        val bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
        v.draw(Canvas(bmp))
        File(dir, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    /** a stand-in cover: solid colour blocks */
    private fun fakeCover(seed: Int): ImageBitmap {
        val b = Bitmap.createBitmap(120, 160, Bitmap.Config.ARGB_8888)
        val colors = intArrayOf(0xFF2B4C7E.toInt(), 0xFF7E2B2B.toInt(), 0xFF2B7E4C.toInt(), 0xFF6B4C9A.toInt())
        for (y in 0 until 160) for (x in 0 until 120) b.setPixel(x, y, if (y < 30) 0xFF101014.toInt() else colors[(seed + y / 40) % 4])
        return b.asImageBitmap()
    }

    private val status = Status("0.8.1", 5148, false, false, 7, true, true, "Getting box art (412 left)", 412, 380, 57)
    private val systems = listOf(SystemItem("PSX", "PlayStation", 1203), SystemItem("SNES", "Super NES", 842), SystemItem("NES", "NES / Famicom", 911),
        SystemItem("Genesis", "Mega Drive / Genesis", 702), SystemItem("Saturn", "Saturn", 214), SystemItem("N64", "Nintendo 64", 388))
    private val games = listOf(
        GameItem(1, "Final Fantasy Tactics (USA)", "PSX", true, true, false, true, "found"),
        GameItem(2, "Final Fantasy Tactics - The Lion War (TLW2)", "PSX", false, false, true, false, "linked"),
        GameItem(3, "Final Fantasy VII (USA) (Disc 1)", "PSX", true, true, false, false, "found"),
        GameItem(4, "Front Mission 3 (USA)", "PSX", true, false, false, false, "found"),
        GameItem(5, "Gran Turismo 2 (USA) (Arcade Disc)", "PSX", false, false, false, false, "not_found"),
        GameItem(6, "Legend of Dragoon, The (USA) (Disc 1)", "PSX", true, true, false, false, "found"),
        GameItem(7, "Metal Gear Solid (USA) (Disc 1)", "PSX", true, true, false, true, "found"),
    )
    private val game = Game(1, "Final Fantasy Tactics (USA)", "PSX", "PlayStation", "1998", "Square", "Sony Computer Entertainment America",
        "Tactical RPG", "1", "USA",
        "A war over the throne of Ivalice pulls a young noble into a conflict of class, faith and betrayal, fought out on isometric battlefields with a deep job system.",
        true, true, 14, 151200, null, null, "found",
        listOf(GameFile("Final Fantasy Tactics (USA).chd", "USB / NVMe drive", 541315152, "1c4e8ff2", "SCUS-94221", true)))

    @Test fun connect() = shot("1-connect") {
        ConnectContent(listOf(Info("192.168.1.23:8767", "0.8.1")), false, "192.168.1.23:8767", "4246", false, null, {}, {}, {}, {}, {})
    }
    @Test fun home() = shot("2-home") { HomeContent("192.168.1.23:8767", status, systems, null, { _, _ -> }, {}, {}, {}) }
    @Test fun gamesList() = shot("3-games") {
        GamesContent("PlayStation", false, null, "", 1203, games, null, art = { g -> if (g.art) fakeCover(g.id.toInt()) else null }, {}, {}, {}, {})
    }
    @Test fun gamesFiltered() = shot("4-no-art") {
        GamesContent("No box art", true, "missing_art", "", 412, games.filter { !it.art }, null, art = { null }, {}, {}, {}, {})
    }
    @Test fun gameDetail() = shot("5-game") { GameContent(game, fakeCover(0), null, null, {}, {}, {}, {}, {}, {}, {}) }
    @Test fun edit() = shot("6-edit") { EditContent(game.title, gameValues(game), false, null, { _, _ -> }, {}, {}) }
    @Test fun basedOn() = shot("7-based-on") {
        BasedOnContent("Final Fantasy Tactics - The Lion War (TLW2)", "Final Fantasy Tactics (USA)", "tactics",
            listOf(Candidate(1, "Final Fantasy Tactics (USA)", 520), Candidate(9, "Final Fantasy Tactics (Japan)", 470), Candidate(4, "Front Mission 3 (USA)", 120)),
            null, {}, {}, {}, {})
    }
    @Test fun settings() = shot("8-settings") { SettingsContent("192.168.1.23:8767", "0.1.0", {}, {}) }

    private val cloud = CloudState(true, true, "nas:Phosphor", "MiSTer-a1b2c3", "2026-10-22 11:45", false, "",
        listOf(CloudPlace("nas", "webdav", true), CloudPlace("drive", "drive", false)), "push", false, true, "", 0)
    private fun cloudShot(name: String, st: CloudState, mode: CloudMode, lines: List<String> = emptyList(), form: AddForm = AddForm()) = shot(name) {
        CloudContent(st, null, mode, lines, "MiSTer-a1b2c3", form, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})
    }
    @Test fun cloudMain() = cloudShot("9-cloud", cloud, CloudMode.MAIN)
    @Test fun cloudPreview() = cloudShot("10-cloud-preview", cloud, CloudMode.PREVIEW, listOf("Would get (2):", "  saves/SNES/Zelda.sav", "  saves/PSX/Final Fantasy Tactics (USA).mcd"))
    @Test fun cloudAdd() = cloudShot("11-cloud-add", cloud, CloudMode.ADD, form = AddForm(kind = "drive"))

    private val snaps = listOf(Snapshot("snap-1", "2026-10-22 12:25", "exit-SNES", 812), Snapshot("snap-2", "2026-10-22 08:00", "daily", 804),
        Snapshot("snap-3", "2026-10-21 21:12", "pre-restore", 790))
    private val backups = BackupState(true, "/media/usb0/phosphor-snapshots", "", snaps, "", false, true, "", 0)
    @Test fun backupsList() = shot("12-backups") { BackupsContent(backups, null, null, emptyList(), {}, {}, {}, {}, {}) }
    @Test fun backupsPreview() = shot("13-backups-restore") {
        BackupsContent(backups, null, snaps[0], listOf("saves/SNES/Super Metroid (USA).sav", "saves/PSX/Final Fantasy Tactics (USA).mcd"), {}, {}, {}, {}, {})
    }
}
