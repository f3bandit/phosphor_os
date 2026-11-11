// Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).
pluginManagement {
    repositories {
        google()
        // Maven Central through Google's mirror first (Central rate-limits busy build machines)
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        // Maven Central through Google's mirror first (Central rate-limits busy build machines)
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
    }
}
rootProject.name = "Phosphor"
include(":app")
