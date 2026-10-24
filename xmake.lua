-- include subprojects
includes("lib/commonlibsse-ng")

-- set project constants
set_project("SanguineSlumber")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("SanguineSlumber")
    add_rules("commonlibsse-ng.plugin", {
        name = "SanguineSlumber",
        author = "Rain",
        description = "Vampiric Drain no longer spreads Sanguinare Vampiris; sleeping carries a small chance instead"
    })

    -- ship the settings file next to the DLL
    add_installfiles("dist/SanguineSlumber.ini", { prefixdir = "SKSE/Plugins" })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    -- SKSE Menu Framework 3 consumer header (QTR-Modding/SKSE-Menu-Framework-3-Example, MIT)
    add_includedirs("extern/SKSEMenuFramework")
    set_pcxxheader("src/pch.h")
