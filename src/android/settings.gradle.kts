// Mutable Dreams for Android, by Kid Bunka (GPL-3.0-only): the Trimui Brick port's core and SDL front end in an app.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "MutableDreams"
include(":app")
