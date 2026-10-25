// Phosphor for Android: ScreenScraper's login, the way Console Mode does it (#137; the same rules as
// Phosphor for Windows' scrape_ss.go). Every request needs a developer login (devid / devpassword:
// Phosphor's own, built in, or the person's own) and the person's screenscraper.fr login. The
// person's login is never sent as the developer login: ScreenScraper refuses that.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor.scrape

import com.f3bandit.phosphor.BuildConfig
import org.json.JSONObject
import java.net.URLEncoder

/** Phosphor's own developer login: a build secret (PHOSPHOR_SS_DEVID / PHOSPHOR_SS_DEVPASSWORD when the
 *  app is built), never in the repo. Empty until ScreenScraper gives one. */
var ssBuiltinId: String = BuildConfig.SS_DEVID
var ssBuiltinPw: String = BuildConfig.SS_DEVPASSWORD

fun ssDevLogin(acct: (String, String) -> String): Pair<String, String> =
    if (ssBuiltinId.isNotEmpty() && ssBuiltinPw.isNotEmpty()) ssBuiltinId to ssBuiltinPw
    else acct("screenscraper", "devid") to acct("screenscraper", "devpassword")

/** what's missing for ScreenScraper, in words ("" when all four are there) */
fun ssMissing(acct: (String, String) -> String): String {
    val m = ArrayList<String>()
    if (acct("screenscraper", "user").isEmpty()) m.add("your user name")
    if (acct("screenscraper", "password").isEmpty()) m.add("your password")
    val (id, pw) = ssDevLogin(acct)
    if (id.isEmpty() || pw.isEmpty()) m.add("a developer ID and password (Phosphor's own is on its way from ScreenScraper; until then, your own if you have one)")
    return if (m.isEmpty()) "" else "ScreenScraper needs " + m.joinToString(", ")
}

/** ScreenScraper's reply in words, and whether it means stopping for the rest of the run; "" when fine */
fun ssAnswer(rc: Int, http: Int, body: ByteArray?): Pair<String, Boolean> {
    val b = body?.let { String(it).lowercase() } ?: ""
    return when {
        "erreur de login" in b && ("développeur" in b || "developpeur" in b) -> "ScreenScraper refused the developer ID" to true
        "erreur de login" in b -> "ScreenScraper refused your login: check the user name and password" to true
        http == 401 -> "ScreenScraper's API is closed to non-members right now: try later" to true
        http == 423 -> "ScreenScraper is down for maintenance: try later" to true
        http == 426 -> "ScreenScraper blocked this version of Phosphor: update it" to true
        http == 430 -> "Today's ScreenScraper lookups are used up: it goes on tomorrow" to true
        http == 431 -> "Too many not-found lookups at ScreenScraper today: it goes on tomorrow" to true
        http == 403 -> "ScreenScraper refused the request (HTTP 403)" to true
        rc < 0 -> "Can't reach screenscraper.fr" to false
        else -> "" to false
    }
}

/** Check login: asks ScreenScraper for the account's details; the message, and whether it works */
suspend fun ssCheck(acct: (String, String) -> String): Pair<String, Boolean> {
    ssMissing(acct).takeIf { it.isNotEmpty() }?.let { return it to false }
    val (id, pw) = ssDevLogin(acct)
    fun e(v: String) = URLEncoder.encode(v, "UTF-8")
    val u = "https://api.screenscraper.fr/api2/ssuserInfos.php?devid=${e(id)}&devpassword=${e(pw)}&softname=Phosphor&output=json" +
        "&ssid=${e(acct("screenscraper", "user"))}&sspassword=${e(acct("screenscraper", "password"))}"
    val r = netRequest("GET", u, max = 1L shl 20)
    val (msg, _) = ssAnswer(r.rc, r.http, r.body)
    if (msg.isNotEmpty()) return msg to false
    if (r.rc != 0 || r.body == null) return "ScreenScraper didn't answer (HTTP ${r.http})" to false
    val user = runCatching { JSONObject(String(r.body)).getJSONObject("response").getJSONObject("ssuser") }.getOrNull()
        ?: return "ScreenScraper's answer couldn't be read" to false
    return "Login works: ${user.optString("requeststoday")} of ${user.optString("maxrequestsperday")} lookups used today" to true
}
