# Credits

Who and what Life Framework is built on. The website `/credits` page and the Credits section of `README.md` are built
from this file and `THIRD_PARTY.md`. Every track and voice gets a row before it is used. Credits are facts from the repo only; a
person's role is added when the maintainer confirms it.

## Projects we build on and borrow from

| Project | Author | License | What we took | Where it is recorded |
|---|---|---|---|---|
| [Everon Life RPG Framework](https://github.com/EveronLife/EveronLife) | Arkensor and its contributors | MIT | Life Framework is a fork and continuation of it. The upstream `EL_` class prefix became `LF_` on 2026-11-09; the credit stays. | `README.md`, `docs/ecosystem.md` |
| [Overthrow](https://github.com/ArmaOverthrow/Overthrow.Arma4) | Aaron Static, the maintainer of Overthrow | Arma Public License Share Alike upstream; used with the owner's permission and relicensed MIT for Life Framework only | Money, resource and shop data code; the persistence lessons in `AGENTS.md`. Each derived file names its source in a header. | `THIRD_PARTY.md`, `docs/overthrow-survey.md` |
| [LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) | LNGCGenetic | MIT | Police, jail, garage and vehicle code. Each ported file keeps its notice. | `THIRD_PARTY.md`, `docs/ecosystem.md` |
| [ArchBuilder](https://arribas22.github.io/archbuilder/) | Arribas | GPL-3.0-or-later add-on, sold with a personal key ([ko-fi.com/arribas](https://ko-fi.com/arribas)) | Ideas and code the building pipeline is built around. The pipeline drives the user's own installed copy; the repo contains none of its source. | `docs/building-pipeline/README.md`, `docs/architecture/building-pipeline-spike.md` |
| Bohemia Interactive | Bohemia Interactive | Arma Public License (the Workshop build); game assets belong to Bohemia Interactive | Base-game assets, and the Arma Reforger engine. Life Framework is not affiliated with Bohemia Interactive. | `README.md`, `assets-source/LICENSES.md` |

Code ported from MIT forks of Everon Life, each with a `// Ported from` line in the file:

- [thebonbon/EveronLife](https://github.com/thebonbon/EveronLife): roleplay chat, mining, jump pad and spike strip code. The mining code was ported with the author's permission given to the maintainer.
- [rekterakathom/EveronLife](https://github.com/rekterakathom/EveronLife): parts of the inventory storage manager and the shop rules.
- [Jacob-Mango/EveronLife](https://github.com/Jacob-Mango/EveronLife): parts of the quantity component.

The per-file list is `THIRD_PARTY.md`. The forks are surveyed in `docs/everonlife-forks.md`.

## Contributors

Everyone who has committed to the repository is on the [GitHub contributors list](https://github.com/Life-Framework/Life-Framework/graphs/contributors),
counted from the commit history. The website page shows the same list with commit counts. Most agent-written commits carry the
name of the person who ran the agent.

## Licenses

- The source in this repository is MIT (`LICENSE`). Contributions are licensed under MIT (`CONTRIBUTING.md`).
- The build published on the Arma Reforger Workshop is licensed under the Arma Public License (APL), because the Workshop Terms of Use require an APL-family license for uploaded content.
- Third-party code keeps its own license and notice (`THIRD_PARTY.md`). Third-party assets are listed in `assets-source/LICENSES.md`: so far only the CC0 texture sets under "Textures (CC0)".

## Tools

- Arma Reforger Tools and the Workbench IDE by Bohemia Interactive build and test the mod.
- The clothing pipeline (`docs/asset-pipeline/clothing.md`) runs Bohemia Interactive's Enfusion Blender Tools (GPL-3.0) from the local
  Arma Reforger Tools install, and downloads the character templates of the [Arma Reforger Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples)
  (Arma Public License) at run time. Neither is committed; a garment built from a Samples mesh credits Bohemia Interactive here.
- [ArchBuilder](https://arribas22.github.io/archbuilder/) by Arribas, a Blender add-on, generates the buildings (`tools\cli building`,
  `docs/building-pipeline/README.md`). Each user runs their own licensed copy; none of its source, keys or buyer details are in the repo.
- [Piper](https://github.com/rhasspy/piper) makes the trailer voice-over (see Voice-over below).
- The website is Vite, React, TypeScript and Tailwind, hosted on Cloudflare Pages, built from this repository and GitHub (`website/README.md`).
- Life Framework is built with AI coding agents under the maintainer's direction, on a Claude subscription (`docs/community/support.md`).

## Generated 3D assets

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Taser model (`Assets/Weapons/Handguns/Taser/Taser.xob`) | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/taser/meshy.json` (generation ids), `tools/assets/taser/spec.json` (prompt) |
| Handcuffs model (`Assets/Items/Roleplay/Handcuffs/Handcuffs.xob`) | Modeled in Blender by `tools/assets/handcuffs/model.py` (articulated strands; the first version was generated with Meshy, see `meshy.json`), textures baked from procedural nodes | n/a | Our own work, no license needed | `tools/assets/handcuffs/model.py`, `tools/assets/handcuffs/spec.json`, `tools/assets/handcuffs/meshy.json` (the replaced Meshy generation) |
| Vault drill model (`Assets/Items/Roleplay/Drill/Drill.xob`), from a generated concept picture (concept 1 of `tools/assets/drill/concepts.json`) | Meshy text-to-image, then Meshy image-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; generated 2026-11-13, under the sign-off of 2026-11-13 for generated models (#664, `docs/decisions.md`) | `tools/assets/drill/meshy.json` (generation ids, the concept's prompt), `tools/assets/drill/spec.json` |
| Bank vault door and frame detail (`Assets/Props/Roleplay/BankVault/VaultDoor.xob`, the frame ring of `VaultFront.xob`): Meshy image-to-3D of a concept picture, itself made by Meshy image-to-image from the vault-door concept 1 | Meshy image-to-image and image-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/vault-front/meshy.json` (generation ids), `tools/assets/vault-front/concepts.json` (prompts, reference, pick) |
| Spike strip model (`Assets/Props/Roleplay/SpikeStrip/SpikeStrip.xob`, `SpikeStrip_Folded.xob`): one generated link, repeated and stacked by the clean stage | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; signed off 2026-11-13 (#664) | `tools/assets/spike-strip/meshy.json` (generation ids), `tools/assets/spike-strip/spec.json` (prompt) |
| Cash strongbox model (`Assets/Props/Roleplay/CashStrongbox/CashStrongbox.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/cash-strongbox/meshy.json` (generation ids), `tools/assets/cash-strongbox/spec.json` (prompt) |
| Lockpick model (`Assets/Items/Roleplay/Lockpick/Lockpick.xob`) | Modeled in Blender by `tools/assets/lockpick/model.py` (a leather wallet, hook pick and tension wrench; the first version was generated with Meshy, see `meshy.json`), textures baked from procedural nodes | n/a | Our own work, no license needed | `tools/assets/lockpick/model.py`, `tools/assets/lockpick/spec.json`, `tools/assets/lockpick/meshy.json` (the replaced Meshy generation) |
| Balaclava (`Assets/Characters/HeadGear/LF/Balaclava/`, `Prefabs/Characters/HeadGear/LF/Balaclava.et`) | Modeled in Blender by `tools/assets/balaclava/model.py`: cut from the head of Bohemia's base body (`Body_LOD0` in the Arma Reforger Samples, downloaded at run time and never committed), pushed out 6 mm with an eye opening; the knit maps are drawn by the same script (numpy). Skinned to the vanilla character skeleton. | https://github.com/BohemiaInteractive/Arma-Reforger-Samples | Mesh derived from the Samples body: Arma Public License (Bohemia Interactive), so it stays in Arma; the knit textures are our own work. No generation credits (#1997) | `tools/assets/balaclava/model.py`, `tools/assets/balaclava/spec.json` |
| Doll mask (`Assets/Characters/HeadGear/LF/DollMask/`, `Prefabs/Characters/HeadGear/LF/DollMask.et`) | Modeled in Blender by `tools/assets/doll-mask/model.py` on `tools/assets/pipeline/blender/mask_shell.py`: cut from the head of Bohemia's base body (`Body_LOD0` in the Arma Reforger Samples, downloaded at run time and never committed), held clear of the skin, with eye openings and an elastic strap; the painted face (smile, cheeks, brows, lashes) is drawn by the same script (numpy). Skinned to the vanilla character skeleton. | https://github.com/BohemiaInteractive/Arma-Reforger-Samples | Mesh derived from the Samples body: Arma Public License (Bohemia Interactive), so it stays in Arma; the painted textures are our own work. No generation credits, nothing from any film | `tools/assets/doll-mask/model.py`, `tools/assets/doll-mask/spec.json` |
| Hockey mask (`Assets/Characters/HeadGear/LF/HockeyMask/`, `Prefabs/Characters/HeadGear/LF/HockeyMask.et`) | Modeled in Blender by `tools/assets/hockey-mask/model.py` on the same `mask_shell.py` (cut from the Samples body head as above, with eye slits and an elastic strap); the faded paint, vent holes and cracks are drawn by the same script (numpy). Skinned to the vanilla character skeleton. | https://github.com/BohemiaInteractive/Arma-Reforger-Samples | Mesh derived from the Samples body: Arma Public License (Bohemia Interactive), so it stays in Arma; the painted textures are our own work. No generation credits, no film markings | `tools/assets/hockey-mask/model.py`, `tools/assets/hockey-mask/spec.json` |
| 1989 general-store food and drink models (`LF_Everon1989/Assets/Items/Store1989/`: `CannedFish`, `CannedMeat`, `CoffeeTin`, `MilkBottle`, `ChocolateBar`, `PickleJar`) | Modeled in Blender by `tools/assets/canned-fish/model.py`, `canned-meat/model.py`, `coffee-tin/model.py`, `milk-bottle/model.py`, `chocolate-bar/model.py` and `pickle-jar/model.py` (plain color wrappers, no text or brands), textures baked from procedural nodes; the bread and soda water items use the base game meshes `S2_Bread` and `SodaBottle_01` (a siphon) | n/a | Our own work, no license needed (the two base game meshes are Bohemia Interactive's) | `tools/assets/canned-fish/spec.json`, `canned-meat`, `coffee-tin`, `milk-bottle`, `chocolate-bar`, `pickle-jar` |
| Bakery and butcher foods (`LF_Everon1989/Assets/Items/Food/`: `Baguette`, `Croissant`, `Saucisson`) | Modeled in Blender by `tools/assets/baguette/model.py`, `croissant/model.py` and `saucisson/model.py` (plain colors baked from procedural nodes, no text or brands); the ham joint item uses the base game mesh `MeatLeg_01` | n/a | Our own work, no license needed (the ham mesh is Bohemia Interactive's) | `tools/assets/baguette/spec.json`, `croissant`, `saucisson` |
| Fishing rod, bait tin and trout models (`Assets/Items/Fishing/FishingRod`, `BaitTin`, `FishTrout`, `FishTroutSmall`, `FishTroutLarge`) | Modeled in Blender by `tools/assets/fishing-rod/model.py`, `bait-tin/model.py`, `fish-trout/model.py` (the small and large trout are the same script, scaled), textures baked from procedural nodes | n/a | Our own work, no license needed | `tools/assets/fishing-rod/spec.json`, `tools/assets/bait-tin/spec.json`, `tools/assets/fish-trout*/spec.json` |
| Fishing float model (`Assets/Items/Fishing/FishingFloat`) | Modeled in Blender by `tools/assets/fishing-float/model.py` (a red-and-white round bobber on a quill), texture baked from procedural nodes | n/a | Our own work, no license needed | `tools/assets/fishing-float/spec.json` |
| Fish models: mackerel, bass, tuna, perch, carp, each in three sizes (`Assets/Items/Fishing/FishMackerel`, `FishBass`, `FishTuna`, `FishPerch`, `FishCarp` plus their `Small` and `Large`; 15 `.xob`, one Meshy generation per species, scaled per size by the clean stage) | Meshy text-to-3D (preview, refine, remesh), generated with Meshy; no CC0 or CC-BY fish fit, so the library rungs came first and were skipped | https://www.meshy.ai | The maintainer's Meshy plan; generated 2026-11-18 (#3134, `docs/decisions.md` sign-off of 2026-11-13 for generated models) | `tools/assets/fish-<species>/meshy.json` (generation ids; the `-small` and `-large` folders point at the same ids), `tools/assets/fish-<species>/spec.json` (prompts) |
| Evidence marker model (`Assets/Props/Roleplay/EvidenceMarker/EvidenceMarker.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/evidence-marker/meshy.json` (generation ids), `tools/assets/evidence-marker/spec.json` (prompt) |
| Pager model (`Assets/Items/Roleplay/Pager/Pager.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/pager/meshy.json` (generation ids), `tools/assets/pager/spec.json` (prompt) |
| Money bag model (`Assets/Items/Roleplay/MoneyBag/MoneyBag.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/money-bag/meshy.json` (generation ids), `tools/assets/money-bag/spec.json` (prompt) |
| Apple juicer model (`Assets/Props/Roleplay/AppleJuicer/AppleJuicer.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/apple-juicer/meshy.json` (generation ids), `tools/assets/apple-juicer/spec.json` (prompt) |
| Police scanner model (`Assets/Items/Fence/PoliceScanner/PoliceScanner.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/police-scanner/meshy.json` (generation ids), `tools/assets/police-scanner/spec.json` (prompt) |
| Post parcel model (`Assets/Items/Roleplay/PostParcel/PostParcel.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/post-parcel/meshy.json` (generation ids), `tools/assets/post-parcel/spec.json` (prompt) |
| Moonshine still model (`Assets/Props/Roleplay/MoonshineStill/MoonshineStill.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/moonshine-still/meshy.json` (generation ids), `tools/assets/moonshine-still/spec.json` (prompt) |
| Smelter furnace model (`Assets/Props/Roleplay/SmelterFurnace/SmelterFurnace.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/smelter-furnace/meshy.json` (generation ids), `tools/assets/smelter-furnace/spec.json` (prompt) |
| Dispatch console model (`Assets/Props/Roleplay/DispatchConsole/DispatchConsole.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/dispatch-console/meshy.json` (generation ids), `tools/assets/dispatch-console/spec.json` (prompt) |

## Textures (CC0)

Downloaded by `tools\cli asset` (the generate stage of a Blender-made asset) and baked into our own maps; the source
files are not in the repository. ambientCG releases every asset under CC0 1.0 (https://docs.ambientcg.com/license/,
https://creativecommons.org/publicdomain/zero/1.0/): no attribution required, credited here anyway.

| Set | Author | Source | License | Used in |
|---|---|---|---|---|
| Metal 046 B | ambientCG | https://ambientcg.com/view?id=Metal046B | CC0 1.0 | Iron ingot: dark forge-scaled steel (`Assets/Resources/Mining/Ingot/Data/MetalIngot_Iron_*`) |
| Metal 047 B | ambientCG | https://ambientcg.com/view?id=Metal047B | CC0 1.0 | Copper ingot; native copper specks in copper ore |
| Metal 048 B | ambientCG | https://ambientcg.com/view?id=Metal048B | CC0 1.0 | Gold ingot; gold flecks in gold ore and in the gold quartz veins |
| Metal 058 C | ambientCG | https://ambientcg.com/view?id=Metal058C | CC0 1.0 | Copper ingot patina; malachite in copper ore and the copper veins |
| Rust 009 | ambientCG | https://ambientcg.com/view?id=Rust009 | CC0 1.0 | Iron ingot rust spots; iron ore; iron staining on the ore rocks |
| Rock 035 | ambientCG | https://ambientcg.com/view?id=Rock035 | CC0 1.0 | Host rock of the iron and copper nuggets; grain in the gold quartz |
| Rock 058 | ambientCG | https://ambientcg.com/view?id=Rock058 | CC0 1.0 | Host rock of the ore veins on the quarry rocks (darkened and desaturated; ore only in its crevices and thin veins) (`Assets/Resources/Mining/OreVein/`) |
| Metal 038 | ambientCG | https://ambientcg.com/view?id=Metal038 | CC0 1.0 | Handcuff and lockpick steel: scratches, scuffs, roughness (`Assets/Items/Roleplay/Handcuffs/Data/`, `Lockpick/Data/`) |
| Leather 014 | ambientCG | https://ambientcg.com/view?id=Leather014 | CC0 1.0 | Lockpick wallet leather grain (`Assets/Items/Roleplay/Lockpick/Data/`) |

Which set feeds which map is in each asset's spec (`tools/assets/metal-ingot`, `ore-nugget`, `ore-veins`, `source.textures`).

## Poly Haven models (CC0)

Taken whole from [Poly Haven](https://polyhaven.com/models) by the source stage of `tools\cli asset` instead of being generated
(`docs/asset-pipeline/sources.md`). Poly Haven releases its models under CC0 1.0 (https://polyhaven.com/license): no attribution
required, credited here anyway with the URL. The stage writes each row; `tools/assets/<slug>/source.json` holds the file hash.
A model under CC-BY would also need its author credited in this table.

| Model | Author | Source | License | Recorded in |
|---|---|---|---|---|
| Cement Bag (`Assets/Items/Resources/CementBag/CementBag.xob`) | PierreB3D | https://polyhaven.com/a/cement_bag | CC0 1.0 | `tools/assets/cement-bag/source.json` |

## Library and purchased models

Models taken from the open libraries through 3d-asset-server (CC0, CC-BY) or bought by a human on CGTrader and similar markets
(Royalty Free), then run through `clean` and `import` (`docs/asset-pipeline/sources.md`). The source stage writes each row. A CC-BY
model needs this credit before it ships; a purchased model also needs the store page and the seller. The raw files are not
committed (`tmp/assets/<slug>/purchased/` is gitignored); only the compiled assets are.

| Model | Author | Source | License | Recorded in |
|---|---|---|---|---|

## Generated sound effects (ElevenLabs)

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Taser sounds: shot, dry fire, cartridge out and in, stun buzz (`Sounds/Weapons/Handguns/Taser/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-13 on the maintainer's Starter plan, a paid plan with a commercial license. The output may be used in the project and in published builds as part of the mod; it may not be distributed as stand-alone audio files or a sample pack (`docs/decisions.md`, 2026-11-13) | `tools/assets/taser/sounds.json` (every generation: prompt, parameters, date; the picks and why) |

## Generated audio
| Vault drill sounds: motor loop, grinding loop (`Sounds/Items/Drill/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-14 on the maintainer's Starter plan, same terms as the taser sounds above | `tools/assets/drill/sounds.json` (every generation: prompt, parameters, date; the picks and why) |

| Robbery alarms: bank bell, fuel station intruder alarm, van horn, shop burglar bell (`Sounds/Alarm/Robbery/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-14 on the maintainer's Starter plan, same terms as the taser sounds above. The fuel and van loops are one generated take cut into an on/off cadence (`derive` in `sounds.json`). The shop bell was generated 2026-11-16 (4 takes, take 1 picked, seamless loop) | `tools/assets/robbery-alarms/sounds.json` (every generation: prompt, parameters, date; the picks and why) |
| Retail store alarm (`Sounds/Alarm/Robbery/Samples/Alarm_Robbery_LF_ALARM_STORE_LP_01.wav`) | Synthesised by this project: two band-limited square-wave tones (1400 and 1000 Hz), no sample or model | none | Original work, same license as the repo | `tools/assets/retail-alarm/synth.mjs` (deterministic; `tools/validation/retail-alarm-selftest.mjs` checks the committed file is its output) |
| Hospital robbery alarm (`Sounds/Alarm/Robbery/Samples/Alarm_Robbery_LF_ALARM_HOSPITAL_LP_01.wav`) | Synthesised by this project: a rising three-note chime (784, 988 and 1175 Hz sine tones with quiet overtones), no sample or model | none | Original work, same license as the repo | `tools/assets/hospital-alarm/synth.mjs` (deterministic; `tools/validation/hospital-alarm-selftest.mjs` checks the committed file is its output and that no other robbery alarm clip carries its pitches; the record is in `tools/assets/robbery-alarms/sounds.json`) |
| Jailbreak siren (`Sounds/Alarm/Jail/Samples/Alarm_Jail_LF_ALARM_JAILBREAK_LP_01.wav`) | Synthesised by this project: four 0.7 s blasts of a 196 Hz sawtooth buzzer (harmonics 1 to 6) with a slight wobble, no sample or model | none | Original work, same license as the repo | `tools/assets/jail-siren/synth.mjs` (deterministic; `tools/validation/jail-siren-selftest.mjs` checks the committed file is its output) |

Music and ambience generated with ElevenLabs.

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Store radio and pub music (`Sounds/Atmosphere/Samples/Atmosphere_StoreRadio_LP.wav`, `Atmosphere_PubRadio_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, Music Terms of 2026-10-09: use inside the mod; no attribution required. **Not under this repository's MIT license**: the tracks may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompts, dates, credits) |
| Radio track "Estrada ballad", a slow 92 bpm estrada ballad (`Sounds/Atmosphere/Samples/Atmosphere_RadioEstradaBallad_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Radio track "Estrada dance", an upbeat estrada dance number (`Sounds/Atmosphere/Samples/Atmosphere_RadioEstradaDance_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Radio track "Accordion waltz", a 3/4 accordion waltz (`Sounds/Atmosphere/Samples/Atmosphere_RadioAccordionWaltz_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Radio track "Accordion kolo", a 2/4 Balkan kolo (`Sounds/Atmosphere/Samples/Atmosphere_RadioAccordionKolo_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Radio track "Orchestral morning", a light orchestral morning piece (`Sounds/Atmosphere/Samples/Atmosphere_RadioOrchestralMorning_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Radio track "Orchestral serenade", a Mediterranean serenade (`Sounds/Atmosphere/Samples/Atmosphere_RadioOrchestralSerenade_LP.wav`) | Music generated with ElevenLabs (ElevenLabs Music, `music_v1`, instrumental) | https://elevenlabs.io | The maintainer's Starter plan, which allows commercial use: use inside the mod. **Not under this repository's MIT license**: the track may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompt, model, date, pick) |
| Background loops: furnace, sawmill, quarry, farm, still, police desk, chop shop, square, bank murmur, bank till and clock, hospital equipment, hospital ward, payphone line, fuel pump, cash van idle (`Sounds/Atmosphere/Samples/`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed) | `tools/assets/atmosphere/ambience.json` |
| Ambience beds: harbor by day (gulls, water, rigging), harbor by night (water only) and village night (crickets, a far dog) (`Sounds/Ambience/Samples/Atmosphere_HarborDay_LP.wav`, `Atmosphere_HarborNight_LP.wav`, `Atmosphere_VillageNight_LP.wav`) | ElevenLabs text-to-sound-effects, loop on, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-23 on the maintainer's Starter plan, same terms as the taser sounds above. Vanilla has no prop-level sound for these: its harbor and night ambience is the global soundscape (`Sounds/Environment/Ambients2D`). #4227 | `tools/assets/atmosphere/ambience-beds.json` (every generation: prompt, parameters, date, credits; the picks and why) |

## Generated voices

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Town clerk's spoken lines: all thirteen, the old arrival lines and the newcomer's welcome (`LF_Everon1989/Sounds/Dialogue/clerk_everon/*.wav`) | ElevenLabs text-to-speech, voice "Julien - Serious" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Replaced the earlier ElevenLabs "JC" recordings and then the Piper ljspeech ones on 2026-11-17 (one voice per character, voices in ElevenLabs); no Piper audio of the clerk remains | `tools/assets/dialogue/clerk_everon/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/clerk_everon/dialogue.json` |
| Cafe owner's seven lines (`LF_Everon1989/Sounds/Dialogue/cafe_owner/cafe_owner_*.wav`) | ElevenLabs text-to-speech, voice "Mariva Viva Muse - Warm and Energetic" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Replaced the Piper ljspeech recordings on 2026-11-17; no Piper audio of her remains | `tools/assets/dialogue/cafe_owner/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/cafe_owner/dialogue.json` |
| Gendarme's three lines (`LF_Everon1989/Sounds/Dialogue/gendarme/`) | ElevenLabs text-to-speech, voice "Cedric - Calm" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Dispatcher's 27 radio lines (`LF_Everon1989/Sounds/Dialogue/dispatcher/`) | ElevenLabs text-to-speech, voice "Eric - Smooth, Trustworthy" from the ElevenLabs premade voices. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/dispatcher/ |
| Dockworkers' four lines (`.../dialogue/dockworker/`) | ElevenLabs text-to-speech, voice "Delgado Stephane" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Fishmonger's four lines (`.../dialogue/dock_fishmonger/`) | ElevenLabs text-to-speech, voice "Chris - Charming, Down-to-Earth" from the ElevenLabs premade voices. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Chandler's four lines (`.../dialogue/dock_chandler/`) | ElevenLabs text-to-speech, voice "George - Warm, Captivating Storyteller" from the ElevenLabs premade voices. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Passers-by's four lines (`.../dialogue/passerby/`) | ElevenLabs text-to-speech, voice "Sylvain - International english" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Relocated walkers' four lines (`.../dialogue/relocated_walker/`) | ElevenLabs text-to-speech, voice "Real Jeremy - Conversational" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Anselme's nine lines (`.../dialogue/resident_anselme/`) | ElevenLabs text-to-speech, voice "Bacbuc - Audiobooks" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Marius Caron's nine lines (`.../dialogue/resident_mireille/`) | ElevenLabs text-to-speech, voice "Alex - Warm, Empathetic, Confident" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Pierre Vasseur's (general store) nine lines (`.../dialogue/resident_store_clerk/`) | ElevenLabs text-to-speech, voice "Kevin Nitcheu" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Lucien Roux's (bank) nine lines (`.../dialogue/resident_teller/`) | ElevenLabs text-to-speech, voice "JP - Social media" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Shopkeepers' five lines (`.../dialogue/shopkeeper_everon/`) | ElevenLabs text-to-speech, voice "James - French Podcast" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Store clerks' five role lines (`.../dialogue/role_store_clerk_everon/`) | ElevenLabs text-to-speech, voice "Jamie - French Accent & Charismatic" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Bank teller's five role lines (`.../dialogue/role_teller_everon/`) | ElevenLabs text-to-speech, voice "Nicolas - French English Male" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Desk officer's five role lines (`.../dialogue/role_police_everon/`) | ElevenLabs text-to-speech, voice "Benjamin - Deep and Calm" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Clinic doctor's five role lines (`.../dialogue/role_medic_everon/`) | ElevenLabs text-to-speech, voice "Antoine - Articulate, Calm and Bright" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Mechanics' five role lines (`.../dialogue/role_mechanic_everon/`) | ElevenLabs text-to-speech, voice "Loic" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| The fence's five role lines (`.../dialogue/role_fence_everon/`) | ElevenLabs text-to-speech, voice "Lucas - Deep, Raspy and Natural" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Town civilians' five role lines (`.../dialogue/role_townsfolk_everon/`) | ElevenLabs text-to-speech, voice "Eliot - Relaxed & Casual" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| The market trader's five role lines (`.../dialogue/role_trader_everon/`) | ElevenLabs text-to-speech, voice "Nicolas Estrem - Mature" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). | tools/assets/dialogue/ |
| Night-shift worker's spoken lines (`LF_Everon1989/Sounds/Dialogue/night_shift/*.wav`) | ElevenLabs text-to-speech, voice "Raspy - Convincing and Weary Narrator" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Replaced the Piper ljspeech recordings on 2026-11-17; no Piper audio of him remains | `tools/assets/dialogue/night_shift/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/night_shift/dialogue.json` |
| Old man Gaspard Lenoir's six lines (`LF_Everon1989/Sounds/Dialogue/old_man/old_man_*.wav`) | ElevenLabs text-to-speech, voice "Alder - Warm, Raspy and Comforting" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Replaced the Piper ljspeech recordings on 2026-11-17; no Piper audio of him remains | `tools/assets/dialogue/old_man/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/old_man/dialogue.json` |
| Go-between Hugo Valmy's five spoken lines (`LF_Everon1989/Sounds/Dialogue/shipment_go_between/shipment_go_between_*.wav`) | ElevenLabs text-to-speech, voice "Cyril - French-Accented Narrator" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Generated 2026-11-19 (#3809) | `tools/assets/dialogue/shipment_go_between/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/shipment_go_between/dialogue.json` |
| Tally clerk Marcel Brun's five spoken lines (`LF_Everon1989/Sounds/Dialogue/tally_clerk/tally_clerk_*.wav`) | ElevenLabs text-to-speech, voice "Antonin - Warm, Unsettling, Elegant" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's plan (commercial use allowed). Generated 2026-11-23 (#4199) | `tools/assets/dialogue/tally_clerk/` |
| Barkeeper Josselin Vidal's five spoken lines (`LF_Everon1989/Sounds/Dialogue/barkeeper/barkeeper_*.wav`) | ElevenLabs text-to-speech, voice "Hank Harvey - Gruff, Rugged and Wise" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's plan (commercial use allowed). Generated 2026-11-23 (#4439) | `tools/assets/dialogue/barkeeper/` |
| Registrar Gaspard Perrin's six spoken lines (`LF_Everon1989/Sounds/Dialogue/registrar/registrar_*.wav`) | ElevenLabs text-to-speech, voice "Liam - Energetic, Social Media Creator" (premade). Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's plan (commercial use allowed). Generated 2026-11-23 (#4457) | `tools/assets/dialogue/registrar/` |
| Harbor master Marcel Roux's fourteen spoken lines (ten work lines, four shipment lines #3444) (`LF_Everon1989/Sounds/Dialogue/harbour_master/harbour_master_*.wav`) | ElevenLabs text-to-speech, voice "LaRauque - Hoarse, Deep, and Dramatic" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Generated 2026-11-18 (#3050), 2026-11-19 (#3444) | `tools/assets/dialogue/harbour_master/elevenlabs.json` (each line's voice, settings, text, hash), `tools/assets/dialogue/harbour_master/dialogue.json` |

## Music

| Track | Author | Source | License | Used in |
|---|---|---|---|---|
| "Carefree" | Kevin MacLeod (incompetech.com) | https://incompetech.com/music/royalty-free/mp3-royaltyfree/Carefree.mp3 | Creative Commons Attribution 4.0 (https://creativecommons.org/licenses/by/4.0/) | `video/tooling-test.json` (internal tooling test, not posted) |
| "Hitman" | Kevin MacLeod (incompetech.com) | https://incompetech.com/music/royalty-free/mp3-royaltyfree/Hitman.mp3 | Creative Commons Attribution 4.0 (https://creativecommons.org/licenses/by/4.0/) | Launch trailer v1 and its cut-downs (`video/trailer-v1/cut.mjs`, #571; not posted until the maintainer signs it off) |

Attribution text for a video description (CC BY 4.0 needs it wherever the music is published):

```
Music: "Carefree" by Kevin MacLeod (incompetech.com)
Licensed under Creative Commons: By Attribution 4.0 License
http://creativecommons.org/licenses/by/4.0/
```

For the launch trailer, the same text with the track name "Hitman".

Only music with a license that allows use in a project video on any platform goes here: Kevin MacLeod (CC BY 4.0),
Pixabay Music (Pixabay license), CC0 tracks from the Free Music Archive. Never the YouTube Audio Library or anything
licensed for one platform only. Tracks are downloaded to `video/music/` (git-ignored); the repo holds the record, not the file.

## Voice-over

| Tool or voice | Author | Source | License |
|---|---|---|---|
| Piper (offline TTS binary, release 2023.11.14-2) | Michael Hansen / Rhasspy | https://github.com/rhasspy/piper | MIT |
| Voice `en_US-ljspeech-high` | Trained from scratch on LJ Speech (Keith Ito) | https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/ljspeech/high | Public domain (dataset https://keithito.com/LJ-Speech-Dataset/) |

Description line: "Voice: Piper en_US-ljspeech (LJ Speech, public domain)".

Not used: `en_US-libritts_r` (it is fine-tuned from `en_US-lessac`, whose Blizzard 2013 data is licensed for research
only, Research License Agreement clause 3.2, so it cannot go in a public promo; dropped 2026-11-11), lessac itself, and
voices fine-tuned from the Ryan or HiFi-TTS speakers (non-commercial, CC BY-NC-SA). Before adopting any other voice,
read its model card down to the dataset it was first trained on.
The newer Piper 1.x is GPL-3.0; this repo uses only the MIT 2023 binary, which cannot load the 2025 voices (for example `joe`).

## Fonts and logo

Captions and cards use the system sans-serif (Segoe UI on Windows). The logo is `website/public/brand/logo-1024.png`.
| Anniversary mayor Bastien Girard's four lines (`LF_Everon1989/Sounds/Dialogue/anniversary_mayor/*.wav`) | ElevenLabs text-to-speech, voice "Henri - French-American Narrator" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). #3466 | `tools/assets/dialogue/anniversary_mayor/elevenlabs.json`, `tools/assets/dialogue/anniversary_mayor/dialogue.json` |
| Anniversary priest Father Étienne Morel's memorial speech (`LF_Everon1989/Sounds/Dialogue/anniversary_priest/*.wav`) | ElevenLabs text-to-speech, voice "JC - Deep, Thoughful and Encouraging" from the ElevenLabs voice library. Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). #3466 | `tools/assets/dialogue/anniversary_priest/elevenlabs.json`, `tools/assets/dialogue/anniversary_priest/dialogue.json` |
| Gaspard Lenoir's two anniversary letter lines (`LF_Everon1989/Sounds/Dialogue/old_man/old_man_anniversary_letter_*.wav`) | ElevenLabs text-to-speech, voice "Alder - Warm, Raspy and Comforting" (his existing voice). Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). #3466 | `tools/assets/dialogue/old_man/elevenlabs.json` |

## Liveries

Repainted vehicle bodies and door plates written by `tools\cli livery build`.

| Asset | Source and license | What it is for | Where |
|---|---|---|---|
| LF_UAZ469_Police | Derived from Bohemia Interactive `UAZ469_Body_CIV_BLUE.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Police repaint and door plates for the UAZ469 | `addons/LF_Everon1989/Assets/Liveries/UAZ469/` |
| LF_S105_Police | Derived from Bohemia Interactive `S105Body_mat_beige.emat` (Arma Public License, base-game asset) | Police repaint for the S105 | `addons/LF_Everon1989/Assets/Liveries/S105/` |
| LF_UAZ469_Rebel | Derived from Bohemia Interactive `UAZ469_Body_CIV_BLUE.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Rebel mud-and-olive repaint and hand-painted door number for the UAZ469 | `addons/LF_Everon1989/Assets/Liveries/UAZ469/` |
| LF_Ural4320_Rebel | Derived from Bohemia Interactive `Ural_4320_Body_CIV_Forest.emat`, `SignWall_01_90x45`, `CamoNet_PalletMRE`, `CamoNet_PalletAmmo_Soviet` and the camo-net materials CS1 (Arma Public License, base-game asset) | Rebel mud-and-olive repaint for the Ural4320, with a hand-painted door number and camo nets over the bed (LF_Ural4320_RebelKit) | `addons/LF_Everon1989/Assets/Liveries/Ural4320/` |
| LF_S1203_Police | Derived from Bohemia Interactive `S1203_body.emat` (Arma Public License, base-game asset) | Police repaint for the S1203 | `addons/LF_Everon1989/Assets/Liveries/S1203/` |
| LF_UAZ452_Ambulance | Derived from Bohemia Interactive `uaz452_body_VAI.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | White ambulance repaint and AMBULANCE side plates for the UAZ452 | `addons/LF_Everon1989/Assets/Liveries/UAZ452/` |
| LF_S1203_Ambulance | Derived from Bohemia Interactive `S1203_body.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | White ambulance repaint with red stripe and AMBULANCE side plates for the S1203 | `addons/LF_Everon1989/Assets/Liveries/S1203/` |
| LF_S105_Taxi | Derived from Bohemia Interactive `S105Body_mat_beige.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Yellow taxi repaint with TAXI quarter-panel and roof plates for the S105 | `addons/LF_Everon1989/Assets/Liveries/S105/` |
| LF_S1203_CompanyBakery | Derived from Bohemia Interactive `S1203_body.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Cream bakery van repaint with the EVERON BAKERY side plates for the S1203 | `addons/LF_Everon1989/Assets/Liveries/S1203/` |
| LF_S1203_CompanyFactory | Derived from Bohemia Interactive `S1203_body.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Factory-blue van repaint with the EVERON WORKS side plates for the S1203 | `addons/LF_Everon1989/Assets/Liveries/S1203/` |
| LF_UAZ452_CompanyHardware | Derived from Bohemia Interactive `uaz452_body_VAI.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Red hardware van repaint with the EVERON HARDWARE side plates for the UAZ452 | `addons/LF_Everon1989/Assets/Liveries/UAZ452/` |
| LF_Ural4320_CompanyTimber | Derived from Bohemia Interactive `Ural_4320_Body_CIV_Forest.emat` and `SignWall_01_90x45` (Arma Public License, base-game asset) | Forest-green timber truck repaint with the EVERON TIMBER cab-door plates for the Ural4320 | `addons/LF_Everon1989/Assets/Liveries/Ural4320/` |

## Contraband labels

English print for the cigarette pack and the vodka bottle (#4383). The brands GANNET and CORVIN are invented. Both keep the vanilla mesh; each colour map is the vanilla map with the pack's or label's own patches repainted by `tools/assets/contraband-labels/build.mjs` (Blender, the two fonts Blender ships under open licenses).

| Asset | Source and license | What it is for | Where |
|---|---|---|---|
| LF_CigaretteCarton_BCR | Derived from Bohemia Interactive `LitterInterior_01_V1_BCR.edds` (Arma Public License, base-game asset); the print is ours | English print on the cigarette pack (`LF_CigaretteCarton`) | `addons/LF_Everon1989/Assets/Items/Contraband/LF_CigaretteCarton/` |
| LF_VodkaBottle_BCR | Derived from Bohemia Interactive `S2_Vodka_paper_BCR.edds` (Arma Public License, base-game asset); the print is ours | English label on the vodka bottle (`LF_VodkaBottle`) | `addons/LF_Everon1989/Assets/Items/Contraband/LF_VodkaBottle/` |

## Work-site sounds

| Asset | Source and license | What it is for | Where |
|---|---|---|---|
| Work-site sounds: shift whistle, mine tool bed, sawmill tool bed (`Sounds/Work/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-23 on the maintainer's Starter plan, same terms as the taser sounds above (vanilla had no shift whistle or matching loop) | `tools/assets/site-ambience/site-ambience.json` (every generation: prompt, parameters, date) and `addons/LF_Everon1989/Sounds/Work/manifest.json` |
| Desk sergeant's five lines (`LF_Everon1989/Sounds/Dialogue/desk_sergeant/`) | ElevenLabs text-to-speech, voice "Adam - Dominant, Firm" (ElevenLabs premade). Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed). Generated 2026-11-23 | `tools/assets/dialogue/desk_sergeant/elevenlabs.json`, `dialogue.json` |
| Booking typewriter and ink pad stamp sounds (`LF_Everon1989/Sounds/Police/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-23; the base game has no typewriter or stamp sound | `tools/assets/booking-sfx/sounds.json` |
| The north outpost's spoken lines: quartermaster (4), contraband buyer (5), gate guards (5) (`LF_Everon1989/Sounds/Dialogue/outpost_*/outpost_*_*.wav`) | ElevenLabs text-to-speech, premade voices "Bill - Wise, Mature, Balanced" (quartermaster), "Callum - Husky Trickster" (buyer) and "Harry - Fierce Warrior" (gate guards). Voice generated with ElevenLabs | https://elevenlabs.io | The maintainer's plan (commercial use allowed). Generated 2026-11-23 (#4459) | `tools/assets/dialogue/outpost_quartermaster/`, `outpost_buyer/`, `outpost_guard/` |
