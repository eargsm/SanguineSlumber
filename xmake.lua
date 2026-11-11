includes("lib/commonlibsse-ng")

set_project("SanguineSlumber")
set_version("1.2.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("SanguineSlumber")
    add_rules("commonlibsse-ng.plugin", {
        name = "SanguineSlumber",
        author = "Rain",
        description = "Vampires may feed on you in your sleep: Drained, a bite mark, and a chance of Sanguinare Vampiris"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_includedirs("extern/SKSEMenuFramework")
    set_pcxxheader("src/pch.h")
