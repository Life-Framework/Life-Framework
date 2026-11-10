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
