# Credits

Who and what Life Framework is built on. The website `/credits` page and the Credits section of `README.md` are built
from this file and `THIRD_PARTY.md`. Every track and voice gets a row before it is used. Credits are facts from the repo only; a
person's role is added when the maintainer confirms it.

## Projects we build on and borrow from

| Project | Author | Licence | What we took | Where it is recorded |
|---|---|---|---|---|
| [Everon Life RPG Framework](https://github.com/EveronLife/EveronLife) | Arkensor and its contributors | MIT | Life Framework is a fork and continuation of it. The upstream `EL_` class prefix became `LF_` on 2026-11-09; the credit stays. | `README.md`, `docs/ecosystem.md` |
| [Overthrow](https://github.com/ArmaOverthrow/Overthrow.Arma4) | Aaron Static, the maintainer of Overthrow | Arma Public License Share Alike upstream; used with the owner's permission and relicensed MIT for Life Framework only | Money, resource and shop data code; the persistence lessons in `AGENTS.md`. Each derived file names its source in a header. | `THIRD_PARTY.md`, `docs/overthrow-survey.md` |
| [LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) | LNGCGenetic | MIT | Police, jail, garage and vehicle code. Each ported file keeps its notice. | `THIRD_PARTY.md`, `docs/ecosystem.md` |
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

## Licences

- The source in this repository is MIT (`LICENSE`). Contributions are licensed under MIT (`CONTRIBUTING.md`).
- The build published on the Arma Reforger Workshop is licensed under the Arma Public License (APL), because the Workshop Terms of Use require an APL-family licence for uploaded content.
- Third-party code keeps its own licence and notice (`THIRD_PARTY.md`). Third-party assets are listed in `assets-source/LICENSES.md`: so far only the CC0 texture sets under "Textures (CC0)".

## Tools

- Arma Reforger Tools and the Workbench IDE by Bohemia Interactive build and test the mod.
- The clothing pipeline (`docs/asset-pipeline/clothing.md`) runs Bohemia Interactive's Enfusion Blender Tools (GPL-3.0) from the local
  Arma Reforger Tools install, and downloads the character templates of the [Arma Reforger Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples)
  (Arma Public License) at run time. Neither is committed; a garment built from a Samples mesh credits Bohemia Interactive here.
- [Piper](https://github.com/rhasspy/piper) makes the trailer voice-over (see Voice-over below).
- The website is Vite, React, TypeScript and Tailwind, hosted on Cloudflare Pages, built from this repository and GitHub (`website/README.md`).
- Life Framework is built with AI coding agents under the maintainer's direction, on a Claude subscription (`docs/community/support.md`).

## Generated 3D assets

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Taser model (`Assets/Weapons/Handguns/Taser/Taser.xob`) | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/taser/meshy.json` (generation ids), `tools/assets/taser/spec.json` (prompt) |
| Handcuffs model (`Assets/Items/Roleplay/Handcuffs/Handcuffs.xob`) | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/handcuffs/meshy.json` (generation ids), `tools/assets/handcuffs/spec.json` (prompt) |
| Vault drill model (`Assets/Items/Roleplay/Drill/Drill.xob`), from a generated concept picture (concept 1 of `tools/assets/drill/concepts.json`) | Meshy text-to-image, then Meshy image-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; generated 2026-11-13, under the sign-off of 2026-11-13 for generated models (#664, `docs/decisions.md`) | `tools/assets/drill/meshy.json` (generation ids, the concept's prompt), `tools/assets/drill/spec.json` |
| Bank vault door and frame detail (`Assets/Props/Roleplay/BankVault/VaultDoor.xob`, the frame ring of `VaultFront.xob`): Meshy image-to-3D of a concept picture, itself made by Meshy image-to-image from the vault-door concept 1 | Meshy image-to-image and image-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/vault-front/meshy.json` (generation ids), `tools/assets/vault-front/concepts.json` (prompts, reference, pick) |
| Spike strip model (`Assets/Props/Roleplay/SpikeStrip/SpikeStrip.xob`, `SpikeStrip_Folded.xob`): one generated link, repeated and stacked by the clean stage | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; signed off 2026-11-13 (#664) | `tools/assets/spike-strip/meshy.json` (generation ids), `tools/assets/spike-strip/spec.json` (prompt) |
| Cash strongbox model (`Assets/Props/Roleplay/CashStrongbox/CashStrongbox.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/cash-strongbox/meshy.json` (generation ids), `tools/assets/cash-strongbox/spec.json` (prompt) |
| Lockpick model (`Assets/Items/Roleplay/Lockpick/Lockpick.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/lockpick/meshy.json` (generation ids), `tools/assets/lockpick/spec.json` (prompt) |
| Evidence marker model (`Assets/Props/Roleplay/EvidenceMarker/EvidenceMarker.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/evidence-marker/meshy.json` (generation ids), `tools/assets/evidence-marker/spec.json` (prompt) |
| Pager model (`Assets/Items/Roleplay/Pager/Pager.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan | `tools/assets/pager/meshy.json` (generation ids), `tools/assets/pager/spec.json` (prompt) |
| Money bag model (`Assets/Items/Roleplay/MoneyBag/MoneyBag.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/money-bag/meshy.json` (generation ids), `tools/assets/money-bag/spec.json` (prompt) |
| Apple juicer model (`Assets/Props/Roleplay/AppleJuicer/AppleJuicer.xob`) | Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/apple-juicer/meshy.json` (generation ids), `tools/assets/apple-juicer/spec.json` (prompt) |

## Textures (CC0)

Downloaded by `tools\cli asset` (the generate stage of a Blender-made asset) and baked into our own maps; the source
files are not in the repository. ambientCG releases every asset under CC0 1.0 (https://docs.ambientcg.com/license/,
https://creativecommons.org/publicdomain/zero/1.0/): no attribution required, credited here anyway.

| Set | Author | Source | Licence | Used in |
|---|---|---|---|---|
| Metal 046 B | ambientCG | https://ambientcg.com/view?id=Metal046B | CC0 1.0 | Iron ingot: dark forge-scaled steel (`Assets/Resources/Mining/Ingot/Data/MetalIngot_Iron_*`) |
| Metal 047 B | ambientCG | https://ambientcg.com/view?id=Metal047B | CC0 1.0 | Copper ingot; native copper specks in copper ore |
| Metal 048 B | ambientCG | https://ambientcg.com/view?id=Metal048B | CC0 1.0 | Gold ingot; gold flecks in gold ore and in the gold quartz veins |
| Metal 058 C | ambientCG | https://ambientcg.com/view?id=Metal058C | CC0 1.0 | Copper ingot patina; malachite in copper ore and the copper veins |
| Rust 009 | ambientCG | https://ambientcg.com/view?id=Rust009 | CC0 1.0 | Iron ingot rust spots; iron ore; iron staining on the ore rocks |
| Rock 035 | ambientCG | https://ambientcg.com/view?id=Rock035 | CC0 1.0 | Host rock of the iron and copper nuggets; grain in the gold quartz |
| Rock 058 | ambientCG | https://ambientcg.com/view?id=Rock058 | CC0 1.0 | Host rock of the ore veins on the quarry rocks (`Assets/Resources/Mining/OreVein/`) |

Which set feeds which map is in each asset's spec (`tools/assets/metal-ingot`, `ore-nugget`, `ore-veins`, `source.textures`).

## Generated sound effects (ElevenLabs)

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Taser sounds: shot, dry fire, cartridge out and in, stun buzz (`Sounds/Weapons/Handguns/Taser/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-13 on the maintainer's Starter plan, a paid plan with a commercial licence. The output may be used in the project and in published builds as part of the mod; it may not be distributed as stand-alone audio files or a sample pack (`docs/decisions.md`, 2026-11-13) | `tools/assets/taser/sounds.json` (every generation: prompt, parameters, date; the picks and why) |

## Generated audio
| Vault drill sounds: motor loop, grinding loop (`Sounds/Items/Drill/Samples/*.wav`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | Generated 2026-11-14 on the maintainer's Starter plan, same terms as the taser sounds above | `tools/assets/drill/sounds.json` (every generation: prompt, parameters, date; the picks and why) |

Music and ambience generated with ElevenLabs.

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Store radio and pub music (`Sounds/Atmosphere/Samples/Atmosphere_StoreRadio_LP.wav`, `Atmosphere_PubRadio_LP.wav`) | ElevenLabs Music (`music_v1`, instrumental), generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan, Music Terms of 2026-10-09: use inside the mod; no attribution required. **Not under this repository's MIT licence**: the tracks may not be taken out of the mod and reused or offered as a music library | `tools/assets/atmosphere/ambience.json` (prompts, dates, credits) |
| Background loops: furnace, sawmill, quarry, farm, still, police desk, chop shop, square, bank murmur, bank till and clock, hospital equipment, hospital ward, payphone line, fuel pump, cash van idle (`Sounds/Atmosphere/Samples/`) | ElevenLabs text-to-sound-effects, generated with ElevenLabs | https://elevenlabs.io | The maintainer's Starter plan (commercial use allowed) | `tools/assets/atmosphere/ambience.json` |

## Generated voices

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Town clerk's spoken lines (`Sounds/Dialogue/clerk/*.wav`) | ElevenLabs text-to-speech, voices generated with ElevenLabs; voice "JC" from the voice library | https://elevenlabs.io | The maintainer's paid plan (Starter): commercial use allowed, no attribution required; use in the project and published builds per `docs/decisions.md` 2026-11-13 | `tools/assets/dialogue/clerk/elevenlabs.json` (request ids, voice, model, text), `tools/assets/dialogue/clerk/dialogue.json` |

## Music

| Track | Author | Source | Licence | Used in |
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

Only music with a licence that allows use in a project video on any platform goes here: Kevin MacLeod (CC BY 4.0),
Pixabay Music (Pixabay licence), CC0 tracks from the Free Music Archive. Never the YouTube Audio Library or anything
licensed for one platform only. Tracks are downloaded to `video/music/` (git-ignored); the repo holds the record, not the file.

## Voice-over

| Tool or voice | Author | Source | Licence |
|---|---|---|---|
| Piper (offline TTS binary, release 2023.11.14-2) | Michael Hansen / Rhasspy | https://github.com/rhasspy/piper | MIT |
| Voice `en_US-ljspeech-high` | Trained from scratch on LJ Speech (Keith Ito) | https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/ljspeech/high | Public domain (dataset https://keithito.com/LJ-Speech-Dataset/) |

Description line: "Voice: Piper en_US-ljspeech (LJ Speech, public domain)".

Not used: `en_US-libritts_r` (it is fine-tuned from `en_US-lessac`, whose Blizzard 2013 data is licensed for research
only, Research Licence Agreement clause 3.2, so it cannot go in a public promo; dropped 2026-11-11), lessac itself, and
voices fine-tuned from the Ryan or HiFi-TTS speakers (non-commercial, CC BY-NC-SA). Before adopting any other voice,
read its model card down to the dataset it was first trained on.
The newer Piper 1.x is GPL-3.0; this repo uses only the MIT 2023 binary, which cannot load the 2025 voices (for example `joe`).

## Fonts and logo

Captions and cards use the system sans-serif (Segoe UI on Windows). The logo is `website/public/brand/logo-1024.png`.
