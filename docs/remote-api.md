# Remote access API

For the Phosphor phone app, a PC app, or the Claude plugin: manage a MiSTer's Phosphor library over the
home network. phosphord 0.8 and later (`games` filters: 0.8.1).

## Turning it on

On the MiSTer: **Settings > Remote access**. It's off until the user turns it on. The screen shows the
address (for example `192.168.1.23:8767`).

- Plain HTTP/1.1, port **8767**, one request per connection (`Connection: close`).
- Only answers home-network addresses (10.x, 172.16-31.x, 192.168.x, 169.254.x, 127.x). Others are
  disconnected without a reply.
- Bodies need `Content-Length` (no chunked uploads). `Expect: 100-continue` is fine.
- Replies are JSON `{"ok":true, ...}` or `{"ok":false, "error":"..."}` (text meant for the user), except
  box art (`image/jpeg`).

## Pairing

1. The user chooses **Pair a device**: the TV shows a 6-digit code for 2 minutes.
2. The app sends it:

   ```
   POST /pair
   {"code":"424690","name":"Pixel 8"}
   ```
   → `{"ok":true,"token":"<64 hex>","id":"<16 hex>"}`

3. The app keeps the token (Android Keystore / Windows Credential Manager) and sends it on every request:
   `Authorization: Bearer <token>`.

One device per code. Five wrong codes close pairing. Wrong codes and keys get a one-second wait.
The MiSTer keeps only a hash of each token. The user can forget a device in Settings, and its token
then gets `401`. When that happens, ask the user to pair again.

| Status | Meaning |
|---|---|
| 401 | no key, wrong key, forgotten device, or wrong pairing code |
| 403 | pairing isn't open, or the command only works on the MiSTer itself |
| 413 | body too big (1 MB for `/api`, 16 MB for art) |
| 429 | 16 devices already paired |
| 503 | phosphord didn't answer |

## Endpoints

### `GET /info` (no key)
`{"ok":true,"name":"Phosphor","version":"0.8.0","api":1}`: use it to find the MiSTer and check the
version.

### `POST /api`
Body: one command, the same JSON as phosphord's local socket (full list in `system/README.md`).
Paired devices may use:

| Command | Use |
|---|---|
| `status` | version, indexing, game count, `generation` (changes when the library changes: refresh then) |
| `categories`, `systems` | the shelf |
| `games` `{"system":"PSX","filter":"missing_art","q":"final","offset":0,"limit":100}` | a system's games, or with `filter` (`missing_art`, `missing_details`, `not_found`, `linked`, `not_game`, `unidentified`) / `q` (title search) across all systems; each with `system`, `art`, `details`, `scrape` |
| `game` `{"id":20}` | details, files, `based_on`, `scrape` state (`found`, `not_found`, `linked`, ...) |
| `edit` `{"id":20,"set":{"title":"...","description":"...","year":"1996","developer":"...","publisher":"...","genre":"...","players":"1-2","region":"USA"}}` | change details; `""` or `null` clears a field; the title can't be empty. Replies with the game. The scraper never overwrites a field that has a value. |
| `link` `{"id":21,"base":20}` (`"port":true`: the base may be on another system), `unlink`, `candidates` `{"id":21,"q":"tactics"}` | "Based on": a hack uses its base game's art and details |
| `scrape`, `rescrape` `{"id":20}`, `scrape_sources` | box art and details lookup |
| `scrape_todo` `{"start":true,"limit":400}` / `{"miss":true,"limit":1000,"offset":0}`, `scrape_result`, `scrape_pc_done` | box art looked up on the PC or phone: the games waiting (or, with `miss`, the ones nothing was found for, for matching to their base games; the reply then has `"miss":true`), the sources' order and logins; the result for one game; done |
| `not_game` `{"id":20,"on":true}` | a boot file, test ROM or tool: out of "not found", no box art looked up (`games` filter `not_game` lists them); `"on":false` puts it back |
| `scrape_undo` `{"id":20,"fields":["description","year"],"art":true}` | undo an automatic match: only the listed details and (with `art`) the box art go, and the game is "not found" again |
| `launch` `{"id":20}`, `favorite` `{"id":20,"on":true}`, `last`, `rescan` | remote control |
| `sysbk` | MiSTer & Linux backups: the list with SHA-256 hashes; `get` a piece (base64), `put` pieces back (see `system/phosphord/sysbk.c`) |
| `look` | the TV's accent colour (amber, red, green, blue, purple) and texture number, for the apps to match |
| `vset` | MiSTer's video settings and profiles, tried with Keep (35 s, then put back by the MiSTer itself): see `system/phosphord/vtry.h` |
| `backup` | snapshots on the MiSTer's drives: list, back up now, preview and restore (all or only saves) |
| `bluetooth` | the paired controllers, read only: pairing, disconnecting and forgetting only from the MiSTer itself |
| `memcard`, `pads`, `pad_analog`, `ports` | front ports (phosphor-main + the Phosphor menu core): memory card backups and putting one back (the card is backed up first), controllers live. `memcard {"do":"prepare"}` loads the Phosphor menu core, from the menu only |
| `cloud` | cloud saves: status, storage places, send now, preview and get saves back, add a storage place (passwords and sign-in tokens go only to the MiSTer) |
| `diag` | read the latest checks (`run` starts them; support bundles only from the MiSTer) |

Not allowed remotely: `wifi`, `remote`, `import`, `set_art` with a file path (use `PUT /art`).

### `GET /art/<game id>`
The game's box art as JPEG, or the base game's art when the game is linked. `404` when there's none.

### `PUT /art/<game id>`
Body: an image (PNG, JPEG, GIF, BMP...; up to 16 MB). Phosphor scales it to 512 px on the long side,
stores it as JPEG and replies with the game (`game.scrape`, details). A linked game gets its own art,
which is then used instead of its base game's art.

## Example (curl)

```sh
M=http://192.168.1.23:8767
curl -s $M/info
T=$(curl -s -XPOST $M/pair -d '{"code":"424690","name":"My PC"}' | jq -r .token)
curl -s -H "Authorization: Bearer $T" -XPOST $M/api -d '{"cmd":"games","system":"PSX","limit":5}'
curl -s -H "Authorization: Bearer $T" -XPOST $M/api -d '{"cmd":"edit","id":20,"set":{"year":"1996"}}'
curl -s -H "Authorization: Bearer $T" -T cover.png $M/art/20
curl -s -H "Authorization: Bearer $T" -o cover.jpg $M/art/20
```

## Notes for app builders

- Find MiSTers by trying `GET /info` on the local subnet (port 8767), or let the user type the address
  shown on the TV.
- Poll `status` every few seconds while the app is open; reload lists when `generation` changes.
- Games that are offline (drive unplugged) aren't listed by `games`.
- The connection is not encrypted (home network only, like MiSTer's own FTP/SMB). Don't use it over
  the internet.
