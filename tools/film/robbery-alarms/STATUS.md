# Robbery alarms video: status (parked 2026-11-14)

Parked on the user's decision; it will be revisited. Nothing has been posted.

## What exists

- The kit in this folder: `shoot.mjs` (session setup, shot list, camera paths, golden hour 17.75), `costume.mjs`,
  `edit.json` and `cut.mjs` (cut, loudness, encode, contact sheet, review frames, `--measure` for anchors),
  `checklist.md` (the reviewer's rules) and `README.md` (one command to rebuild).
- Fixes the shoot needed, on this branch: the clerk dialogue voices (silent before), the film audio pid (recorded the
  wrong client), slate offsets masked by the alarm, the eased vault door swing, the `robbery-police` film scene with a
  dressed driver, and the `itemwear`/`itemstrip` harness verbs.
- The fuel kiosk world fix (one register, alarm beacon on a wall box) shipped separately in #1060.

## Takes and cuts (not in git)

- Raw takes: `tmp/robbery-video/takes/` in the `Life-Framework-ws-robbery-video` worktree. The cut uses `EST`,
  `WINDOW`, `CLERK`, `FRONT`, `SIDE`, `BEACON`, `BANK` and `POLICE`. `CLERK` is a copy of `CLERK_5`, `BANK` of `BANK_7`
  and `EST` of `EST_2`. The rest (`INSIDE`, `ROB*`, `HOLD*`, `*_2..._6`) are spares or rejects.
- Rough cut v1 (36.8 s), what the user saw: `robbery-alarms-roughcut-v1.mp4`, in the coordinator's scratchpad
  `robbery-video/` folder.
- Last cut (33.1 s, 9.4 MB, 1080p30, -14.5 LUFS): `robbery-alarms.mp4` in the same folder. It already has nobody at
  the vault, one clerk shot and a 0.5 s black gap.

## What is wrong (the reviewers and the user)

Open:
- **No walk-in.** The harness can't walk a player smoothly or raise a weapon to aim. The robber is already at the
  window, pistol held low; reviewers read it as "looking at the gun", not a hold-up.
- **The vault door fills the frame.** The south-west angle hides the door's flat grey back face, but the door swings
  toward the lens and covers about 60% of the frame. The open vault is never seen. Grey slabs, a crate and a blue card
  lie on the grass near it. The vault sits in the bank's outer wall.
- **The clerk is static.** Calm idle pose; only the audio carries his panic.
- **The police driver** wears a dark suit and cap (there is no police uniform). The shot ends before he is clear,
  but he is still visible in the last frames. A second, parked police UAZ sits in frame.
- **Frame stalls:** the engine capture freezes for up to a second when the PC is short of RAM. Cuts were placed around
  the stalls; a re-shoot on a quiet PC would help.

Addressed in the last cut (not yet seen by the user):
- **The robber at the vault.** He now sets it off and leaves before the door turns, and client 2's body no longer
  parks by the garage.
- **The black gap before the end card** is 0.5 s (it was 0.6 s).

To resume: `README.md`. A session must run on this PC (`--machine local`), because the frame and audio capture read
local processes.
