# Roadmap

Each step is useful on its own. Order matters: later steps reuse earlier ones.

1. **cifs-helper module.** Mount on DHCP bound, retry with backoff, port-445 check, lock, `soft` mounts,
   stale-mount health check, boot log, hook reinstalled from `user-startup.sh`. Test on a stock MiSTer
   Pi with WiFi first.
2. **backup-agent module.** Saves and frontend settings, on game exit or a schedule.
3. **Main fork (`phosphor-main`).** Track MiSTer-devel/Main_MiSTer; add the local API (launch, pause,
   snapshot slots, framebuffer switch, last-frame grab). Keep the diff small; CI flags upstream changes.
4. **Menu core fork (`phosphor-menu`).** Track MiSTer-devel/Menu_MiSTer; add the SNAC pad reader and PS1
   memory card SIO from the SS1 Controller Tester and SS1 MemCard cores.
5. **Frontend MVP.** Library shelf, game list, launch, settings, mount-change rescan.
6. **Memory Cards and Controllers screens** over the Main API (no core swap).
7. **In-game sheet.** HDMI/scaler first; OSD fallback for direct-video analog.
8. **health-watch and update-guard modules.**
9. **Release prep.** Name and trademark check, publish the GPL forks, own downloader database for installs.

## Open questions

- Frontend language/toolkit: Rust + Slint (like Degauss and Zaparoo), or Go with a framebuffer renderer?
- Final name (Phosphor is a working name).
