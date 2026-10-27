// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
import java.security.SecureRandom

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.f3bandit.phosphor"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.f3bandit.phosphor"
        minSdk = 26
        targetSdk = 36
        versionCode = 18
        versionName = "0.9.8"
        // Phosphor's own ScreenScraper developer login: a build secret (PHOSPHOR_SS_DEVID / PHOSPHOR_SS_DEVPASSWORD
        // in the environment), never in the repo; empty without it. Scrambled (XOR with a fresh random key each
        // build, stored as hex) so it isn't plain text in the APK; SsLogin.kt unscrambles it.
        val ssKey = ByteArray(32).also { SecureRandom().nextBytes(it) }
        fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }
        fun scr(v: String?): String {
            val b = (v ?: "").toByteArray(Charsets.UTF_8)
            return "\"" + hex(ByteArray(b.size) { (b[it].toInt() xor ssKey[it % ssKey.size].toInt()).toByte() }) + "\""
        }
        buildConfigField("String", "SS_K", "\"" + hex(ssKey) + "\"")
        buildConfigField("String", "SS_DEVID", scr(System.getenv("PHOSPHOR_SS_DEVID")))
        buildConfigField("String", "SS_DEVPASSWORD", scr(System.getenv("PHOSPHOR_SS_DEVPASSWORD")))
    }
    buildFeatures { buildConfig = true }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/webshared"))
    testOptions {
        unitTests.isIncludeAndroidResources = true
        unitTests.all {
            // Robolectric fetches its Android images at test time: through the same mirror as the build
            it.systemProperty("robolectric.dependency.repo.url", "https://maven-central.storage-download.googleapis.com/maven2")
            listOf("PHOSPHOR_TEST_ADDR", "PHOSPHOR_TEST_CODE", "PHOSPHOR_TEST_PNG").forEach { k -> System.getenv(k)?.let { v -> it.environment(k, v) } }
            it.testLogging { showStandardStreams = true }
        }
    }
}

kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }

dependencies {
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.activity:activity-ktx:1.11.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.9.4")
    implementation("androidx.webkit:webkit:1.14.0")
    // only the state holders (mutableStateOf, snapshotFlow) the background work shares with the page: no Compose UI
    implementation("androidx.compose.runtime:runtime:1.9.5")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.robolectric:robolectric:4.16")
}

/* the page's shared parts come from Phosphor for Windows (apps/windows/web), so both apps draw the
   console's textures the same way: the fonts, and look.js = the scripts Windows' index.html loads before
   texture.js, then texture.js (circuit.js and the tex_*.js sprite textures Texture Maker adds there),
   and video-options.js (MiSTer's video settings, explained) */
val webShared by tasks.registering {
    val win = rootProject.file("../windows/web")
    val out = layout.buildDirectory.dir("generated/webshared/web")
    inputs.dir(win)
    inputs.file(rootProject.file("../windows/fanart/fanart.json"))
    val notices = rootProject.file("../../THIRD_PARTY_NOTICES.md")
    inputs.file(notices)
    outputs.dir(out)
    doLast {
        val dir = out.get().asFile
        dir.deleteRecursively()
        File(dir, "fonts").mkdirs()
        File(win, "fonts").listFiles()!!.filter { it.name.endsWith(".woff2") }.forEach { it.copyTo(File(dir, "fonts/" + it.name), true) }
        val html = File(win, "index.html").readText()
        val names = Regex("""<script src="([^"]+)"></script>""").findAll(html).map { it.groupValues[1] }.toList()
        val upTo = names.indexOf("texture.js")
        require(upTo >= 0) { "texture.js isn't in apps/windows/web/index.html" }
        val js = names.take(upTo + 1).joinToString("\n") { n -> "/* ---- $n */\n" + File(win, n).readText() }
        File(dir, "look.js").writeText("/* made by the build from apps/windows/web: don't edit */\n$js")
        File(win, "video-options.js").copyTo(File(dir, "video-options.js"), true)
        // the fan art list (FanArt.kt reads it when GitHub can't be reached)
        File(win, "../fanart/fanart.json").copyTo(File(dir, "fanart.json"), true)
        // the open-source licences, for About › Open-source licences
        notices.copyTo(File(dir, "THIRD_PARTY_NOTICES.md"), true)
    }
}
tasks.named("preBuild") { dependsOn(webShared) }
