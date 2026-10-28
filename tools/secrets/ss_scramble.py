#!/usr/bin/env python3
# Phosphor's ScreenScraper developer login, scrambled for a build: reads PHOSPHOR_SS_DEVID and
# PHOSPHOR_SS_DEVPASSWORD from the environment (a build secret: GitHub Actions secrets, never in the
# repo) and writes them XORed with a fresh random key, so the login isn't plain text in the program.
#   ss_scramble.py c  OUT.h     a C header (phosphord: src_accounts.c)
#   ss_scramble.py go OUT.go    a Go file (Phosphor for Windows: scrape_ss.go)
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
import os, secrets, sys

lang, out = sys.argv[1], sys.argv[2]
ident = os.environ.get("PHOSPHOR_SS_DEVID", "").strip().encode()
pw = os.environ.get("PHOSPHOR_SS_DEVPASSWORD", "").strip().encode()
if not ident or not pw:
    sys.exit("ss_scramble: PHOSPHOR_SS_DEVID and PHOSPHOR_SS_DEVPASSWORD are both needed")
key = secrets.token_bytes(32)
x = lambda b: bytes(c ^ key[i % len(key)] for i, c in enumerate(b))
nums = lambda b: ", ".join(str(c) for c in b)
if lang == "c":
    src = ("/* generated for one build by tools/secrets/ss_scramble.py: never commit */\n"
           "static const unsigned char SS_K[] = { %s };\n"
           "static const unsigned char SS_ID_X[] = { %s };\n"
           "static const unsigned char SS_PW_X[] = { %s };\n") % (nums(key), nums(x(ident)), nums(x(pw)))
elif lang == "go":
    src = ("// generated for one build by tools/secrets/ss_scramble.py: never commit\n\npackage main\n\n"
           "func init() {\n\tk := []byte{%s}\n\tun := func(b []byte) string {\n\t\to := make([]byte, len(b))\n"
           "\t\tfor i, c := range b {\n\t\t\to[i] = c ^ k[i%%len(k)]\n\t\t}\n\t\treturn string(o)\n\t}\n"
           "\tssBuiltinID = un([]byte{%s})\n\tssBuiltinPw = un([]byte{%s})\n}\n") % (nums(key), nums(x(ident)), nums(x(pw)))
else:
    sys.exit("ss_scramble: c or go")
with open(out, "w") as f:
    f.write(src)
