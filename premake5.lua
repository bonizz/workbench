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
        "src/engine/debug/**.cpp",
        "src/engine/debug/**.mm",
        "src/engine/debug/**.h",
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

project "tests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "build/tests/%{cfg.buildcfg:lower()}"
    objdir "build/obj/tests"

    files {
        "src/tests/main.cpp",
        "src/engine/core/math.cpp",
        "src/engine/core/math.h",
        "src/engine/core/object_id.h",
        "src/engine/core/time.cpp",
        "src/engine/core/time.h",
        "src/engine/debug/debug_state.cpp",
        "src/engine/debug/debug_state.h",
        "src/engine/renderer/render_context.cpp",
        "src/engine/renderer/render_context.h",
        "src/engine/renderer/render_command.h",
        "src/engine/scene/camera.cpp",
        "src/engine/scene/camera.h",
        "src/engine/scene/game_object.cpp",
        "src/engine/scene/game_object.h",
        "src/engine/scene/scene.cpp",
        "src/engine/scene/scene.h",
        "src/engine/scene/transform.cpp",
        "src/engine/scene/transform.h"
    }

    system "macosx"
    architecture "arm64"

    includedirs { "src", "src/engine", "external" }

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter {}

