add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- Set --core_only=y to configure just the SDK-free core (architect-tests)
-- with ZERO game dependencies: no repository clones, no package downloads,
-- no LeviLamina headers. Core CI uses this; full mod builds omit it.
option("core_only")
    set_default(false)
    set_showmenu(true)
    set_description("SDK-free core only (architect-tests), skip the game dependency tree")
option_end()

if not has_config("core_only") then
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
end

-- Pinned to the verified 26.51 client/server line (see docs/TOOLCHAIN.md).
-- xmake-repo versions.txt lists 26.51.0-26.51.7; the 26.51.* pattern floats
-- across patches within that line only. Record the exact resolved version
-- from `xmake require --info levilamina` in docs/TOOLCHAIN.md at build time.
-- tooth.json must name the same line (client variant).
-- Skipped entirely under --core_only=y (core CI never resolves these).
if not has_config("core_only") then
add_requires("levilamina 26.51.*", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")
end

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

-- The game mod target needs the LeviLamina tree: skip it under --core_only=y
-- (rules/packages would fail to resolve without their requires).
if not has_config("core_only") then
target("architect") -- Architect: Litematica-style schematic blueprint overlay (client mod).
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    -- C++23: LeviLamina 26.51 headers use std::to_underlying (Reflection.h).
    -- The template's c++20 fails against the pinned line (CI evidence
    -- 2026-10-13). Core stays C++20 (architect-tests) for portability.
    set_languages("c++23")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    if is_config("target_type", "server") then
    --  add_includedirs("src-server")
    --  add_files("src-server/**.cpp")
    else
    --  add_includedirs("src-client")
    --  add_files("src-client/**.cpp")
    end
target_end()
end -- core_only: architect target needs the game tree

-- Pure-logic core tests. No LeviLamina dependency: builds with the host
-- toolchain (no clang-cl, no modpacker) so the core stays honest about
-- its SDK-free boundary. Validated by core CI (--core_only=y).
-- Run: `xmake f --core_only=y && xmake build architect-tests && xmake run architect-tests`
target("architect-tests")
    set_kind("binary")
    set_languages("c++20")
    set_symbols("debug")
    -- Warnings visible in CI logs. Deliberately NOT -Werror: warning sets
    -- differ across MSVC/Clang/GCC and cannot all be observed from here;
    -- promote to error once all three compilers are green (see ci.yml).
    set_warnings("all")
    add_includedirs("src")
    add_files("src/architect/**.cpp")
    add_files("tests/cpp/*.cpp")
target_end()
