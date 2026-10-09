plugins {
    id("com.android.application")
}

android {
    namespace = "com.lineagehide.module"
    compileSdk = 36
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.lineagehide.module.stub"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0.0"

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-Wall", "-Wextra", "-fvisibility=hidden")
                arguments += listOf("-DANDROID_STL=c++_static")
                abiFilters += listOf("arm64-v8a")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}
