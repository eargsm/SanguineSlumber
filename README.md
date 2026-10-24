# Sanguine Slumber

An SKSE plugin for Skyrim: **Vampiric Drain no longer spreads Sanguinare Vampiris.** Instead, every
sleep carries a small chance of waking up infected — higher in the wild than in town.

- Vampires' (and Serana's) Vampiric Drain can no longer infect the player.
- Each sleep rolls a chance to contract the vanilla disease, which then progresses exactly like vanilla
  (cure it within 3 days, or turn).
- Safe places (inns, homes, temples, castles, town/city grounds) and the wild (camps, caves, ruins, forts,
  wilderness) can use separate chances.
- Configurable in game through [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)
  (optional), or through `SKSE/Plugins/SanguineSlumber.ini`.
- No ESP, no edited records, nothing stored in saves.

One DLL for SE 1.5.97, AE 1.6.x / 1.7.x and VR. Tested on AE 1.6.1170.

## How it works

| Piece | Mechanism |
|-------|-----------|
| Drain block | Every vanilla vampire Drain Health effect carries `VampireAbsorbHealthScript`, which calls `DoCombatSpellApply(DiseaseSanguinareVampiris)` on the player ~0.07 s after the hit. The plugin hooks the player's `MagicTarget::AddTarget` (vfunc 0x01, `VTABLE_PlayerCharacter[4]`) and refuses `Skyrim.esm:0B8780` only within a few seconds of a vampire drain hit. The Redwater Den bloodspring and Dawnguard traps use a separate spell and are never touched. |
| Sleep roll | `TESSleepStopEvent` — the same event vanilla `PlayerSleepQuestScript` uses for Well Rested. Safe vs wild comes from the current location's `LocTypeDwelling` / `LocTypeHabitation` / `LocTypePlayerHouse` keywords. |
| Infection | The disease is **cast** on the player (`CastSpellImmediate`) — `AddSpell` on a disease only adds an inert spell-list entry whose effect never starts. |
| Menu | SKSE Menu Framework 3, registered at `kPostLoad`. No import dependency: the plugin runs on its INI alone if the framework is absent. |

## Building

Requirements: Visual Studio 2022 Build Tools (C++ workload, Windows 11 SDK), [xmake](https://xmake.io) 3.0+, git.

```bat
git clone --recurse-submodules https://github.com/eargsm/SanguineSlumber
cd SanguineSlumber
xmake f -p windows -a x64 --toolchain=msvc -m releasedbg -y
xmake build -y
```

Output: `build/windows/x64/releasedbg/SanguineSlumber.dll`. To install straight into a mod manager folder,
set `XSE_TES5_MODS_PATH` (e.g. `F:\MO2\mods`) and run `xmake install` — it creates
`<mods>/SanguineSlumber/SKSE/Plugins/` with the DLL, PDB and default INI.

Gotchas:
- Run xmake from **PowerShell or cmd**, not Git Bash — under MSYS it detects MinGW and the package installs fail.
- If the DirectXTK package fails with `'CompileShaders.cmd' is not recognized`, the environment variable
  `NoDefaultCurrentDirectoryInExePath` is set (some tools set it for their shells). Clear it for the build.

## Third-party code

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) — git submodule (`lib/commonlibsse-ng`).
- `extern/SKSEMenuFramework/SKSEMenuFramework.h` — consumer header from
  [QTR-Modding/SKSE-Menu-Framework-3-Example](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-Example),
  MIT licensed (see `extern/SKSEMenuFramework/LICENSE`).
- Project layout from [libxse/commonlibsse-ng-template](https://github.com/libxse/commonlibsse-ng-template).

## License

GPL-3.0 with the modding exception in [`EXCEPTIONS`](EXCEPTIONS). See [`LICENSE`](LICENSE).
