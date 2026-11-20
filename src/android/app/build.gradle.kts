plugins {
    id("com.android.application")
}

android {
    namespace = "com.kidbunka.mutabledreams"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        // A new app of its own (not TrackWerk's com.kidbunka.trackwerk): Mutable Dreams starts at its own 1.
        applicationId = "com.kidbunka.mutabledreams"
        minSdk = 28                 // Android 9: posix_spawnp (the archive starts curl); the RG405V has Android 12
        targetSdk = 34
        versionCode = 15900         // 1.5.9; always rising, so a newer APK installs over an older one
        versionName = "1.5.9"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")   // the RG405V (and phones); x86_64 for the emulator
        }
        externalNativeBuild {
            cmake {
                // Android 15+ with 16 KB pages refuses libraries aligned to 4 KB (as TrackWerk notes)
                arguments += listOf("-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // SDL2's Java half, from the same vendored SDL 2.30.9 as its C half (SDLActivity checks they match)
    sourceSets {
        getByName("main") {
            // (-PmdPort=/path/to/port when building from a copy outside iCloud, as tools/build_android.py does)
            java.srcDir(providers.gradleProperty("mdPort").map { "$it/third_party/SDL2/android/java" }
                .orElse("../../third_party/SDL2/android/java").get())
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // unsigned: Kid Bunka signs it himself with his release key (never part of this build)
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}
