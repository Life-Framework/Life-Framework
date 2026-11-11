# Trailer quality gate

The real trailer (#571) is cut last, after the M2 polish fixes and the visual QA sweep (#679), and is posted only after it
passes this gate. `video/tooling-test.json` is a tooling test and is never posted. Its footage still shows the FPS
counter and the Steam version text in the corners and the website watermark; this gate is what catches those.

0. **Before filming: placement.** From a worktree merged with `origin/main`, run
   `tools\cli audit placement --strict` (about 10 s on the test pool, no session; `docs/placement-audit.md`).
   It must report nothing flagged, or each remaining line must be in `agent-box/place-audit-allow.json`
   with a reason. It needs the Reforger dedicated server, so CI cannot run it; this step is the gate.
   Then shoot every area you will film with `tools\cli shots area <name> --eye-only --annotate` and look
   at each contact sheet for what numbers miss: junk strewn about, the wrong asset, odd scale.
1. **Frames.** Extract one frame every 0.5 s
   (`ffmpeg -i cut.mp4 -vf fps=2 frames/f%04d.png`). A second agent, not the one who cut it, reviews every frame for:
   - floating or clipping props;
   - white or placeholder meshes, missing textures;
   - debug text, FPS counters, version strings, HUD, harness overlays, toasts and hints;
   - frame drops or stutter (compare adjacent frames; a repeated frame is a drop);
   - an unsteady camera;
   - captions that are hard to read (contrast, length, time on screen, covered by the picture).
2. **Audio.** Integrated loudness about -14 LUFS (`ffmpeg -i cut.mp4 -af ebur128=peak=true -f null -`), true peak under
   -1 dBFS, no clipping; voice-over audible over the music; the music track and the voice are in `docs/credits.md` and the video description.
3. **Format.** 1080p at 30 fps or better, H.264 + AAC, captions burned in and an `.srt` beside it.
4. **Honesty.** Captions say only what the footage shows and the game does, in the plain tone of `docs/community/discord.md`.
5. **Sign-off.** The maintainer watches the final file and signs off before it is posted.
