# Credits

Third-party material used in Life Framework media. The website credits page and README section are tracked in #480;
this file is where video and audio credits are recorded until then. Every track and voice gets a row before it is used.

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
