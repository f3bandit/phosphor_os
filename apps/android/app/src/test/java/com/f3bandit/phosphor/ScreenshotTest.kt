// Draws every screen with sample data into app/build/screenshots/ (no phone or emulator needed), inside
// the app's navigation (app bar, bottom bar or rail) the way it shows on a phone or tablet.
package com.f3bandit.phosphor

import android.graphics.Bitmap
import android.graphics.Canvas
import androidx.activity.ComponentActivity
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import com.f3bandit.phosphor.scrape.PhoneScrapeState
import com.f3bandit.phosphor.scrape.StoredArt
import com.f3bandit.phosphor.ui.AboutContent
import com.f3bandit.phosphor.ui.AddForm
import com.f3bandit.phosphor.ui.AppShell
import com.f3bandit.phosphor.ui.BOXART_TABS
import com.f3bandit.phosphor.ui.BackupsContent
import com.f3bandit.phosphor.ui.BasedOnContent
import com.f3bandit.phosphor.ui.BluetoothContent
import com.f3bandit.phosphor.ui.BoxArtLookupContent
import com.f3bandit.phosphor.ui.BoxArtSourcesContent
import com.f3bandit.phosphor.ui.CardsContent
import com.f3bandit.phosphor.ui.CloudContent
import com.f3bandit.phosphor.ui.CloudMode
import com.f3bandit.phosphor.ui.Conn
import com.f3bandit.phosphor.ui.ConnectContent
import com.f3bandit.phosphor.ui.ConnectionContent
import com.f3bandit.phosphor.ui.DiagContent
import com.f3bandit.phosphor.ui.EditContent
import com.f3bandit.phosphor.ui.GameContent
import com.f3bandit.phosphor.ui.GamesContent
import com.f3bandit.phosphor.ui.GuardContent
import com.f3bandit.phosphor.ui.HomeContent
import com.f3bandit.phosphor.ui.LibraryContent
import com.f3bandit.phosphor.ui.LocalTabs
import com.f3bandit.phosphor.ui.LookContent
import com.f3bandit.phosphor.ui.NotesContent
import com.f3bandit.phosphor.ui.POWER_TABS
import com.f3bandit.phosphor.ui.PhoneStorageContent
import com.f3bandit.phosphor.ui.PhosphorTheme
import com.f3bandit.phosphor.ui.PowerContent
import com.f3bandit.phosphor.ui.STORAGE_TABS
import com.f3bandit.phosphor.ui.Section
import com.f3bandit.phosphor.ui.StorageDrivesContent
import com.f3bandit.phosphor.ui.TabsSpec
import com.f3bandit.phosphor.ui.UpdateContent
import com.f3bandit.phosphor.ui.VIDEO_SLOTS
import com.f3bandit.phosphor.ui.VideoListContent
import com.f3bandit.phosphor.ui.VideoSysContent
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

    private val conn = Conn("Connected", true)

    /** one screen, inside the app's navigation for section (null: on its own, like pairing) */
    private fun shot(name: String, section: Section? = Section.HOME, texture: Int = 0, tabs: List<String>? = null, tab: Int = 0,
                     badge: Set<Section> = emptySet(), more: Boolean = false, content: @Composable () -> Unit) {
        com.f3bandit.phosphor.ui.P.accent = com.f3bandit.phosphor.ui.ACCENTS.getValue("amber")
        rule.setContent {
            PhosphorTheme {
                AppShell(section, if (section == null) null else conn, texture, badge, {}, {}, startMore = more) {
                    if (tabs != null) CompositionLocalProvider(LocalTabs provides TabsSpec(tabs, tab) {}) { content() } else content()
                }
            }
        }
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
    private val cats = listOf(Category(0, "Consoles", 4100), Category(1, "Handhelds", 640), Category(2, "Arcade", 408))
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

    @Test fun connect() = shot("1-connect", null) {
        ConnectContent(listOf(Info("192.168.1.23:8767", "0.8.1")), false, "192.168.1.23:8767", "4246", false, null, {}, {}, {}, {}, {})
    }
    @Test fun home() = shot("2-home", Section.HOME, badge = setOf(Section.NOTES)) { HomeContent(status, game, fakeCover(0), null, {}, {}, { _, _ -> }, {}) }
    @Test fun library() = shot("2b-library", Section.LIBRARY) {
        LibraryContent(cats, 1, systems, "", null, 0, status, null, { null }, {}, {}, {}, {}, {})
    }
    @Test fun gamesList() = shot("3-games", Section.LIBRARY) {
        GamesContent("PlayStation", false, null, "", 1203, games, null, art = { g -> if (g.art) fakeCover(g.id.toInt()) else null }, {}, {}, {}, {})
    }
    @Test fun gamesFiltered() = shot("4-no-art", Section.HOME) {
        GamesContent("No box art", true, "missing_art", "", 412, games.filter { !it.art }, null, art = { null }, {}, {}, {}, {})
    }
    @Test fun gameDetail() = shot("5-game", Section.LIBRARY) { GameContent(game, fakeCover(0), null, null, {}, {}, {}, {}, {}, {}, {}) }
    @Test fun edit() = shot("6-edit", Section.LIBRARY) { EditContent(game.title, gameValues(game), false, null, { _, _ -> }, {}, {}) }
    @Test fun basedOn() = shot("7-based-on", Section.LIBRARY) {
        BasedOnContent("Final Fantasy Tactics - The Lion War (TLW2)", "Final Fantasy Tactics (USA)", "tactics",
            listOf(Candidate(1, "Final Fantasy Tactics (USA)", 520), Candidate(9, "Final Fantasy Tactics (Japan)", 470), Candidate(4, "Front Mission 3 (USA)", 120)),
            null, {}, {}, {}, {})
    }

    private val savesTabs = listOf("Memory cards", "Backups", "Cloud")
    private val cloud = CloudState(true, true, "nas:Phosphor", "MiSTer-a1b2c3", "2026-10-22 11:45", false, "",
        listOf(CloudPlace("nas", "webdav", true), CloudPlace("drive", "drive", false)), "push", false, true, "", 0)
    private fun cloudShot(name: String, st: CloudState, mode: CloudMode, lines: List<String> = emptyList(), form: AddForm = AddForm()) = shot(name, Section.SAVES, tabs = savesTabs, tab = 2) {
        CloudContent(st, null, mode, lines, "MiSTer-a1b2c3", form, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})
    }
    @Test fun cloudMain() = cloudShot("9-cloud", cloud, CloudMode.MAIN)
    @Test fun cloudPreview() = cloudShot("10-cloud-preview", cloud, CloudMode.PREVIEW, listOf("Would get (2):", "  saves/SNES/Zelda.sav", "  saves/PSX/Final Fantasy Tactics (USA).mcd"))
    @Test fun cloudAdd() = cloudShot("11-cloud-add", cloud, CloudMode.ADD, form = AddForm(kind = "drive"))

    private val snaps = listOf(Snapshot("snap-1", "2026-10-22 12:25", "exit-SNES", 812), Snapshot("snap-2", "2026-10-22 08:00", "daily", 804),
        Snapshot("snap-3", "2026-10-21 21:12", "pre-restore", 790))
    private val backups = BackupState(true, "/media/usb0/phosphor-snapshots", "", snaps, "", false, true, "", 0)
    @Test fun backupsList() = shot("12-backups", Section.SAVES, tabs = savesTabs, tab = 1) { BackupsContent(backups, null, null, emptyList(), {}, {}, {}, {}, {}) }
    @Test fun backupsPreview() = shot("13-backups-restore", Section.SAVES, tabs = savesTabs, tab = 1) {
        BackupsContent(backups, null, snaps[0], listOf("saves/SNES/Super Metroid (USA).sav", "saves/PSX/Final Fantasy Tactics (USA).mcd"), {}, {}, {}, {}, {})
    }

    private val cardFiles = listOf(CardBackup("port1-20261022-133030.mcd", 131072, 1792690230), CardBackup("port2-20261020-091502.mcd", 131072, 1792500902))
    private val cards = CardState(true, true, false, false, true, "", 0, false, false, 0, 1, "", "", cardFiles)
    private val padsNow = listOf(Pad(1, true, 0x73, 0, 128, 128, 128, 128), Pad(2, true, 0x41, 0, 0, 0, 0, 0))
    @Test fun cards() = shot("14-cards", Section.SAVES, tabs = savesTabs) { CardsContent(cards, padsNow, null, null, {}, {}, {}, {}, {}, {}) }
    @Test fun cardsBusy() = shot("15-cards-busy", Section.SAVES, tabs = savesTabs) { CardsContent(cards.copy(busy = true, job = "backup", port = 1, done = 512, total = 1024), padsNow, null, null, {}, {}, {}, {}, {}, {}) }
    @Test fun cardsPutBack() = shot("16-cards-put-back", Section.SAVES, tabs = savesTabs) { CardsContent(cards, padsNow, null, cardFiles[0], {}, {}, {}, {}, {}, {}) }
    private val bt = BtState(true, listOf(BtDevice("11:22:33:44:55:66", "8BitDo Pro 2", true, true), BtDevice("AA:BB:CC:11:22:33", "DualSense Wireless Controller", false, true)), false, false, 0, "", "")
    @Test fun bluetooth() = shot("17-bluetooth", Section.BLUETOOTH) { BluetoothContent(bt, null, {}) }
    private val vids = listOf(
        VideoSys("SNES", "Super NES", true, "Scaling/Pixelated.txt", null, "Scanlines/Scanlines_50.txt", null, "1x", "Shadow_Masks/Trinitron.txt", "Gamma/NTSC.txt"),
        VideoSys("PSX", "PlayStation", false, null, null, null, null, "off", null, null))
    @Test fun video() = shot("19-video", Section.VIDEO) { VideoListContent(vids, null, {}, {}) }
    @Test fun videoSys() = shot("20-video-system", Section.VIDEO) { VideoSysContent(vids[0], null, null, null, {}, {}, {}, {}, {}) }
    @Test fun videoPick() = shot("21-video-pick", Section.VIDEO) { VideoSysContent(vids[0], null, VIDEO_SLOTS[2], listOf("Scanlines/Scanlines_25.txt", "Scanlines/Scanlines_50.txt", "Scanlines/Scanlines_75.txt"), {}, {}, {}, {}, {}) }
    @Test fun diag() = shot("22-diag", Section.DIAG) {
        DiagContent(DiagState(true, false, 1792690000, "warn", "2 things to look at", "", listOf(
            DiagItem("storage", "sd", "ok", "SD card: 9.3 GB free"), DiagItem("storage", "fsck", "warn", "SD card wasn't shut down cleanly last time"),
            DiagItem("network", "wifi", "ok", "WiFi connected (strong)"), DiagItem("cores", "menu", "warn", "menu.rbf is older than the MiSTer release"))), null, {}, {})
    }

    // ---- 0.8: More, box art on the phone, power & updates, storage, look, notifications

    @Test fun more() = shot("23-more", Section.HOME, badge = setOf(Section.NOTES), more = true) { BoxArtLookupContent(PhoneScrapeState(), ScrapeProgress(false, "", "", "", "", 0, 0, 0, 0, 0), null, {}, {}) }
    @Test fun boxartRunning() = shot("24-boxart-phone", Section.BOXART, tabs = BOXART_TABS) {
        BoxArtLookupContent(PhoneScrapeState(true, "Looking up box art", 412, 128, 117, 9, 2, "Gran Turismo 2 (USA) (Arcade Disc)", "PlayStation", started = 1),
            ScrapeProgress(false, "Waiting: the phone is looking up box art", "", "", "", 0, 0, 0, 0, 0), null, {}, {})
    }
    private val srcs = listOf(ScrapeSource("libretro", "libretro", true, true, null), ScrapeSource("arcadedb", "ArcadeDB", true, true, null),
        ScrapeSource("launchbox", "LaunchBox", true, true, null), ScrapeSource("screenscraper", "ScreenScraper", true, false, "login"),
        ScrapeSource("thegamesdb", "TheGamesDB", false, false, "apikey"), ScrapeSource("igdb", "IGDB", false, false, "client"))
    @Test fun boxartSources() = shot("25-boxart-sources", Section.BOXART, tabs = BOXART_TABS, tab = 1) {
        BoxArtSourcesContent(srcs, listOf(ScrapeAccount("screenscraper", "user", true, "f3bandit")), 3, mapOf("user" to "f3bandit"), null, false, {}, {}, { _, _ -> }, { _, _ -> }, {})
    }
    @Test fun power() = shot("26-power", Section.POWER, tabs = POWER_TABS) {
        PowerContent(PowerState(true, 3, 3, true, listOf(PowerEvent(1792690000, "Power lost while writing saves"), PowerEvent(1792500000, "Power lost during a backup"))), null, {}, {})
    }
    @Test fun update() = shot("27-update", Section.POWER, tabs = POWER_TABS, tab = 1) {
        UpdateContent(UpdateState(true, "0.27.16", listOf(UpdatePackage("/media/fat/phosphor/updates/phosphor-0.28.0-armhf.tar.gz", "0.28.0")),
            listOf(Rollback("0.27.12", "2026-10-12")), "", "", 0), null, {}, {})
    }
    @Test fun guard() = shot("28-restore-points", Section.POWER, tabs = POWER_TABS, tab = 2) {
        GuardContent(GuardState(true, "250402", "2026-10-20 21:04", "241103", true, listOf(GuardFile("/media/fat/MiSTer.ini", true), GuardFile("/root/.ssh/authorized_keys", true)),
            listOf(RestorePoint("rp-1", "Before MiSTer Linux 250402", "2026-10-20 21:02", "241103", true)), null), null, {}, {}, {})
    }
    private val drives = StorageState(true, listOf(
        Drive("/media/fat", "sd", "exFAT", 124_000_000, 9_700_000, "", "", "", true),
        Drive("/media/usb0", "nvme", "exFAT", 488_000_000, 21_000_000, "Corsair MP600", "gen4", "8.2", true)), emptyList(),
        StorageCheck("quick", "/media/fat", false, true, 1792690000, "Partitions: OK\nFile system: OK\nRESULT: PASS"))
    @Test fun storageSd() = shot("29-storage-sd", Section.STORAGE, tabs = STORAGE_TABS) { StorageDrivesContent(drives, true, null, { _, _ -> }, {}) }
    @Test fun storageNvme() = shot("30-storage-nvme", Section.STORAGE, tabs = STORAGE_TABS, tab = 1) { StorageDrivesContent(drives, false, null, { _, _ -> }, {}) }
    @Test fun storagePhone() = shot("31-storage-phone", Section.STORAGE, tabs = STORAGE_TABS, tab = 2) {
        PhoneStorageContent(listOf(StoredArt(5, "Gran Turismo 2 (USA)", "PlayStation", 1792690000000, false, 48_000),
            StoredArt(4, "Front Mission 3 (USA)", "PlayStation", 1792680000000, true, 52_000)), 100_000, 152_000_000, true, false, {}, {}, {}, {})
    }
    @Test fun look() = shot("32-look", Section.LOOK, texture = 6) { LookContent("amber", 6, {}, {}) }
    @Test fun lookGreen() = shot("33-look-circuit", Section.LOOK, texture = 24) {
        com.f3bandit.phosphor.ui.P.accent = com.f3bandit.phosphor.ui.ACCENTS.getValue("green")
        LookContent("green", 24, {}, {})
    }
    @Test fun notes() = shot("34-notifications", Section.NOTES) {
        NotesContent(listOf(
            Note("power-3", 1792690000000, "warn", "Power supply problem?", "Your MiSTer lost power 3 times in the middle of something.", false, "power"),
            Note("update-0.28.0", 1792680000000, "info", "Phosphor 0.28.0 is ready to install", "It's on the MiSTer (installed: 0.27.16).", true, "power/1")), true, true, {}, {}, {}, {})
    }
    @Test fun connection() = shot("35-connection", Section.CONNECTION) { ConnectionContent("192.168.1.23:8767", true, "0.27.16", false, null, {}, {}) }
    @Test fun about() = shot("36-about", Section.ABOUT) { AboutContent("0.8.0") }
    @Config(qualifiers = "w900dp-h600dp-xhdpi")
    @Test fun tablet() = shot("37-tablet", Section.LIBRARY) {
        LibraryContent(cats, 0, systems, "", null, 0, status, null, { null }, {}, {}, {}, {}, {})
    }
}
