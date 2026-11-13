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
- Third-party code keeps its own licence and notice (`THIRD_PARTY.md`). Third-party assets are listed in `assets-source/LICENSES.md`; none are used yet.

## Tools

- Arma Reforger Tools and the Workbench IDE by Bohemia Interactive build and test the mod.
- [Piper](https://github.com/rhasspy/piper) makes the trailer voice-over (see Voice-over below).
- The website is Vite, React, TypeScript and Tailwind, hosted on Cloudflare Pages, built from this repository and GitHub (`website/README.md`).
- Life Framework is built with AI coding agents under the maintainer's direction, on a Claude subscription (`docs/community/support.md`).

## Generated 3D assets

| Asset | Tool | Source | Terms | Recorded in |
|---|---|---|---|---|
| Taser model (`Assets/Weapons/Handguns/Taser/Taser.xob`) | Meshy text-to-3D, generated with Meshy | https://www.meshy.ai | The maintainer's Meshy plan; use in the project and published builds signed off 2026-11-13 (#664, `docs/decisions.md`) | `tools/assets/taser/meshy.json` (generation ids), `tools/assets/taser/spec.json` (prompt) |

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
