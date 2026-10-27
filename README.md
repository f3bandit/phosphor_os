# Phosphor

Unofficial custom firmware for the SuperStation One, built on mainline MiSTer and its Linux: a
console-style home screen with your library and box art, the front-port controllers and memory cards,
Bluetooth, WiFi, and saves, all with a controller, plus a phone app. Working name;
private while in development. Not made or supported by MiSTer-devel or the SuperStation One's makers.

# Phosphor

> **A simple, streamlined, console-style experience built *with* stock MiSTer FPGA.**

Phosphor bridges the gap between advanced hardware tinkering and a turn-key retro gaming console. It is designed to make MiSTer accessible to everyone—bringing back the simple "plug and play" feel of classic hardware while preserving the full power and openness of the MiSTer ecosystem.

---

## 💡 Core Philosophy

Phosphor is built **with** stock MiSTer, not against it:
* **Non-destructive:** Uses `MiSTer-devel`'s Linux and kernel exactly as they ship. Nothing on your SD card is overwritten or replaced.
* **Zero Overhead:** No background polling processes or unnecessary system strain.
* **Always Accessible:** MiSTer's native menu is always just one button press away.
* **No Extra Setup:** Everything is baked in and ready out of the box—no downloading additional packages for basic functionality.

Phosphor takes the workload off users and saves developers from troubleshooting issues caused by misconfigured settings.

---

## 🎛️ Choice & Coexistence

The MiSTer ecosystem shouldn't be restricted to advanced power users. Phosphor offers a choice: whether you prefer classic MiSTer, an Analogue-style console interface, or a mainstream console experience, you decide how you play.

> **Note:** Neither Phosphor nor `ss1-tool` are replacements for Console Mode or MiSTer Companion, nor are they intended to compete with them. They are simply options for the community.

A lot of Phosphor's underlying code is brought over from `ss1-tool` (originally built for Retro Remake's console mode) to streamline things for both basic and power users alike. We will always support and collaborate with open-source developers across the MiSTer space—including Console Mode, Retro Remake, Taki Udon, animeotaku, and all core and tool creators.

---

## 🔒 Privacy & User Control

Phosphor and its associated applications strictly adhere to user-first privacy principles:

- 🚫 **Zero Tracking:** No telemetry, data collection, ads, or targeted tracking.
- 🔐 **100% Privacy:** Your data and hardware belong to you.
- 📡 **Full Transparency:** Nothing is sent anywhere automatically. Any network traffic is fully disclosed and strictly limited to connecting your device to user-enabled services.
- ⚙️ **Complete Control:** Nothing happens on your system without your explicit action.

---

## 🕹️ Why Phosphor Exists

Phosphor was built because creators like Taki Udon and Retro Remake demonstrated that open hardware can directly compete with commercial manufacturers using FOMO tactics, artificial scarcity, or releasing clone hardware with abandoned software (like the old flashcard "timebomb" era or just move onto the next product, and never update older products they built their brand on).

By bringing the console form factor back to MiSTer with the SS1, Taki Udon and Retro Remake introduced healthy competition to the retro gaming market. Every developer building hardware, software, tools, and cores makes this community stronger and more vibrant.

---

<p align="center">
  <b>Choices are good for everyone. Play your games your way.<br>Games are meant to be fun—not a political debate.</b>
</p>

## What it is

| Part | What it does | Where |
|---|---|---|
| Shell | The interface: library shelf (categories, Favorites, Recently played), search, game details and box art, Settings (WiFi, Bluetooth, controllers and memory cards, screen filters, backups, cloud saves, remote access, screensaver, About), Welcome screen | `frontend/` |
| System service | `phosphord`: library database, indexer, box art and details from six sources, play history, launching, front ports, Bluetooth, WiFi, remote access for the phone; `phos` command line | `system/` |
| Main fork | MiSTer Main with a small local API: the shell takes the screen, Launch Phosphor in the OSD, front-port controllers and memory cards, Phosphor's menu core | [`phosphor-main`](https://github.com/f3bandit/phosphor-main), branch `phosphor` (GPL-3.0, published at release) |
| Menu core fork | MiSTer's menu core plus the front ports (PlayStation controllers and memory cards over SNAC), off until Phosphor asks | [`phosphor-menu`](https://github.com/f3bandit/phosphor-menu), branch `phosphor` (GPL, published at release) |
| Modules | One-job ARM programs that start on an event or on request, then exit | [`phosphor-modules`](https://github.com/f3bandit/phosphor-modules) |
| Android app | Phosphor for Android: library, box art from the phone, details, Based on, play on TV, Favorites, memory cards, Bluetooth (read only), backups, cloud saves, screen filters, diagnostics; over the home network | `apps/android/` |
| SD card image | Ready-to-flash card: MiSTer-devel's release + Phosphor, boots into Phosphor; built from Mr. Fusion | `image/` |
| Website | The page for phosphor-os.github.io | `site/` |
| PC tool | Phosphor Link: the SuperStation One's debug port and Claude library tools on a PC | `tools/phosphor-link/` |

Current: Phosphor 0.17.0 (shell), phosphord 0.12.4, Phosphor for Android 0.7.0. Hardware testing:
[`docs/testing.md`](docs/testing.md).

## Modules

The modules live in [`phosphor-modules`](https://github.com/f3bandit/phosphor-modules) so they can be
installed on their own (SS1 Tool can offer them to stock MiSTer users) and released separately:
cifs-helper, backup-agent, health-watch, diag, cloud-sync, update-guard.

## Rules

- One job per module, clean install and uninstall, a short log in `/tmp`.
- Files live on the SD card (`/media/fat/phosphor/`), never inside `linux.img`; hooks are reinstalled from `user-startup.sh` after Linux updates.
- Nothing busy-waits: blocking reads with timeouts, so idle means zero CPU.
- Only MiSTer's own interfaces to the FPGA: `/dev/MiSTer_cmd`, MGL, core-mounted files, and the Main fork's API.
- Private settings (WiFi networks, Bluetooth pairings, share passwords, keys, device keys) stay on the
  SS1, the NVMe, a NAS or the phone app: never over the internet. Remote access answers the home network
  only and can't read or change WiFi, Bluetooth pairings or remote access itself.
- Every change is backed up first and can be undone; nothing installs, updates or changes itself.
- Original design only: own name, palette, fonts (open licence), icons, sounds and terms.

See [`docs/architecture.md`](docs/architecture.md) and [`docs/roadmap.md`](docs/roadmap.md).

## Credits

- **Taki and Retro Remake**, for making the SuperStation One, such excellent hardware.
- **MiSTer-devel** and the MiSTer community: MiSTer's Linux, Main, the menu core and the cores.
- **Disc support:** Main_MiSTer_Physical_Disc by **Anime0t4ku and bonna79** (GPL-3.0), in phosphor-main.
- **RetroAchievements:** **odelot**'s MiSTer RetroAchievements Main and cores (GPL-3.0), and
  **rcheevos** by the RetroAchievements team (MIT); achievements and badges from retroachievements.org.
- **Music:** MusicBrainz and the Cover Art Archive (CD names and covers); dr_flac, dr_mp3 and dr_wav by
  **David Reid** (public domain or MIT-0); stb_vorbis by **Sean Barrett** (public domain or MIT).

## Licence

[PolyForm Noncommercial 1.0.0](LICENSE.md): use, modify and share for any non-commercial purpose;
selling it or a product built on it is not permitted. The Main and menu core forks are separate
repositories under the GPL, like MiSTer, and public: [phosphor-main](https://github.com/f3bandit/phosphor-main)
and [phosphor-menu](https://github.com/f3bandit/phosphor-menu). [SOURCES.md](SOURCES.md) links the exact
source of every release, the other free software in them, and the credits and disclaimers.
