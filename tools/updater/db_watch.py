#!/usr/bin/env python3
# Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).
"""Database watch: the first stage of Phosphor's own updater (docs/updater-inventory.md).

Reads the database list from Update All's own source (theypsilon/Update_All_MiSTer, databases.py, read as
text, never run), downloads each database, and checks it against Phosphor's verdicts (system/ua/status.json)
and the paths Phosphor owns (tools/updater/rules.json). It reports:
  - databases Phosphor hasn't reviewed yet (new in Update All),
  - files that land on Phosphor's own paths (its Main, its menu core, MiSTer Linux, its folders), other than
    the ones expected (the main distribution's MiSTer and menu.rbf, which Phosphor swaps for its own),
  - what changed since the last run (new files on Phosphor's paths, a new MiSTer Linux, databases gone),
  - databases that couldn't be read.
It changes nothing and publishes nothing. Standard library only.

  db_watch.py [--all] [--previous state.json] [--state out.json] [--report out.md]
  --all: the game artwork and manuals databases too (244 more downloads; weekly is enough)
Exit code 0 always (the report says what needs a look); 2 when Update All's list itself can't be read.
"""
import argparse, hashlib, io, json, os, re, sys, time, urllib.request, zipfile
from fnmatch import fnmatch

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
UA_RAW = "https://raw.githubusercontent.com/theypsilon/Update_All_MiSTer/master/src/update_all/"
UA = "Phosphor-db-watch/1 (+https://github.com/f3bandit/phosphor_os)"

def fetch(url, tries=3):
    last = None
    for i in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=120) as r:
                return r.read()
        except Exception as e:                         # (one retry after a pause: GitHub's raw host limits bursts)
            last = e
            time.sleep(2 + 3 * i)
    raise last

def load_json(data, url):
    if url.split("?")[0].endswith(".zip") or data[:2] == b"PK":
        z = zipfile.ZipFile(io.BytesIO(data))
        names = [n for n in z.namelist() if n.endswith(".json")] or z.namelist()
        data = z.read(names[0])
    return json.loads(data)

# --- Update All's database list (read as text) ---------------------------------------------------------------

def ua_databases(src):
    consts = dict(re.findall(r"^(DB_ID_\w+)\s*(?::\s*Final\[str\])?\s*=\s*'([^']+)'", src, re.M))
    out = []
    for m in re.finditer(r"self\.(\w+)\s*=\s*Database\(db_id=([^,]+),\s*db_url=([^,]+),\s*title='([^']*)'", src):
        db_id = m.group(2).strip()
        db_id = consts.get(db_id, db_id.strip("'\""))
        url = m.group(3).strip().strip("'\"")
        if not url.startswith("http"):
            continue
        out.append({"var": m.group(1), "id": db_id, "title": m.group(4), "url": url})
    return out

def ua_version(src):
    m = re.search(r"UPDATE_ALL_VERSION[^=]*=\s*['\"]([^'\"]+)['\"]", src)
    return m.group(1) if m else "?"

def heavy(db):                                         # the 244 artwork and manuals databases
    return "manualsdb" in db["id"] or "artworkdb" in db["id"]

# --- Phosphor's verdicts ------------------------------------------------------------------------------------

def verdicts(status, var_ids):
    """db id -> (status, note), from status.json's items (db:<id>, or var:<Update All setting>)"""
    out = {}
    for k, v in (status.get("items") or {}).items():
        if not isinstance(v, dict) or "status" not in v:
            continue
        kind, _, name = k.partition(":")
        if kind == "db":
            out[name] = (v["status"], v.get("note", ""))
        elif kind == "var":
            for i in var_ids.get(name, [name] if "/" in name else []):
                out.setdefault(i, (v["status"], v.get("note", "")))
    return out

# --- One database's files -----------------------------------------------------------------------------------

def norm(p):
    return p.lstrip("|").lstrip("/")

def db_paths(d, get):
    """every path the database installs (files, and what its archives unpack, where that's not just games/)"""
    paths = set(norm(p) for p in (d.get("files") or {}))
    for kind in ("zips", "archives"):
        for z in (d.get(kind) or {}).values():
            if not isinstance(z, dict):
                continue
            target = norm(z.get("target_folder") or z.get("target_folder_path") or "")
            summary = z.get("internal_summary") or z.get("summary_inline")
            if summary is None and z.get("summary_file", {}).get("url") and not re.match(r"(games|docs|Cheats|Filters|Gamma|Shadow_Masks|Presets|Fonts?|wallpapers|_[A-Z][^/]*)/", target):
                try:
                    summary = load_json(get(z["summary_file"]["url"]), z["summary_file"]["url"])
                except Exception:
                    summary = None
            if summary:
                paths.update(norm(p) for p in (summary.get("files") or {}))
            elif target:
                paths.add(target.rstrip("/") + "/*")
    return paths

def match(p, pat):
    return re.search(pat[3:], p) is not None if pat.startswith("re:") else fnmatch(p, pat)

def on_phosphors_paths(paths, rules, db):
    # (by title first: the DB9 / SNAC8 and AitorGomez distributions share the main distribution's id)
    exp = rules.get("expected", {})
    expected = exp.get(db["title"], exp.get(db["id"], [])) if db["title"] in exp or db["id"] in exp else []
    hits = []
    for p in sorted(paths):
        for pat, why in rules["protected"].items():
            if match(p, pat) and not any(match(p, e) for e in expected):
                hits.append((p, why))
                break
    return hits

# --- The run ------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--previous")
    ap.add_argument("--state", default="db_watch_state.json")
    ap.add_argument("--report", default="db_watch_report.md")
    ap.add_argument("--catalog", default="db_watch_catalog.json")
    ap.add_argument("--status", default=os.path.join(ROOT, "system", "ua", "status.json"))
    ap.add_argument("--rules", default=os.path.join(HERE, "rules.json"))
    a = ap.parse_args()

    try:
        src = fetch(UA_RAW + "databases.py").decode()
        ua_ver = ua_version(fetch(UA_RAW + "constants.py").decode())
    except Exception as e:
        print(f"Update All's database list couldn't be read: {e}", file=sys.stderr)
        return 2
    rules = json.load(open(a.rules))
    status = json.load(open(a.status))
    dbs = ua_databases(src)
    known = verdicts(status, rules.get("var_ids", {}))
    prev = json.load(open(a.previous)) if a.previous and os.path.exists(a.previous) else {}
    prev_dbs = prev.get("dbs", {})

    cache = {}
    def get(url):
        if url not in cache:
            cache[url] = fetch(url)
        return cache[url]

    def classify(db):
        c = rules.get("catalog", {})
        e = c.get(db["title"]) or c.get(db["id"])
        if e is None:
            e = next((v for pre, v in rules.get("catalog_prefix", {}).items() if db["id"].startswith(pre)), None)
        return dict(e) if e else {"category": "review", "tag": "review", "why": "Not in Phosphor's catalog yet (tools/updater/rules.json)"}

    def covered(tag, entry, hit):
        """a file on Phosphor's paths that the tag already deals with"""
        if tag in ("block", "not needed", "in phosphor", "warn"):
            return True                                  # (not installed, or installed after a warning that says so)
        if tag in ("patch", "launcher"):
            return not entry.get("ready") or any(match(hit, pt) for pt in entry.get("parts", []))
        return False                                     # ok, opt-in, review: installed as they are

    state = {"update_all": ua_ver, "checked": time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime()), "dbs": {}}
    look, changes, failed, unreviewed = [], [], [], []
    seen = set()
    for db in dbs:
        if heavy(db) and not a.all:
            if db["id"] in prev_dbs:                     # (kept from the last full run, classified again)
                state["dbs"][db["id"]] = dict(prev_dbs[db["id"]], **classify(db))
            continue
        key = db["id"] + "|" + db["url"]
        if key in seen:
            continue
        seen.add(key)
        sid = db["id"] if db["id"] not in state["dbs"] else db["id"] + " (" + db["title"] + ")"
        try:
            raw = get(db["url"])
            d = load_json(raw, db["url"])
            paths = db_paths(d, get)
        except Exception as e:
            failed.append((db, str(e)[:160]))
            if db["id"] in prev_dbs:
                state["dbs"][sid] = prev_dbs[db["id"]]
            continue
        hits = on_phosphors_paths(paths, rules, db)
        linux = (d.get("linux") or {}).get("version")
        entry = {"title": db["title"], "url": db["url"], "sha256": hashlib.sha256(raw).hexdigest(),
                 "files": len(paths), "timestamp": d.get("timestamp"), "on_phosphor": [h[0] for h in hits][:400]}
        if linux:
            entry["linux"] = linux
        entry.update(classify(db))
        entry["status_json"] = known.get(db["id"], ("", ""))[0]     # (Update All's own screen in Phosphor, for now)
        state["dbs"][sid] = entry
        if entry["tag"] == "review" and entry["category"] == "review":
            unreviewed.append(db)
        loose = [h for h in hits if not covered(entry["tag"], entry, h[0])]
        if loose:
            look.append((db, entry["tag"], loose))
        p = prev_dbs.get(sid)
        if p:
            new_hits = sorted(set(entry["on_phosphor"]) - set(p.get("on_phosphor", [])))
            if new_hits:
                changes.append((db, f"now also installs on Phosphor's paths: {', '.join(new_hits[:8])}" + (" …" if len(new_hits) > 8 else "")))
            if linux and p.get("linux") and linux != p["linux"]:
                changes.append((db, f"MiSTer Linux {p['linux']} → **{linux}** (the Linux update warning applies)"))
            if p.get("sha256") != entry["sha256"]:
                changes.append((db, f"updated ({p.get('files', '?')} → {entry['files']} files)"))
        elif prev.get("ua_ids") and db["id"] not in prev["ua_ids"]:
            changes.append((db, "new database"))     # (new in Update All's list, not just checked for the first time)
    state["ua_ids"] = sorted({db["id"] for db in dbs})
    for i in sorted(set(prev.get("ua_ids", [])) - set(state["ua_ids"])):
        changes.append(({"id": i, "title": prev_dbs.get(i, {}).get("title", i), "url": ""}, "gone from Update All's list"))

    attention = bool(look or unreviewed or failed or [c for c in changes if not c[1].startswith("updated")])
    state["needs_a_look"] = sorted({d["id"] for d, *_ in look} | {d["id"] for d in unreviewed} |
                                   {d["id"] for d, _ in changes if not _.startswith("updated")})
    json.dump(state, open(a.state, "w"), indent=1, sort_keys=True)
    keys = ("title", "url", "category", "tag", "why", "ready", "parts", "files", "sha256", "timestamp", "linux")
    catalog = {"update_all": ua_ver, "checked": state["checked"],
               "databases": [dict({"id": sid.split(" (")[0]}, **{k: v[k] for k in keys if k in v}) for sid, v in sorted(state["dbs"].items())]}
    json.dump(catalog, open(a.catalog, "w"), indent=1)

    L = []
    w = L.append
    w(f"Update All **{ua_ver}**{' (was ' + prev.get('update_all') + ')' if prev.get('update_all') not in (None, ua_ver) else ''}: "
      f"{len(state['dbs'])} databases checked on {state['checked']}" + ("" if a.all else " (artwork and manuals: weekly)") + ".")
    w("")
    cats = ["cores", "special cores", "hybrid cores", "wallpapers", "bios", "game art", "tools", "not needed", "review"]
    tags = ["ok", "patch", "launcher", "warn", "opt-in", "block", "in phosphor", "not needed", "review"]
    grid = {}
    for v in state["dbs"].values():
        grid[(v.get("category", "review"), v.get("tag", "review"))] = grid.get((v.get("category", "review"), v.get("tag", "review")), 0) + 1
    used = [t for t in tags if any(grid.get((c, t)) for c in cats)]
    w("| | " + " | ".join(used) + " |")
    w("|---|" + "---|" * len(used))
    for c in cats:
        if any(grid.get((c, t)) for t in used):
            w(f"| **{c}** | " + " | ".join(str(grid.get((c, t), "")) for t in used) + " |")
    pend = sorted(v["title"] for v in state["dbs"].values() if v.get("tag") in ("patch", "launcher") and not v.get("ready"))
    if pend:
        w("")
        w("Waiting for Phosphor's parts (not offered until they're ready): " + ", ".join(pend) + ".")
    w("")
    if not attention:
        w("Nothing needs a look: no new databases, nothing new on Phosphor's paths.")
    if look:
        w("### On Phosphor's paths")
        w("Files on Phosphor's own paths that the database's tag doesn't deal with:")
        w("")
        w("| Database | Tag | Files | Why it matters |")
        w("|---|---|---|---|")
        for db, verdict, hits in look:
            files = ", ".join(f"`{p}`" for p, _ in hits[:5]) + (f" and {len(hits) - 5} more" if len(hits) > 5 else "")
            whys = "; ".join(sorted({why for _, why in hits}))
            w(f"| {db['title']} (`{db['id']}`) | {verdict} | {files} | {whys} |")
        w("")
    if unreviewed:
        w("### Not reviewed yet")
        w("In Update All's list but not in Phosphor's catalog (`tools/updater/rules.json`): category and tag needed.")
        w("")
        for db in unreviewed:
            w(f"- {db['title']} (`{db['id']}`): {db['url']}")
        w("")
    big = [c for c in changes if not c[1].startswith("updated")]
    if big:
        w("### Changed since the last run")
        for db, what in big:
            w(f"- {db['title']} (`{db['id']}`): {what}")
        w("")
    if failed:
        w("### Couldn't be read")
        for db, err in failed:
            w(f"- {db['title']} (`{db['id']}`): {err}")
        w("")
    upd = [c for c in changes if c[1].startswith("updated")]
    if upd:
        w(f"<details><summary>{len(upd)} databases updated (nothing on Phosphor's paths)</summary>")
        w("")
        for db, what in upd:
            w(f"- {db['title']}: {what}")
        w("")
        w("</details>")
    open(a.report, "w").write("\n".join(L) + "\n")
    print("\n".join(L))
    print(f"\n{'NEEDS A LOOK' if attention else 'ok'}: {len(look)} on Phosphor's paths, {len(unreviewed)} not reviewed, "
          f"{len(big)} changes, {len(failed)} unreadable", file=sys.stderr)
    if os.environ.get("GITHUB_OUTPUT"):
        open(os.environ["GITHUB_OUTPUT"], "a").write(f"attention={'yes' if attention else 'no'}\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
