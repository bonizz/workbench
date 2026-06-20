workspace "workbench"
    configurations { "Debug", "Release" }
    architecture "arm64"

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "build/%{cfg.buildcfg:lower()}"
    objdir "build/obj/%{cfg.buildcfg:lower()}"

    files {
        "src/sandbox/main.mm",
        "src/engine/core/**.cpp",
        "src/engine/core/**.h",
        "src/engine/platform/**.mm",
        "src/engine/platform/**.h",
        "src/engine/renderer/**.cpp",
        "src/engine/renderer/**.mm",
        "src/engine/renderer/**.h",
        "src/engine/scene/**.cpp",
        "src/engine/scene/**.h",
        "src/engine/editor/**.cpp",
        "src/engine/editor/**.mm",
        "src/engine/editor/**.h",
        "external/imgui/*.cpp",
        "external/imgui/*.h",
        "external/imgui/*.mm"
    }

    system "macosx"

    includedirs { "src", "src/engine", "external" }

    links {
        "Cocoa.framework",
        "Carbon.framework",
        "GameController.framework",
        "Metal.framework",
        "QuartzCore.framework"
    }

    filter { "files:external/imgui/imgui_impl_metal.mm or external/imgui/imgui_impl_osx.mm" }
        buildoptions { "-Wno-arc-bridge-casts-disallowed-in-nonarc" }

    filter {}

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"
