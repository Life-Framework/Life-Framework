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
| Voice `en_US-libritts_r-medium` | Trained on LibriTTS-R (Koizumi et al.) | https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/libritts_r/medium | CC BY 4.0 (dataset http://www.openslr.org/141/) |

The voice is fine-tuned from Piper's `en_US-lessac-medium`, whose dataset carries its own terms. Before a voice-over
ships in a posted video, read the lessac model card
(https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/lessac/medium) and keep the line
"Voice: Piper TTS, en_US-libritts_r (CC BY 4.0)" in the description. Voices fine-tuned from the Ryan or HiFi-TTS
speakers are non-commercial (CC BY-NC-SA) and are not used.
The newer Piper 1.x is GPL-3.0; this repo uses only the MIT 2023 binary, which cannot load the 2025 voices (for example `joe`).

## Fonts and logo

Captions and cards use the system sans-serif (Segoe UI on Windows). The logo is `website/public/brand/logo-1024.png`.
