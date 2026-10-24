// Phosphor for Android, box art on the phone: the name handling, the same as phosphord's (ident.c
// title_norm, scraper.c name_base, src_libretro.c serial_norm and friends) and Phosphor for Windows'
// scrape_text.go, so the phone matches exactly what the MiSTer would.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

private val titleNoise = setOf(
    "hack", "hacks", "hacked", "patch", "patched", "mod", "modded", "translation", "translated", "trans", "english", "eng", "en",
    "fix", "fixed", "fixes", "edition", "improvement", "improved", "restoration", "redux", "beta", "proto", "prototype", "demo", "by",
    "unl", "hb", "rev", "version", "romhack", "kaizo",
)

private fun isAlnum(c: Char) = c in '0'..'9' || c in 'a'..'z' || c in 'A'..'Z'
private fun isDigit(c: Char) = c in '0'..'9'

/** lower-case title words, nothing in brackets, no noise words: "Super Mario World (USA)" -> "super mario world" */
fun titleNorm(title: String): String {
    val b = StringBuilder()
    var depth = 0; var n = 0
    // byte by byte like the C and Go: non-ASCII letters become spaces
    val bytes = title.toByteArray(Charsets.UTF_8)
    var i = 0
    while (i < bytes.size && n < 511) {
        var c = (bytes[i].toInt() and 0xFF).toChar()
        i++
        when (c) {
            '(', '[', '{' -> { depth++; continue }
            ')', ']', '}' -> { if (depth > 0) depth--; continue }
        }
        if (depth > 0) continue
        if (isAlnum(c)) { if (c in 'A'..'Z') c += 'a' - 'A'; b.append(c) } else b.append(' ')
        n++
    }
    return b.toString().split(' ', '\t', '\n', '\r').filter { it.isNotEmpty() }
        .filter { w -> !(w in titleNoise || (w.length > 1 && w[0] == 'v' && isDigit(w[1]))) }
        .joinToString(" ")
}

/** "Final Fantasy III (USA)" -> "Final Fantasy III"; "Legend of Zelda, The - A Link" -> "The Legend of Zelda - A Link" */
fun nameBase(official: String): String {
    var s = official
    s.indexOf(" (").let { if (it >= 0) s = s.substring(0, it) }
    s.indexOf(" [").let { if (it >= 0) s = s.substring(0, it) }
    val i = s.indexOf(", The")
    if (i >= 0 && (i + 5 == s.length || s[i + 5] == ' ')) s = "The " + s.substring(0, i) + s.substring(i + 5)
    return s
}

/** "GM T-81036 -00" -> "T-81036" */
fun serialNorm(input: String): String {
    var p = input.trimStart(' ')
    if (p.length >= 3 && p.substring(0, 3).equals("GM ", ignoreCase = true)) p = p.substring(3)
    val b = StringBuilder()
    for (c0 in p) {
        if (c0 == ' ' || c0 == '\t' || c0 == '\n' || c0 == '\r' || c0 == '\u000B' || c0 == '\u000C') continue
        b.append(if (c0 in 'a'..'z') c0 - ('a' - 'A') else c0)
    }
    var o = b.toString()
    val l = o.length
    if (l > 4 && o[l - 3] == '-' && isDigit(o[l - 2]) && isDigit(o[l - 1])) o = o.substring(0, l - 3)
    return o
}

fun nameKey(name: String): String {
    var base = nameBase(name)
    if (base.length >= 4 && base.substring(0, 4).equals("The ", ignoreCase = true)) base = base.substring(4)
    return titleNorm(base)
}

/** the first brackets' words: "X (USA, Canada)" -> "USA, Canada" */
fun regionOf(name: String): String {
    val i = name.indexOf('(')
    if (i < 0) return ""
    val j = name.indexOf(')', i)
    if (j < 0) return ""
    return name.substring(i + 1, j)
}

fun discNo(name: String): Int {
    val i = name.lowercase().indexOf("(disc ")
    if (i < 0 || i + 6 >= name.length || !isDigit(name[i + 6])) return 0
    var j = i + 6
    while (j < name.length && isDigit(name[j])) j++
    return name.substring(i + 6, j).toIntOrNull() ?: 0
}

/** libretro thumbnail file names: these characters become _ */
fun thumbName(name: String): String = buildString { for (c in name) append(if ("&*/:`<>?\\|".indexOf(c) >= 0) '_' else c) }

/** like phosphord's: everything but letters, digits and -._~ as %XX (UTF-8 bytes) */
fun netEscape(s: String): String {
    val hex = "0123456789ABCDEF"
    val b = StringBuilder()
    for (by in s.toByteArray(Charsets.UTF_8)) {
        val c = by.toInt() and 0xFF
        val ch = c.toChar()
        if (c < 0x80 && (isAlnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~')) b.append(ch)
        else { b.append('%'); b.append(hex[c shr 4]); b.append(hex[c and 15]) }
    }
    return b.toString()
}

/** like sres_set: trimmed, no Windows line ends */
fun trimField(v: String) = v.trim().replace("\r", "")
