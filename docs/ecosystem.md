# Ecosystem leverage map

What the Reforger community and free-asset sites can give Life Framework, and
under which terms. Surveyed 2026-11-08. Licenses were read from each repo's
`LICENSE` file through `raw.githubusercontent.com`, not from badges or the
GitHub license field. Workshop facts were read from the mod pages on
`reforger.armaplatform.com/workshop`. Nothing was imported in this change.

Related surveys: [EveronLife forks](everonlife-forks.md),
[Overthrow survey](overthrow-survey.md), [what the scopes reuse](roadmap/sources.md).

## What we can and cannot copy

Our code is MIT. A file we copy into the repo must carry a license that lets
us redistribute it under MIT terms, so only permissive licenses qualify.

| License | Copy into the repo? | Other routes |
|---|---|---|
| MIT, BSD, ISC, Apache-2.0, Boost (BSL-1.0), Unlicense | Yes. Keep the notice. | |
| CC0, public domain | Yes. Record provenance. | |
| CC-BY 3.0/4.0, OFL 1.1 (fonts) | Yes. Attribution required (OFL: keep the license text, rename the converted font). | |
| GPL-2.0/3.0, AGPL | No | Workshop dependency, inspiration, or ask the authors to relicense specific files |
| APL (Arma Public License) | No: non-commercial and Bohemia-games-only, which MIT is not | Dependency, inspiration, ask |
| APL-SA, CC-BY-SA | No: share-alike | Dependency, inspiration, ask |
| APL-ND, CC-BY-ND, CC-BY-NC-ND | No: no derivatives | Plain dependency only (no overriding its files), inspiration |
| CC-BY-NC | No | Inspiration |
| No license file | No: all rights reserved | Ask the author, inspiration |
| PolyForm Strict, other source-available | No | Read only |

**Workshop upload terms.** Section 3 of the
[Workshop Terms of Use](https://reforger.armaplatform.com/workshop-terms)
says uploaded content must be licensed to users under APL, APL-SA, APL-ND,
ADPL-SA, or a custom license that explicitly forbids commercial use. MIT is
not on that list. Decided 2026-11-08 (`docs/decisions.md`, "The Workshop build
is APL"): the Workshop build is published under APL and the GitHub source stays
MIT, starting with the playtest release (M1-24, #109).

**A GitHub licence governs the code in that repo.** Many mods carry an
APL-family licence on the Workshop (Bohemia requires one) and a permissive
licence on GitHub. An author may license the same work differently in different
places. When the copyright holder publishes the code on GitHub under MIT, BSD,
Apache-2.0, CC0, or CC-BY, that licence governs our use of the code in the repo,
whatever the Workshop page says. Two conditions: the repo is published by the
copyright holder (an official or author account, not a third-party re-upload),
and the grant covers only what is in the repo (an asset shipped only on the
Workshop stays under the Workshop licence). Every row below was checked against
this rule on 2026-11-08; no project in the tables was marked not-copyable
because of its Workshop licence alone. The Workshop-only candidates in section 2
have no source repo from their authors.

**Base game first.** Before pulling from anywhere, check the vanilla game:
`Configs/Commanding/Gestures/Gesture_*.conf` (follow, halt, point, salute,
get in), `EItemGesture`, the unconscious state and cameras, and the vanilla
furniture, bench, and chair props. These cost nothing and match the game's
look. `tools\cli call asset_search` does not index `.anm` files; browse
animations in Workbench.

## 1. GitHub repos

### Reforger and Enfusion projects

| Project | License (file read) | Last push | What it has for us | How | Effort | Helps |
|---|---|---|---|---|---|---|
| [SouLGenetic/LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) | MIT (2026 LNGCGenetic) | 2026-09-09 | Life-RP scripts: cuff, uncuff, escort, search plus results dialog, put in and take out of vehicle, revive, issue fine, dispatch calls list, ATM and transfer dialogs, garage, vehicle keys and trunk, player menu, nearby players list. Already ported: jail, cuff, escort, put in and take out of vehicle (`Feature/Police/LF_Jail*`), garage and vehicle spawn (`Feature/Vehicles/EL_VehicleGarage*`). | Copy (keep the header, as `LF_JailEscort.c` does). Do-not-port list in `roadmap/sources.md`. | Low per action | #98, #96, #105, #106, #121, #101 |
| [EveronLife/EveronLife](https://github.com/EveronLife/EveronLife) | MIT (2022 EveronLife) | 2026-07-28 | Our upstream. Forks surveyed in `everonlife-forks.md`. | Copy | Low | M2 |
| [acemod/ACE-Anvil](https://github.com/acemod/ACE-Anvil) | GPL-2.0 | 2026-11-06 | **Source `.anm` animations**: captives (surrender in/loop/out, tied in/loop/out, zip-cuff action and hold poses) and carrying (carried and dragged in/loop/out), with the `.asi`/`.agr` wiring. Also medical, radio, finger poses. | Workshop dependency (flagship), inspiration for the anim-graph wiring, **ask** the ACE team to license the captives and carrying animation files to us under MIT or CC-BY | Medium | #98, #106, #121 |
| [Arkensor/EnfusionPersistenceFramework](https://github.com/Arkensor/EnfusionPersistenceFramework) | MIT (2023 Arkensor) | 2026-04-18 (v0.6.15, 2025-11-01) | Deprecated by its author since game 1.6, replaced by the base-game persistence system. We already moved off it (`docs/persistence.md`). Read its hitzone and inventory serializers for ideas only. | Inspiration | n/a | #89, #95 |
| [Arkensor/EnfusionDatabaseFramework](https://github.com/Arkensor/EnfusionDatabaseFramework) | MIT | 2026-04-14 | JSON/binary/REST DB drivers. Useful only if we ever need an external DB. | Inspiration | n/a | M3+ |
| [blackwaterbread/Marx](https://github.com/blackwaterbread/Marx) | APL | 2026-10-11 | Generic private-property and economy framework: wallet, ledger, shop catalog, **stash grid**, loadouts, in-game test runner and debug panel. Well tested. | Inspiration, ask | n/a | #100, #102, test harness |
| [wyqydsyq/DynamicEconomy](https://github.com/wyqydsyq/DynamicEconomy) | None | 2026-09-05 | Cash, banking, ATM entity, traders with supply/demand, **`ATM.xob` and `Cash.xob` models**, funds HUD. | Ask (models), inspiration (price curves) | Low if granted | #110, supply-demand decision |
| [ELifeRPG/reforger-mod](https://github.com/ELifeRPG/reforger-mod) | APL-SA (backend repo AGPL-3.0) | 2026-11-08 | Active life mod with a REST backend: banking, companies, characters, chat emotes, car key model (`CarKey_01.xob`), locked-state indicator. | Inspiration | n/a | #102, #101 |
| [ArmaOverthrow/Overthrow.Arma4](https://github.com/ArmaOverthrow/Overthrow.Arma4) | APL-SA (badge says MIT; file wins). **For Life Framework only**, the owner's permission overrides it: use and relicense under MIT, with credit (decision 2026-11-08; record held by the maintainer) | 2026-09-18 | Persistence serializers, player and vehicle records, ownership and real estate, economy manager, respawn service, Game Master panel and records, civilian ambience, AI deployments, UI contexts. See `overthrow-survey.md` and pull #2 below. | **Copy with credit** (header in section 5) | Low to medium per system | #89, #95, #101, M2 |
| [gruppe-adler/DoorLock](https://github.com/gruppe-adler/DoorLock) | None | 2026-01-28 | Door lock action and component with **lock and rattle sounds** (`doorlock1/2.acp`, `rattle.acp`), AI door opening. | Ask (sounds), inspiration | Low | #106, property M2 |
| [zen-mod/GME_AR](https://github.com/zen-mod/GME_AR) | APL | 2026-11-05 | Game Master tools; admin attributes and context actions. | Inspiration | n/a | admin, M3 |
| [Kexanone/ArmaReforgerMods](https://github.com/Kexanone/ArmaReforgerMods), [COE2_AR](https://github.com/Kexanone/COE2_AR) | APL-SA | 2026-11-07 | Mature Reforger scripting by an ACE maintainer; AI tasking, objectives. | Inspiration | n/a | AI police M2+ |
| [borkedgb/AmbientBusNetwork](https://github.com/borkedgb/AmbientBusNetwork) | APL-SA | 2026-06-20 | Ambient bus system that finds bus-stop signs on the terrain. | Inspiration | n/a | M2 town life |
| [MrFrostDev/MrFrost-ServerEssentials](https://github.com/MrFrostDev/MrFrost-ServerEssentials) | None | 2026-08-01 | In-game info menu and player bug reports. | Inspiration, ask | n/a | #109 feedback |
| [plusflabby/Community-Pause-Menu](https://github.com/plusflabby/Community-Pause-Menu) | MIT | 2026-07-19 | Configurable community hub in the pause menu (links, rules). | Copy | Low | #109 |
| [plusflabby/AR-Persistent-Player-Stats](https://github.com/plusflabby/AR-Persistent-Player-Stats) | MIT | 2026-06-04 | Chat command over persisted stats. | Copy if wanted | Low | admin |
| [SyykoNT/Pistol-Lethality](https://github.com/SyykoNT/Pistol-Lethality) | MIT | 2026-10-26 | Small ACE-Medical tuning addon; a clean MIT example of an ACE-dependent addon. | Inspiration | n/a | ACE dependency pattern |
| [y0014984/Advanced-Equipment-for-Reforger](https://github.com/y0014984/Advanced-Equipment-for-Reforger) | APL-SA | 2023-10-27 | Usable laptops and terminals with an in-world screen. | Inspiration | n/a | #110 terminal screens |
| [MikkoIsHere/Taser-7-Reforger-Mod](https://github.com/MikkoIsHere/Taser-7-Reforger-Mod) | None | 2026-08-23 | Taser. | Ask | n/a | police M2 |
| [MelsomNO/ArmA-Reforger-Growing-Weed-Addon](https://github.com/MelsomNO/ArmA-Reforger-Growing-Weed-Addon) | None | 2026-10-16 | Plant-growing loop. | Inspiration | n/a | farming M2 |
| [CoalitionArma/COALITION-Lobby](https://github.com/CoalitionArma/COALITION-Lobby) | None | 2026-11-07 | Lobby UI. | Inspiration | n/a | spawn UX |
| [BohemiaInteractive/Arma-Reforger-Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples), [Arma-Reforger-Misc](https://github.com/BohemiaInteractive/Arma-Reforger-Misc) | APL | 2025 | Official samples (user actions, UI, components). | Inspiration | n/a | all |
| [NarcoMarshDev/Enforce-Script-Extensions](https://github.com/NarcoMarshDev/Enforce-Script-Extensions) | APL | 2022 | Script utility library. | Inspiration | n/a | |
| Older RP attempts: [ArmaRelife/ArmaLife](https://github.com/ArmaRelife/ArmaLife), [zenexgaming/ZXGLife](https://github.com/zenexgaming/ZXGLife), [jlatoria/Reforger-Life](https://github.com/jlatoria/Reforger-Life) | None | 2022-2024 | Abandoned stubs. | Ignore | | |

### Tooling

| Project | License | Last push | What it has for us | How | Helps |
|---|---|---|---|---|---|
| [steffenbk/enfusion-mcp-BK](https://github.com/steffenbk/enfusion-mcp-BK) | MIT | 2026-08-29 | Our primary MCP (`tools/mcp/enfusion-mcp-bk`). | In use | all |
| [Goldwep/enfusion-workbench-mcp](https://github.com/Goldwep/enfusion-workbench-mcp) | MIT | 2026-10 | Our back-pocket MCP (112 tools). | In use, disabled | all |
| [arma-reforger-competitive/arma-reforger-tools-mcp](https://github.com/arma-reforger-competitive/arma-reforger-tools-mcp), [25thplaymaking/enfusion-mcp-bifrost](https://github.com/25thplaymaking/enfusion-mcp-bifrost), [fma-rc/enfusion-mcp](https://github.com/fma-rc/enfusion-mcp), [EnfusionMCP/EnfusionMCP](https://github.com/EnfusionMCP/EnfusionMCP) | MIT | 2026-04 to 2026-10 | Other forks of the same MCP; read their changelogs for tools worth upstreaming into ours. | Copy tools | MCP |
| [Hexomanya/screenshot-toolbox](https://github.com/Hexomanya/screenshot-toolbox) | MIT | 2026-07-10 | Workbench tool for direct skeleton posing and bone control, for staged screenshots. | Copy | #91 |
| [Hexomanya/terrain-toolbox](https://github.com/Hexomanya/terrain-toolbox) | MIT | 2026-08-29 | Workbench terrain plugins and generators. | Copy | M2 town |
| [landaire/enfusion_tools](https://github.com/landaire/enfusion_tools), [WoozyMasta/edds](https://github.com/WoozyMasta/edds), [WoozyMasta/pac1](https://github.com/WoozyMasta/pac1) | MIT | 2026 | Read and write `.pak` and `.edds` outside Workbench. Useful for a validator that checks textures or a filming pipeline that reads assets. | Copy or call | validators, #91 |
| [acemod/docker-reforger](https://github.com/acemod/docker-reforger) | MIT | 2026-08-30 | Containerised dedicated server, mod download through `config.json`. | Copy | hosting M4 |
| [ZSU-GG-Reforger/ReforgerJS](https://github.com/ZSU-GG-Reforger/ReforgerJS) | BSL-1.0 | 2025-11 | Node RCON/log framework for server admin bots. | Copy | M4 ops |
| [WoozyMasta/bercon-cli](https://github.com/WoozyMasta/bercon-cli) | MIT | 2026-10-30 | BattlEye RCON CLI. | Use | #108, M4 |
| [LucasSKrewer/dz-testbench](https://github.com/LucasSKrewer/dz-testbench) | MIT | 2026-09-21 | DayZ Enforce Script test harness (boot-time server assertions). Same engine family; compare with our `EL_Test`. | Inspiration | tests |
| [MitoAtGit/reforger-modding-knowledge](https://github.com/MitoAtGit/reforger-modding-knowledge) | MIT | 2026-08-31 | Field-tested modding knowledge base. | Copy into skills where it helps | agents |
| [willy92wins/DayZ-Modding-Knowledge-Pack](https://github.com/willy92wins/DayZ-Modding-Knowledge-Pack), [StarDZ-Team/Dayz-Modding-Skills](https://github.com/StarDZ-Team/Dayz-Modding-Skills) | MIT | 2026 | Enforce Script agent knowledge (DayZ). | Inspiration | agents |
| [simonvic/tree-sitter-enforce](https://github.com/simonvic/tree-sitter-enforce) | MIT | 2026-09-17 | Enforce Script grammar; a real parser for structural lint. | Copy or depend | validators |
| [Mavericktfius/arma-reforger-modding](https://github.com/Mavericktfius/arma-reforger-modding) | CC-BY-SA-4.0 | 2026-09-27 | Blender-to-Enfusion reference. | Read only | assets |
| [CAD4Z/enfusion-skills](https://github.com/CAD4Z/enfusion-skills) | PolyForm Strict | 2026-10-19 | Agent skills; source-available, no reuse. | Read only | |
| [steffenbk/bk-reforger-blender-addons](https://github.com/steffenbk/bk-reforger-blender-addons) | None | 2026-04-02 | Blender rigging and FBX export for Reforger. | Use the tool (no redistribution) | assets |
| [soda3x/ArmaReforgerServerTool](https://github.com/soda3x/ArmaReforgerServerTool), [fugasjunior/arma-server-manager](https://github.com/fugasjunior/arma-server-manager) | GPL-3.0 | 2026 | Server managers. | Use, don't copy | M4 |

## 2. Workshop dependencies

A few Workshop dependencies are acceptable. Criteria, all required:

1. Maintained: updated for the current game version, more than one release a year.
2. Widely used: downloads in the hundreds of thousands or a known team.
3. Terms allow it: the license permits use as a dependency; we never edit or
   ship its files, and we only override its prefabs if the license allows
   derivatives.
4. It carries something we cannot reasonably build ourselves, mostly
   animations and art.
5. The framework core works without it. Only the flagship content pack (or an
   optional `LifeFramework_<Dep>` bridge addon) depends on it; the core
   detects the missing dependency and falls back with an `EL_Debug.Warn`.

| Candidate | ID | License | Facts (2026-11-08) | What it covers | Recommendation |
|---|---|---|---|---|---|
| **Bon Action Animations** (TheBonBon) | `5C9758250C8C56F1` | APL-ND | v1.0.3, built for game 1.7; updated 2026-06-01; 2.43 MB; 2.95M downloads, 95%; about monthly updates 2023-2025, then one per game update. No API docs or source repo found. | Gesture and emote radial (dances, idles, lean, cigarette, repair, sit on a box, salute-style military gestures), zip ties, "pull out" of a zip-tied player, seat in vehicle, shoulder tap. No surrender loop, escort, chair sitting, ATM, or give-item animations listed. | **Not now.** It is behind the current game version, has no documented API, and ND forbids overriding its files. Its emotes do not cover the M1 interactions. Re-check after it updates for 1.9; then it could be an optional flagship dependency for idle emotes only. "Bacon" mods are a different author (ceo_of_bacon) and are weapons and cosmetics; none apply. |
| **ACE Captives** | `646D52AF8BB3FF15` | GPL-2.0+ | v1.4.3, 2026-05-31; 986k downloads, 93%; needs ACE Core | Surrender (hands up) from the gesture radial, zip cuffs, release, escort, load into vehicle, with real animations | **Best fit for #98 and #106.** First ask ACE for the captives `.anm`/`.asi` files under a permissive license (it removes the dependency). If not granted, make it an optional flagship dependency behind a bridge addon; our cuff logic keeps working with a fallback pose. |
| **ACE Carrying** | `5DBD560C5148E1DA` | GPL-2.0+ | v1.4.3, 2026-05-31; 3.18M downloads, 94% | Carry and drag incapacitated players | Same route as Captives; M2 (drag a downed player, #121 follow-up). |
| ACE Medical Core | `60C4C12DAE90727B` | GPL-2.0+ | 1.93M downloads | Medical and revive | No. Our hospital respawn (#121) is simpler and in-world. Inspiration only. |
| Retro Police Cars (schmorrii) | `6A22A7F3AEF8A570` | APL-SA | v1.1.3, 2026-11-07; 8.2k downloads, 100%; needs Emergency Light Framework | Police-livery UAZ469/452 and S105 with sirens and lights, civilian faction | **M2 flagship candidate** for 1989 police cars. Small but active. Ask the author first; we already have `M151A2_Police_Base`. |
| Vehicle Ownership & Security | `6A26F2E514BEE220` | APL | v0.1.27, game 1.9, 2026-11-06; 2.6k downloads | Owner lock, access grants, remote immobilise | No. We have keys and locks; inspiration only. |
| Police Department (Ben.obj) | `5CB6E1FA81C7B8D5` | APL-ND | v0.0.2, 2023-12-04 (stale) | Police building with 4 interiors, 2 cells | No: stale. Build from vanilla buildings (#96, #106). |
| RHS Status Quo | `595F2BF2F44836FB` | CC BY-NC-ND 4.0 plus [EULA](https://docs.rhsmods.org/rhs-status-quo-user-documentation/arma-reforger/eula) | 4.07M downloads, 195 MB, game 1.9 | Uniforms, vehicles, weapons | **Never a dependency.** The EULA treats dependent mods as derivatives that must be ND-licensed and public, RHS can take them down at will, and any server monetization is forbidden. A server owner can still load it alongside us. |

**Declaring a dependency.** Add the Workshop GUID to `Dependencies` in
`addons/LifeFramework/LifeFramework.gproj` (today only `58D0FB3206B6F859`,
the base game). For the optional route, a separate bridge addon
(`addons/LifeFramework_ACE/LifeFramework_ACE.gproj`) depends on both
`LifeFramework` and the Workshop mod, so the core gproj never changes.
Dependencies of a dependency must also be resolvable. A hosted server lists
every mod in `config.json` `game.mods[]` by `modId` and downloads them.

**Effect on our test harness.** `tools\cli test` and `cli build` start the
server with `-noBackend` and `-addonsDir` set to the repo `addons` folder plus
a junction to the base game's `core`/`data` only (`serverArgs` in
`tools/cli.mjs`). With `-noBackend` nothing downloads, so a dependency must
already be on disk:

1. Subscribe once in the Workbench or game launcher; the packed addon lands in
   the user's Reforger addons folder (`Documents\My Games\ArmaReforger\addons`
   for the game, `...\ArmaReforgerWorkbench\addons` for Workbench).
2. Add a third `-addonsDir` entry for that folder (an env var such as
   `LF_WORKSHOP_ADDONS`, read in `serverArgs` and `cmdBuild`), and add the
   bridge addon to `-addons`.
3. The core suite keeps running with no dependency present. Bridge tests are
   tagged and skipped with a clear message when the folder or addon is
   missing, so a fresh worktree and CI (`validate` only) stay green.

No harness change is made until a dependency is actually adopted.

## 3. Asset sources

Every source below was checked against its own license page on 2026-11-08,
except rows marked *snippet* (the page refused automated fetches; terms come
from search excerpts of the official page) and *unverified*.

### 3D models and PBR textures

| Source | License | Attribution | Redistribute in a mod | Good for |
|---|---|---|---|---|
| [Poly Haven](https://polyhaven.com/license) | CC0 | No | Yes | Concrete, asphalt, plaster, grime textures; props; HDRIs for filming |
| [ambientCG](https://docs.ambientcg.com/license/) | CC0 | No | Yes, explicitly "in a video game" | 1980s tile, wallpaper, metal, paper |
| [Kenney](https://kenney.nl/support) | CC0 | No | Yes | Placeholder props, UI kits, SFX |
| [Quaternius](https://quaternius.com/faq.html) | CC0 | No | Yes | Stylized; mostly placeholders here |
| [3DTextures.me](https://3dtextures.me/about) | CC0 | No | Yes | PBR textures |
| [Smithsonian Open Access](https://www.si.edu/openaccess/faq) *snippet* | CC0 on badged items only | No | Yes for CC0 items | Scanned period objects |
| [OpenGameArt](https://opengameart.org/content/faq) *snippet* | Per asset | Per license | CC0, CC-BY, OGA-BY only | Misc |
| [Sketchfab](https://sketchfab.com/licenses) *snippet* | Per model | Unless CC0 | CC0 and CC-BY only. Standard and Editorial: no | Specific period props and vehicles |
| [BlenderKit](https://www.blendkit.com/docs/licenses/licensing-faq/) | CC0 or Royalty Free | | CC0 only; RF forbids user-extractable files | Filter to CC0 |
| [cgbookcase](https://www.cgbookcase.com) | "Free for any purpose" *unverified as CC0* | No | Probably | Textures |
| [ShareTextures](https://sharetextures.com/p/license) | Custom "CC0-based" with a redistribution ban | | **No** | Avoid |

### Decals, signage, icons

| Source | License | Attribution | Notes |
|---|---|---|---|
| [game-icons.net](https://game-icons.net/about.html) | CC BY 3.0 | Yes: "Icons made by {author}. Available on https://game-icons.net" | Interaction-menu and action icons (#98) |
| [Openclipart](https://openclipart.org/share) | CC0 | No | Skip items flagged `pd_issue` |
| [Font Awesome Free](https://github.com/FortAwesome/Font-Awesome/blob/6.x/LICENSE.txt) | Icons CC BY 4.0, fonts OFL | Yes | Skip brand icons |
| [Material Symbols](https://developers.google.com/fonts/docs/material_symbols) | Apache-2.0 | Keep license and NOTICE | Modern look |
| [Lucide](https://lucide.dev/license) | ISC | Keep notice | Modern look; poor fit for 1989 |
| [Wikimedia Commons](https://commons.wikimedia.org) | Per file | Per file | US federal works (MUTCD road signs) are public domain. Avoid BY-SA. |

### Fonts (OFL is fine)

OFL allows bundling in a game. Ship the license text and copyright line, and
because conversion to the engine font format is a modification, give the
converted font a new name that avoids any Reserved Font Name (for example
`LF_Typewriter`). Licenses read from each font's `METADATA.pb` in
[google/fonts](https://github.com/google/fonts).

| Font | License | Use |
|---|---|---|
| Courier Prime, Cutive Mono | OFL | Typewriter: tickets (#104), registration form and ID card (#93), police reports |
| Special Elite | Apache-2.0 | Worn typewriter |
| VT323, Share Tech Mono | OFL | ATM and terminal screens (#110) |
| Doto, DotGothic16, Silkscreen | OFL | Dot-matrix receipts and pager text |
| Overpass | OFL | Highway Gothic look for road signs (#94) |
| D-DIN (Datto) *snippet* | OFL | DIN 1451 European street signs (Everon) |
| IBM Plex Mono | OFL | Clean mono |
| OCR-B (Matthew Skala) *snippet* | Public domain dedication | ID-card machine-readable line |
| Roadgeek 2005 *unverified* | Reported non-commercial | Avoid; use Overpass |

### Sounds and music

| Source | License | Attribution | Redistribute in a mod | Notes |
|---|---|---|---|---|
| [Freesound](https://freesound.org/help/faq/) | Per file: CC0, CC-BY, CC-BY-NC, Sampling+ | For CC-BY | CC0 and CC-BY only | Best source for cash register, coins, note riffle, payphone and DTMF, typewriter, pager beep, cell door, alarm bell, radio squelch. Exclude NC and Sampling+. |
| [Kenney audio](https://kenney.nl/support) | CC0 | No | Yes | UI clicks and confirmations |
| [OpenGameArt audio](https://opengameart.org) | Per file | Per file | CC0 and CC-BY only | |
| [Incompetech](https://incompetech.com/music/royalty-free/faq.html) | CC BY 4.0 | Yes, exact credit line | Yes | Radio filler music; some tracks get YouTube Content ID claims (matters for the filming harness) |
| [Free Music Archive](https://freemusicarchive.org/faq/) | Per track | Per track | Exclude NC and ND | |
| [Sonniss GDC bundle](https://sonniss.com/gdc-bundle-license/) | Custom royalty-free | No | **Gray area**: forbids supplying the sounds in an "asset pack", and a mod ships extractable files | Ask Sonniss in writing first |
| [Pixabay sounds and music](https://pixabay.com/service/terms/) | Pixabay Content License | No | Bans standalone redistribution | Ask or skip |
| [Mixkit](https://mixkit.co/license/) *unverified* | Mixkit free license | | Reportedly finished projects only | Ask or skip |
| [Zapsplat](https://www.zapsplat.com/license-agreement) *snippet* | Standard license | Yes (free tier) | **No** | Exclude |
| BBC Sound Effects (RemArc) *snippet* | Personal and educational only | | **No** | Exclude |

### Animations

Every external animation must be retargeted to the Reforger character
skeleton in Blender before export, so even a free clip costs real work.

| Source | License | Redistribute in a mod | Notes |
|---|---|---|---|
| ACE-Anvil captives and carrying `.anm` | GPL-2.0 | Not under MIT | Already on the Reforger skeleton. **Ask ACE** for a permissive grant; best value of any animation source. |
| [CMU Graphics Lab mocap](http://mocap.cs.cmu.edu) *snippet* | Free, "no restrictions"; credit requested | Yes, from the original BVH/ASF | Sitting, hands up, gestures, handshakes. Third-party FBX conversions may carry other licenses; use CMU's originals. |
| Quaternius and Kenney animation packs | CC0 | Yes | Stylized rigs; limited use |
| Truebones free mocap (Gumroad) *snippet* | "Royalty free, any purpose" | Probably | Save the license text with the download |
| [Mixamo](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html) *snippet* | Adobe terms: royalty-free in games, but raw files may not be redistributed | **No** for a moddable addon without Adobe's written OK | Avoid |
| Rokoko free library, ActorCore *unverified* | Vendor EULAs | Assume no | Read the EULA first |
| Bandai Namco Research motion dataset | CC BY-NC 4.0 | **No** | Exclude |

### Enfusion import pipeline

The Bohemia wiki pages named here refused automated fetches; confirm details
with `tools\cli call wiki_read` before a first import.

| Asset | Pipeline | Effort |
|---|---|---|
| Model | FBX into the addon, Resource Browser "Register and Import" as model to a `.xob`; material names become `.emat` slots. Buildings face +Z. Pages: `Arma_Reforger:FBX_Import`, `:Prop_Creation`, `:Enfusion_Blender_Tools`. AGENTS.md: bake the real `.emat` into the `.xob`. | Low for a static prop; medium with LODs, colliders, destruction |
| Texture | PNG/TGA/TIF imports to `.edds` beside the source; the suffix sets compression (`_BCR` albedo, `_NMO` normal/metal/AO, `_MCR`, `_MASK`). Page: `Arma_Reforger:Textures`. | Low; verify visually (dedicated server has no renderer) |
| Sound | `.wav` (Opus supported since 1.6) into a Bank node of an Audio Editor `.acp`; a `SoundComponent` plays the event. Pages: `Arma_Reforger:Audio_Editor`, `:Audio:_Sound_Components`. | Low per sound once one `.acp` exists |
| Animation | Retarget in Blender to the Reforger skeleton, export `.txa` with a matching export profile, import to `.anm`, wire into an `.asi`/anim graph in the Animation Editor. Pages: `:Enfusion_Blender_Tools:_Import/Export_Animation`, `:Animation_Editor`. | High; plan a day for the first one |
| Font | Engine `.fnt`; no official TTF conversion page found. Check `wb_knowledge` before committing to a custom font. | Unknown; spike first |
| Icon / decal | PNG to `.edds`, then an imageset or a decal material. | Low |

## 4. Top 10 pulls

Ranked by value over effort, weighted toward what raises felt quality
(animation and sound).

| # | Pull | License | How | Effort | Helps |
|---|---|---|---|---|---|
| 1 | **Period SFX set** from Freesound (CC0 first, CC-BY with credit) and Kenney audio: cash register and coins, note handling, ATM keypad beeps, door and cell clang, bank alarm bell, radio squelch, payphone and DTMF, typewriter, pager beep | CC0 / CC-BY | Copy | Low | #105 alarm, #110 ATM and till, #106 jail, #104 ticket, #96 duty desk |
| 2 | **Overthrow systems** (paths under `Scripts/Game/`). Persistence: `Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` (versioned, append-only, idempotent apply), `OVT_VehicleManagerSerializer.c` (garage record), `OVT_StorageComponentSerializer.c`. Economy and ownership: `GameMode/Managers/OVT_EconomyManagerComponent.c`, `OVT_OwnerManagerComponent.c`, `OVT_RealEstateManagerComponent.c`. Police and AI: `GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c` (`SeatAgent` for escort), the other deployment modules for AI police, `GameMode/Civilians/OVT_CivilianAmbienceManagerComponent.c` for town life and witnesses. Moderation: `GameMode/GM/OVT_GMRecords.c`, `UI/GM/OVT_GMPanelUIComponent.c`. Respawn: `Services/OVT_RespawnService.c`. UI: `UI/Context/OVT_ShopContext.c` and `OVT_StorageContext.c` (screens built from parts), `UI/HUD/OVT_WantedInfo.c` | APL-SA, relicensed MIT for us by the owner | Copy with the Overthrow header, rename to `LF_` | Low to medium per system | #89, #95, #101, #106, #121, property, AI police, M2 moderation and investigation |
| 3 | **ACE captives and carrying animations** (surrender, tied, zip-cuff, carried, dragged) | GPL-2.0 | Ask ACE for a permissive grant on these files; fallback: optional flagship dependency on ACE Captives through a bridge addon | Low if granted, medium as dependency | #98 cuff/search/escort, #106 jail, #121 |
| 4 | **LNGCLifeRP_Public actions not yet ported**: search plus results dialog, uncuff, revive, nearby-players list, dispatch calls list, ATM transfer dialog layout | MIT | Copy with header | Low each | #98, #105, #121 |
| 5 | **Typewriter, terminal, dot-matrix, and DIN fonts** (Courier Prime, VT323, Doto, D-DIN, OCR-B) | OFL / PD | Copy, renamed | Medium (font conversion spike) | #93 ID card, #104 ticket, #110 screens, #94 signs |
| 6 | **Vanilla gestures and props first** (`Gesture_*.conf`, `EItemGesture`, chairs, benches) for give-item, show-ID, and wave reactions | Base game | Use | Low | #98, #93 |
| 7 | **CMU mocap** retargeted for sitting, hands-up idle, and counter use (ATM, till) where vanilla and ACE have nothing | Free, credit | Copy after retarget | High | #110, #98, M2 sitting |
| 8 | **Poly Haven and ambientCG textures** for paper, card stock, worn signage, and interior surfaces | CC0 | Copy | Low | #94, #93, #104, #120, #107 |
| 9 | **DynamicEconomy `ATM.xob` and `Cash.xob`, gruppe-adler DoorLock sounds** | None | Ask the authors | Low if granted | #110, #106 |
| 10 | **Hexomanya/screenshot-toolbox** skeleton posing for staged showcase shots | MIT | Copy into tools | Low | #91 filming, #109 showcase |

Next after these: game-icons.net icons (CC BY 3.0, copy with credit) for the interaction menu and Read cards, Retro Police Cars for M2 police vehicles (ask, APL-SA
dependency), Marx's stash grid and test runner as design reading for #100 and
the harness, and Community-Pause-Menu (MIT) for the playtest guide link in
#109.

## 5. Import policy

**Code**

1. Copy only MIT, BSD, ISC, Apache-2.0, BSL-1.0, Unlicense, or CC0 code. Read
   the `LICENSE` file on the commit you copy from; a badge or a README line
   does not count (Overthrow's badge says MIT, its file says APL-SA).
   The licence that counts is the one on the copy we take. A mod whose
   Workshop page says APL but whose author publishes the code on GitHub under
   MIT, BSD, Apache-2.0, CC0, or CC-BY is copyable from that repo, provided
   the copyright holder published it (not a third-party re-upload). Assets
   that exist only in the Workshop build stay under the Workshop licence.
2. Keep the original copyright and license text at the top of every copied
   file, followed by `// Ported from <repo> (<license>), author <name>.` as in
   `Feature/Police/LF_JailEscort.c`. Apache-2.0 also needs its NOTICE kept.
3. Rename to `LF_` per AGENTS.md; the header keeps the original names.
4. Add a line to `THIRD_PARTY.md` at the repo root (create it with the first
   import): file paths, source URL and commit, author, license. Add the
   project to README Credits.
5. Never copy GPL, AGPL, APL, APL-SA, APL-ND, CC-BY-SA, NC, ND, or unlicensed
   code, not even "rewritten line by line". Read it for design, then write our
   own from the behaviour.
   One exception: Overthrow, whose owner gave us permission to use and
   relicense its code under MIT with credit. A file copied or derived from it
   starts with
   `// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.`
   in place of the header in rule 2. README Credits and `THIRD_PARTY.md`
   already carry the project entry; add the file paths to `THIRD_PARTY.md`.
6. To ask an author, open an issue or Discord message naming the exact files
   and the license we need (MIT or CC-BY), and keep their written answer
   linked in `THIRD_PARTY.md`.

**Assets**

1. Allowed: CC0, public domain, CC-BY, OFL (fonts), MIT/Apache asset repos.
   Exclude NC, ND, SA, "standard" marketplace licenses, and anything without a
   readable license.
2. Keep the original download under `assets-source/<type>/` and record one
   line per asset in `assets-source/LICENSES.md`: asset, source URL, author,
   license, our resource path. CC-BY assets also go in README Credits (and a
   credits screen when we have one).
3. A converted font gets a new name and ships `OFL.txt`.

**Workshop dependencies**

1. Meet all five criteria in section 2; record the decision in
   `docs/decisions.md`.
2. Prefer an optional bridge addon over adding the GUID to the core
   `LifeFramework.gproj`.
3. Never edit, repack, or override the files of an ND or RHS-style mod.
4. The core test suite must pass without the dependency present.
