// Matching to base games and fan art (Match.kt, FanArt.kt): the same cases as Phosphor for Windows'
// automatch_test.go, so both apps decide the same way.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import com.f3bandit.phosphor.scrape.AmLibGame
import com.f3bandit.phosphor.scrape.FanArt
import com.f3bandit.phosphor.scrape.LrEntry
import com.f3bandit.phosphor.scrape.amClean
import com.f3bandit.phosphor.scrape.amGlued
import com.f3bandit.phosphor.scrape.amKey
import com.f3bandit.phosphor.scrape.amKind
import com.f3bandit.phosphor.scrape.amLibBase
import com.f3bandit.phosphor.scrape.amLoose
import com.f3bandit.phosphor.scrape.amNotGame
import com.f3bandit.phosphor.scrape.amPortFrom
import com.f3bandit.phosphor.scrape.amStrip
import com.f3bandit.phosphor.scrape.fanExact
import com.f3bandit.phosphor.scrape.fanKey
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34])
class MatchTest {
    @Test fun kind() {
        for ((t, k, tag) in listOf(
            Triple("Bahamut Lagoon (Japan) [T-En by DeJap]", "translation", "[T-En by DeJap]"),
            Triple("Mother 3 (Japan) (Translated En)", "translation", "(Translated En)"),
            Triple("Super Mario World - Kaizo (Hack)", "hack", ""),
            Triple("Zelda - Outlands (USA) [h1]", "hack", "[h1]"),
            Triple("Super Metroid Redux", "fan", "Redux"),
            Triple("Gradius III FastROM (USA)", "fan", "FastROM"),
            Triple("Super Mario World (USA)", "", ""),
            Triple("Hackers (USA)", "", ""),
            Triple("Translation Party (World)", "", ""),
            Triple("Super Mario Bros. (SNES Port)", "port", "(SNES Port)"),
            Triple("Zelda II - The Adventure of Link NES2SNES", "port", "NES2SNES"),
            Triple("Port Royale (Europe)", "", ""),
            Triple("Valis IV ENG DUB", "translation", ""),
            Triple("Exile II - Un-Worked Designs (Hack v1)", "hack", ""),
        )) {
            val (gk, gt) = amKind(t)
            assertEquals(t, k, gk)
            if (tag.isNotEmpty()) assertEquals(t, tag, gt)
        }
    }

    @Test fun strip() {
        for ((a, b) in listOf(
            "Bahamut Lagoon (Japan) [T-En by DeJap]" to "Bahamut Lagoon",
            "Super Mario World - Kaizo (Hack)" to "Super Mario World",
            "Super Metroid Redux" to "Super Metroid",
            "Chrono Trigger v1.1 (USA)" to "Chrono Trigger",
            "Zelda II - The Adventure of Link NES2SNES" to "Zelda II - The Adventure of Link",
            "Super Mario Bros. (SNES Port)" to "Super Mario Bros.",
        )) assertEquals(a, b, amStrip(a))
    }

    @Test fun clean() {
        for ((a, b) in listOf(
            "Zeldas.Adventure.DX.v2.0.0" to "Zeldas Adventure DX",
            "SuperMarioBrosMini" to "Super Mario Bros Mini",
            "Valis_IV_ ENG_DUB" to "Valis IV ENG DUB",
            "AkiraGBC1.35" to "Akira GBC",
            "Glory Hunters 1.3" to "Glory Hunters",
            "Super Mario Bros. 3 (USA)" to "Super Mario Bros. 3 (USA)",
        )) assertEquals(a, b, amClean(a))
        assertEquals("kid icarus", amGlued("kid icarusnes").getOrNull(1))
    }

    @Test fun notGame() {
        for (n in listOf("mister-boot", "boot0", "BtnTest", "N64 Controller Tester", "GameShark Lite (USA) (Unl)", "Empty"))
            assertTrue(n, amNotGame(amClean(n)).isNotEmpty())
        for (n in listOf("Boot Hill (USA)", "Bootleggers")) assertEquals(n, "", amNotGame(amClean(n)))
    }

    @Test fun key() {
        for ((a, b) in listOf("Romancing SaGa 3" to "Romancing Sa-Ga 3 (Japan)", "Final Fantasy 2" to "Final Fantasy II (Japan)", "Pac Man" to "Pac-Man (USA)"))
            assertEquals(a, amKey(a), amKey(b))
    }

    @Test fun libBase() {
        val lib = listOf(
            AmLibGame(1, "Bahamut Lagoon (Japan)", false, true), AmLibGame(2, "Super Mario World (USA)", false, true),
            AmLibGame(3, "Super Mario World (Europe)", false, false), AmLibGame(4, "Super Metroid (Japan, USA)", false, true),
            AmLibGame(5, "Castlevania - Dracula X (USA)", false, true), AmLibGame(7, "Super Mario World - Kaizo (Hack)", false, false),
            AmLibGame(8, "Gradius III (USA)", true, true),
        )
        for ((t, want) in listOf("Bahamut Lagoon (Japan) [T-En by DeJap]" to 1L, "Super Mario World - Kaizo (Hack)" to 2L, "Super Metroid Redux" to 4L,
            "Gradius III FastROM (USA)" to 0L, "Castlevania - Some Hack (Hack)" to 0L))
            assertEquals(t, want, amLibBase(99, t, lib)?.id ?: 0L)
        // a port: the NES original
        val nes = listOf(AmLibGame(1, "Super Mario Bros. (World)", false, true), AmLibGame(2, "Metroid (USA)", false, true))
        assertEquals(1L, amLibBase(9, "Super Mario Bros. (SNES Port)", nes)?.id)
        assertEquals("NES", amPortFrom("SNES", "(SNES Port)"))
        assertEquals("", amPortFrom("NES", "(Port)"))
    }

    @Test fun loose() {
        val all = listOf("Rockman 7 - Shukumei no Taiketsu! (Japan)", "Mega Man 7 (USA)", "Castlevania - Dracula X (USA)", "Castlevania - Bloodlines (USA)",
            "Castlevania (USA)", "Legend of Zelda, The - A Link to the Past (USA)", "Dragon Ball Z - Super Saiya Densetsu (Japan)",
            "Dragon Ball Z - Super Butouden (Japan)", "Dragon Ball Z - Super Butouden 2 (Japan)", "Tengai Makyou - Deden no Den (Japan)",
            "Tengai Makyou - Fuuun Kabuki Den (Japan)", "Star Ocean - The Second Story (USA) (Disc 1)", "Star Ocean - The Second Story (USA) (Disc 2)",
            "Kishin Douji Zenki - Battle Raiden (Japan)", "Kishin Douji Zenki - Tenchi Meidou (Japan)", "Fushigi no Dungeon 2 - Fuurai no Shiren (Japan)",
            "Nangoku Shounen Papuwa-kun (Japan)", "Final Fantasy Legend, The (USA)", "Final Fantasy Adventure (USA)", "Final Fantasy Mystic Quest (USA)",
            "Army Men - World War - Final Front (USA)")
            .map { LrEntry(it, "", "", 0, "") }
        for ((t, want) in listOf(
            "Rockman 7 (Japan)" to "Rockman 7 - Shukumei no Taiketsu! (Japan)",
            "Legend of Zelda - A Link to the Past (USA)" to "Legend of Zelda, The - A Link to the Past (USA)",
            "Castlevania - Something (USA)" to "",
            "Dragon-Ball Z - Super Saiyan Densetsu" to "Dragon Ball Z - Super Saiya Densetsu (Japan)",
            "Tengai Makyou - Ziria (English v1.2)" to "",
            "Dragon-Ball-Z - Super Gokuden 2" to "",
            "Star Ocean - The Second Story - Disc 2 (U) [UNDUB]" to "Star Ocean - The Second Story (USA) (Disc 2)",
            "Demon Child Zenki - Battle Raiden" to "Kishin Douji Zenki - Battle Raiden (Japan)",
            "Mysterious Dungeon 2 - Shiren the Wanderer" to "Fushigi no Dungeon 2 - Fuurai no Shiren (Japan)",
            "Nankoku Shounen Papuwa-kun" to "Nangoku Shounen Papuwa-kun (Japan)",
            "Final Fantasy Tactics" to "",
            "Final Fantasy Tactics FF6 - World of Ruin" to "",
        )) assertEquals(t, want, amLoose(all, t).first?.name ?: "")
    }

    @Test fun fanArt() {
        val f = File("../../windows/fanart/fanart.json").takeIf { it.exists() } ?: File("../windows/fanart/fanart.json")
        val m = FanArt.index(f.readText())
        assertNotNull(m)
        fun find(sys: String, t: String) = m!!["=$sys|${fanExact(t)}"] ?: m["$sys|${fanKey(t)}"]
        for ((t, want) in listOf("Roll-chan (MM1)" to "Roll-chan", "Roll-chan 2 (MM11)" to "Roll-chan 2", "Roll-chan 3 Improvement (Classic)" to "Roll-chan 3",
            "Roll-chan 6 (MM8)" to "Roll-chan 6", "Roll-chan No Constancy (v1.3)(normal)" to "Roll-chan No Constancy"))
            assertEquals(t, want, find("NES", t)?.names?.first())
        assertEquals(null, find("SNES", "Roll-chan 2"))
    }
}

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34])
class SsLoginTest {
    @Test fun answers() {
        val dev = com.f3bandit.phosphor.scrape.ssAnswer(2, 403, "Erreur de login : Vérifier vos identifiants développeur !".toByteArray())
        assertTrue(dev.second && "developer ID" in dev.first)
        val user = com.f3bandit.phosphor.scrape.ssAnswer(2, 403, "Erreur de login : Vérifier les identifiants utilisateurs !".toByteArray())
        assertTrue(user.second && "your login" in user.first)
        assertTrue(com.f3bandit.phosphor.scrape.ssAnswer(1, 430, null).second)
        assertEquals("" to false, com.f3bandit.phosphor.scrape.ssAnswer(0, 200, "{}".toByteArray()))
        assertEquals(false, com.f3bandit.phosphor.scrape.ssAnswer(-1, 0, null).second)
    }

    @Test fun missing() {
        fun acct(m: Map<String, String>): (String, String) -> String = { _, k -> m[k] ?: "" }
        assertTrue("developer ID" in com.f3bandit.phosphor.scrape.ssMissing(acct(mapOf("user" to "u", "password" to "p"))))
        assertEquals("", com.f3bandit.phosphor.scrape.ssMissing(acct(mapOf("user" to "u", "password" to "p", "devid" to "d", "devpassword" to "x"))))
        com.f3bandit.phosphor.scrape.ssBuiltinId = "phosphor"; com.f3bandit.phosphor.scrape.ssBuiltinPw = "s"
        try {
            assertEquals("", com.f3bandit.phosphor.scrape.ssMissing(acct(mapOf("user" to "u", "password" to "p"))))
        } finally { com.f3bandit.phosphor.scrape.ssBuiltinId = ""; com.f3bandit.phosphor.scrape.ssBuiltinPw = "" }
    }
}
