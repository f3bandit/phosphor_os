#!/usr/bin/env python3
# Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).
"""Builds what Phosphor's updater on the SS1 installs from (stage 2 of docs/updater-inventory.md).

From db_watch.py's catalog and Phosphor's latest release, into one folder (published as the updater-catalog
branch, raw.githubusercontent.com/f3bandit/phosphor_os/updater-catalog/...):

  catalog.json        every database: category, tag, why, and install_url, the database to give MiSTer's
                      downloader: the original one, or for a patch, Phosphor's copy of it
  dbs/<name>.json.zip a patch database: the original with Phosphor's parts in place of its files (same path,
                      Phosphor's md5, size and URL), so the downloader installs them and keeps them; every
                      other file is the original's, from the original's URL. A part is never left out: the
                      downloader would delete the file as obsolete (menu.rbf: the bootloader's core)
  parts/<file>        Phosphor's files the patch databases point at, from Phosphor's release

  build_catalog.py --catalog db_watch_catalog.json --release phosphor-x-armhf.tar.gz --out site
                   [--base https://raw.githubusercontent.com/f3bandit/phosphor_os/updater-catalog/]
Standard library only. Exit 1 when a ready patch can't be built (its parts missing from the release, or inside
an archive that's unpacked whole): the catalog then offers that database as not ready.
"""
import argparse, copy, hashlib, io, json, os, re, sys, tarfile, time, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from db_watch import fetch, load_json, norm   # noqa: E402  (same download and reading as the watch)

def slug(s):
    return re.sub(r"[^A-Za-z0-9._-]+", "_", s).strip("_")

def release_files(path):
    """the release's files by their path inside it (without the top folder phosphor-<ver>/)"""
    out, ver = {}, "?"
    with tarfile.open(path) as t:
        for m in t.getmembers():
            if not m.isfile():
                continue
            name = m.name.split("/", 1)[1] if "/" in m.name else m.name
            if name in ("VERSION",) or name.startswith(("bin/", "cores/")):
                out[name] = t.extractfile(m).read()
        if "VERSION" in out:
            ver = out["VERSION"].decode(errors="replace").split()[0]
    return out, ver

def patch_db(d, parts, part_url, rel):
    """the original database with Phosphor's parts in place; problems as a list (empty: done)"""
    d = copy.deepcopy(d)
    files = d.setdefault("files", {})
    problems = []
    for path, src in parts.items():
        if src == "original":
            continue
        rpath = src.split(":", 1)[1]
        data = rel.get(rpath)
        if data is None:
            problems.append(f"{rpath} isn't in Phosphor's release")
            continue
        new = {"hash": hashlib.md5(data).hexdigest(), "size": len(data), "url": part_url(path, data)}
        keys = [k for k in list(files) if norm(k) == path]
        if keys:                                        # (keeps the original's tags and options)
            for k in keys:
                files[k] = dict(files[k], **new)
            continue
        found = False
        for kind in ("archives", "zips"):
            for zid, z in (d.get(kind) or {}).items():
                summ = z.get("summary_inline") or z.get("internal_summary")
                if not summ:
                    continue
                hit = [k for k in list(summ.get("files", {})) if norm(k) == path]
                if not hit:
                    continue
                if z.get("extract", "selective") == "all" or z.get("kind") == "extract_all_contents":
                    problems.append(f"{path} is in {zid}, which is unpacked whole")
                    found = True
                    continue
                for k in hit:
                    old = summ["files"].pop(k)
                    keep = {a: b for a, b in old.items() if a not in ("arc_at", "arc_id", "zip_id", "zip_path")}
                    files[k] = dict(keep, **new)
                found = True
        if not found:
            problems.append(f"{path} isn't in the database")
    return d, problems

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", required=True)
    ap.add_argument("--release", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--base", default="https://raw.githubusercontent.com/f3bandit/phosphor_os/updater-catalog/")
    a = ap.parse_args()
    cat = json.load(open(a.catalog))
    rel, ver = release_files(a.release)
    os.makedirs(os.path.join(a.out, "dbs"), exist_ok=True)
    os.makedirs(os.path.join(a.out, "parts"), exist_ok=True)

    def part_url(path, data):
        name = os.path.basename(path)
        open(os.path.join(a.out, "parts", name), "wb").write(data)
        return a.base + "parts/" + name

    failed = []
    out = {"phosphor": ver, "update_all": cat.get("update_all"), "checked": cat.get("checked"),
           "built": time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime()), "databases": []}
    for e in cat["databases"]:
        e = dict(e)
        e["install_url"] = e["url"]
        if e.get("tag") == "patch" and e.get("ready") and any(v != "original" for v in (e.get("parts") or {}).values()):
            try:
                d = load_json(fetch(e["url"]), e["url"])
                pd, problems = patch_db(d, e["parts"], part_url, rel)
            except Exception as ex:
                pd, problems = None, [f"couldn't read the original: {ex}"]
            if problems:
                failed.append((e["title"], problems))
                e["ready"] = False
                e["why"] += " (not ready: " + "; ".join(problems) + ")"
                e["install_url"] = None
            else:
                name = slug(e["id"] + "-" + e["title"]) + ".json.zip"
                pd["db_url"] = a.base + "dbs/" + name
                buf = io.BytesIO()
                with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
                    z.writestr("db.json", json.dumps(pd, separators=(",", ":")))
                open(os.path.join(a.out, "dbs", name), "wb").write(buf.getvalue())
                e["install_url"] = pd["db_url"]
        elif e.get("tag") in ("patch", "launcher") and not e.get("ready"):
            e["install_url"] = None                     # (not offered until Phosphor's parts are ready)
        out["databases"].append(e)
    json.dump(out, open(os.path.join(a.out, "catalog.json"), "w"), indent=1)
    n = sum(1 for e in out["databases"] if e["install_url"] and e["install_url"].startswith(a.base))
    print(f"catalog for Phosphor {ver}: {len(out['databases'])} databases, {n} patched by Phosphor, "
          f"{len(os.listdir(os.path.join(a.out, 'parts')))} parts")
    for t, p in failed:
        print(f"NOT READY: {t}: {'; '.join(p)}", file=sys.stderr)
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
