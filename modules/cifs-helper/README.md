# cifs-helper

Mounts network shares reliably on MiSTer, including WiFi cold boots. Works on stock MiSTer, the
SuperStation One, and alongside any frontend.

## Behaviour

1. **Started by the network, not the clock.** A `dhcpcd` exit hook runs it when WiFi or Ethernet gets
   an address (also on reconnects).
2. **Never blocks boot.** Runs detached (`setsid`).
3. **Checks before mounting.** TCP probe on port 445 (no ping); cached last-good server IP; cached
   working SMB version.
4. **Retries with backoff.** 2, 4, 8, 16 s … up to 5 minutes, then stops and logs why.
5. **One at a time.** Lock file plus "already mounted?" check.
6. **Doesn't freeze.** `soft` mount options; health check every 60 s with a time-limited `stat`;
   stale mount → lazy unmount and remount.
7. **Survives Linux updates.** Hook copy on the SD card; `user-startup.sh` reinstalls it when missing.
8. **Leaves a record.** `/tmp/phosphor/cifs.log` and a last-boot summary on the SD card
   (network up at X s, mounted at Y s, or failed because Z).

## Config

Reads MiSTer's existing `/media/fat/Scripts/cifs_mount.ini` (SERVER, SHARE, USERNAME, PASSWORD,
LOCAL_DIR, ADDITIONAL_MOUNT_OPTIONS) so it needs no new settings, and switches off the stock boot
hook so the two never mount at once.

## Status

Design only.
