# video/

Storyboards for `tools\cli film-make` (#395, #397). Rendered output goes to `agent-box/reports/film/` and is git-ignored.

```
tools\cli film-make --setup                      # once: Piper + a voice into tools/film/.tts (git-ignored)
tools\cli film-make video/tooling-test.json      # -> agent-box/reports/film/film-post-tooling-test.mp4 + .srt
tools\cli film-make video/x.json --no-vo --keep  # skip voice-over; keep tmp/film-make-<name>/ (graph, ass, cards)
```

A storyboard names clips, optional title and end cards, a music file, and per-clip cue files. Field names, and which one
a bad storyboard blames, are in `tools/film/post.mjs` (`validateBoard`).

- **Captions:** a cue file, one line per cue: `from to text` (seconds from the start of the take; `#` is a comment). Or inline `[[from, to, "text"]]`.
- **Voice-over:** `at text` lines, or inline `[[at, "text"]]`. Each line is spoken offline by Piper and placed at that time in the take.
- **Cards:** `titleCard` and `endCard` with `lines` (first line big, the rest smaller) and `seconds`. The logo is added to both.
- **Music:** `music.file` under `video/music/` (git-ignored; credit it in `docs/credits.md` first), `gainDb`, `fadeIn`, `fadeOut`. It ducks under game sound and voice.
- **Output:** 1080p, 30 fps, H.264 + AAC, mixed to about -14 LUFS with the peaks limited; an `.srt` beside the mp4.
- **Footage paths** in a storyboard are repo-relative; the clips under `agent-box/reports/film/` are git-ignored, so a storyboard names the takes it needs.
- **Not yet:** the 4K and vertical (9:16) presets of #397.

## Storyboards that shoot (lf-storyboard/1, #392, #395)

A storyboard with `"format": "lf-storyboard/1"` and a `shots` list is shot first, then post-produced. The schema is
`storyboard.schema.json`; `tools/film/storyboard.mjs` holds the checks, and a bad file fails naming the field
(`storyboard shots[2].camera.radius: must be a number, 1 or more`). `tools\cli validate` checks every file here.

```
node agent-box/session.mjs up --clients 1 --mission playtest --profile full --res 1920x1080 --wait
node agent-box/harness.mjs --client 1 flow ui-intake          # spawn the character
tools\cli film-make video/camera-test.json --no-vo            # five camera modes, one scene (internal test)
tools\cli film-make video/trailer-571.json --group spawn-square --client 1 --preview --no-vo
node agent-box/session.mjs down
```

- **Camera modes:** `firstPerson` (the client's own view; `client`), `follow` (`point` near a vehicle, `distance`), `orbit` (`point`, `radius`, `azimuth` 0 = +z side, 90 = +x, 180 = -z, 270 = -x, `arc`, `height`), `static` (`position`, `lookAt`), `drone` (`keys`: from and to, eased), `still` (a gallery still, not shot here). A move lasts the shot duration.
- **Clean frame:** HUD and hint pop-ups are off for every take unless the shot says `"hud": "on"`.
- **Scenes:** `scene` is a preset name (`street`, or a light-only `trailer-*` preset: clock, clear sky, no actors). `scenes[]` overrides hour and weather for the film.
- **`pending`:** a camera with `"pending": "why"` is listed and skipped, never guessed (prose placement, actor setup not written).
- **Flags:** `--shot S01,S02` re-shoots those shots, `--group <name>` one location group, `--preview` half size and 10 fps without voice or music, `--client n` films every shot on one client, `--no-shoot` posts the takes already on disk, `--shoot-only` stops after the takes. Takes land in `agent-box/reports/film/<name>/<id>.mp4`; the frames are deleted after encoding. Put the storyboard path first.
- **Title shots** (`"kind": "title"`, `lines`): a first shot is the title card, a last one the end card.
- A cut with no sound at all (no `--sound`, no music, no voice) is left silent.
