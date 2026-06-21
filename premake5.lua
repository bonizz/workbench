-- premake5 5.0.0-beta8's `ninja` action declares `depfile = $out.d` / `deps = gcc`
-- on the cc/cxx compile rules but never tells clang to write that depfile. clang
-- emits nothing, Ninja records `#deps 0`, and editing a header never triggers a
-- rebuild -- the stale-object bug that used to force clean rebuilds.
--
-- The flags can't go in cflags: premake emits cflags as a global Ninja binding,
-- where `$out` expands to empty (you'd get `-MF .d`, dumping every depfile into a
-- single junk `.d` file). They have to live in the rule command, where `$out`
-- resolves per edge -- exactly how premake's own pch rule already does it.
--
-- premake generates the rule text from an embedded module, so we override the
-- rule emitters and inject the flags during generation. This keeps the fix in
-- premake5.lua (no post-processing of generated .ninja files), so any plain
-- `premake5 ninja` produces correct dependency tracking. The coupling to
-- premake's private ninja internals is acceptable: the project is pinned to
-- premake5 5.0.0-beta8, and premake.override throws loudly if these entry points
-- ever disappear.
local p = premake
local ninja = p.modules.ninja
local function injectDeps(text)
    return (text:gsub("(%-c %$in %-o %$out)\n", "%1 -MMD -MP -MF $out.d\n"))
end
p.override(ninja.cpp, "cxxrule", function(base, cfg, toolset)
    p.out(injectDeps(p.capture(function() base(cfg, toolset) end)))
end)
p.override(ninja.cpp, "ccrule", function(base, cfg, toolset)
    p.out(injectDeps(p.capture(function() base(cfg, toolset) end)))
end)

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
        "src/engine/agent/**.cpp",
        "src/engine/agent/**.h",
        "src/engine/agent/**.mm",
        "src/engine/capture/**.cpp",
        "src/engine/capture/**.h",
        "src/engine/capture/**.mm",
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

    includedirs { "src", "src/engine", "src/ext", "external" }

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
        "src/tests/test_core.cpp",
        "src/tests/test_scene.cpp",
        "src/tests/test_agent.cpp",
        "src/tests/test_renderer.cpp",
        "src/tests/test_serialization.cpp",
        "src/tests/test_picking.cpp",
        "src/tests/test_debug.cpp",
        "src/tests/test_test_suite.cpp",
        "src/engine/agent/command.cpp",
        "src/engine/agent/command.h",
        "src/engine/agent/script_runner.cpp",
        "src/engine/agent/script_runner.h",
        "src/engine/agent/test_suite.cpp",
        "src/engine/agent/test_suite.h",
        "src/engine/capture/capture.h",
        "src/engine/capture/capture.mm",
        "src/engine/core/cli_options.cpp",
        "src/engine/core/cli_options.h",
        "src/engine/core/math.cpp",
        "src/engine/core/math.h",
        "src/engine/core/object_id.h",
        "src/engine/core/picking.cpp",
        "src/engine/core/picking.h",
        "src/engine/core/settings.cpp",
        "src/engine/core/settings.h",
        "src/engine/core/time.cpp",
        "src/engine/core/time.h",
        "src/engine/debug/bundle.cpp",
        "src/engine/debug/bundle.h",
        "src/engine/debug/debug_state.cpp",
        "src/engine/debug/debug_state.h",
        "src/engine/renderer/render_context.cpp",
        "src/engine/renderer/render_context.h",
        "src/engine/renderer/render_command.h",
        "src/engine/renderer/mesh_geometry.cpp",
        "src/engine/renderer/mesh_geometry.h",
        "src/engine/renderer/metal_renderer.h",
        "src/engine/renderer/metal_renderer.mm",
        "src/engine/scene/camera.cpp",
        "src/engine/scene/camera.h",
        "src/engine/scene/component.h",
        "src/engine/scene/component.cpp",
        "src/engine/scene/game_object.cpp",
        "src/engine/scene/game_object.h",
        "src/engine/scene/mesh_renderer.cpp",
        "src/engine/scene/mesh_renderer.h",
        "src/engine/scene/rotate_component.cpp",
        "src/engine/scene/rotate_component.h",
        "src/engine/scene/scene.cpp",
        "src/engine/scene/scene.h",
        "src/engine/scene/scene_serializer.cpp",
        "src/engine/scene/scene_serializer.h",
        "src/engine/scene/transform.cpp",
        "src/engine/scene/transform.h"
    }

    system "macosx"
    architecture "arm64"

    includedirs { "src", "src/engine", "src/ext", "external" }

    links {
        "Cocoa.framework",
        "Metal.framework",
        "QuartzCore.framework"
    }

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter {}

