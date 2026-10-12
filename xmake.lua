add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- Pinned to the verified 26.51 client/server line (see docs/TOOLCHAIN.md).
-- xmake-repo versions.txt lists 26.51.0-26.51.7; the 26.51.* pattern floats
-- across patches within that line only. Record the exact resolved version
-- from `xmake require --info levilamina` in docs/TOOLCHAIN.md at build time.
-- tooth.json must name the same line (client variant).
add_requires("levilamina 26.51.*", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

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
    set_languages("c++20")
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

-- Pure-logic core tests. No LeviLamina dependency: builds with the host
-- MSVC toolchain (no clang-cl, no modpacker) so the core stays honest
-- about its SDK-free boundary. PROPOSED until CI runs it (docs/TOOLCHAIN.md).
-- Run: `xmake build architect-tests && xmake run architect-tests`
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
