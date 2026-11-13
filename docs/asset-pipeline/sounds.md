# Sounds for an asset

`tools\cli asset sound <slug>` gives an asset its sound effects: generated with the ElevenLabs
sound-effects API, cut and levelled to the format the engine plays, and written into the addon as
samples and sound projects. It runs after the asset's `prefab` stage and has its own file,
`tools/assets/<slug>/sounds.json`, which is both the work order and the credit record. What the
first run (the taser) taught us is in
[`docs/architecture/asset-sound-spike.md`](../architecture/asset-sound-spike.md).

```
tools\cli asset sound <slug> --init                      sounds.json from the kind's event list
tools\cli asset sound <slug>                             generate, process, and wire when every sound has a pick
tools\cli asset sound <slug> --only generate|process|wire
tools\cli asset sound <slug> --again <sound id>          one more take of one sound
tools\cli asset sound <slug> --status | --settle         the record | read the account's credit counter into it
```

| Step | Does | Who judges |
|---|---|---|
| `generate` | One API call per take, `variations` takes per sound. Each call is a row in `sounds.json` (prompt, duration, influence, date, format). The raw takes go to `tmp/assets/<slug>/sound/raw/` | Nobody yet |
| `process` | ffmpeg decodes to 48 kHz mono. The tool then normalises the peak to -3 dBFS, cuts the silence, fades a one-shot in and out, folds a loop's end into its start, and lays a `layer` over the start. Writes `sound/wav/<id>_<take>.wav` and a picture per take in `sound/review/` | The tool checks format, level, silence and the loop seam. **The agent reads the pictures** (below) |
| `wire` | Copies the picked takes to `<project_dir>/Samples/` with `.meta` files, writes one `.acp` per `project` with its `.meta`, records the references under `wired` | The tool; `asset-sound-selftest` rechecks the committed files on every `validate` |

Nothing is rebuilt in Workbench: the engine plays a plain `.wav` beside a `WAVResourceClass`
`.meta`, and reads the `.acp` as text. `tools\cli test` refreshes the resource database by itself.

## What the engine wants

Read from the game (1.9) on 2026-11-13, not guessed:

- Vanilla weapon samples (`Sounds/Weapons/Handguns/M9/Samples/...`) are RIFF PCM, 16 bit, **mono,
  48 kHz**. A positioned sound is mono; the project places it.
- Vanilla one-shots peak between about -3 and -5 dBFS and are trimmed in the project, by the bank's
  `Volume` (-12 to -24 dB for the M9's handling) and by the amplitude node's distance curve.
- A weapon plays the events its animation and muzzle fire, by name, from the projects listed in
  its `WeaponSoundComponent` `Filenames`: `SOUND_SHOT`, `SOUND_DRY`, `SOUND_RELOAD_MAG_REMOVE`,
  `SOUND_RELOAD_MAG_INSERT`, and for a pistol with a slide `SOUND_RELOAD_BOLT_PULL`,
  `SOUND_RELOAD_BOLT_RELEASE`, `SOUND_RELOAD_MAG_PUSH`. An event no listed project has is silent.
- The project the tool writes is the vanilla M9 handling project's graph, one chain per event:
  Sound, Shader, Bank, with a vanilla amplitude config per `range`, the vanilla frequency and
  spatiality nodes, the vanilla final mix and its three reverb sends.

## The sound list (the playbook step)

`--init` writes the events of the asset's kind (`SOUND_KINDS` in `sound.mjs`). Edit the list: drop
what the item does not do, add what it does.

| Kind | Events | Played by |
|---|---|---|
| `weapon` | `SOUND_SHOT` (project `Shot`), `SOUND_DRY`, `SOUND_RELOAD_MAG_REMOVE`, `SOUND_RELOAD_MAG_INSERT` (project `Handling`) | The engine, from the vanilla pistol's animation events. Leave the slide events out for a weapon with no slide |
| `gadget` | `SOUND_LF_USE` | Your script |
| `deployable` | `SOUND_LF_DEPLOY`, `SOUND_LF_PICKUP` | Your script |

A sound your script plays is named `SOUND_LF_<WHAT>`; a loop ends in `_LP` and sets `"loop": true`.

Per sound:

| Field | Meaning |
|---|---|
| `event`, `project` | The event name, and which `.acp` it goes in (`<project_prefix>_<project>.acp`) |
| `bus` | Mixer input: `shots`, `handling`, or `sfx` for anything a script plays |
| `range` | Distance curve: `near22` (a click), `reload60` (handling), `voice70` (a taser, a shout), `far170`. The number is the range in metres |
| `volume_db` | The bank's trim. Start from the kind's default and compare with the vanilla sound it replaces |
| `prompt`, `duration_seconds`, `prompt_influence`, `variations` | The generation |
| `loop` | A seamless loop: asks the API for one, and the tool crossfades the end into the start |
| `trim_db` | Silence threshold, -45 by default. Raise it (-30) when a take ends in a faint tone |
| `layer` | `{ "id", "take", "gain_db", "delay_ms" }`: lay a take of a `layer_only` sound over the start |
| `pick`, `reason` | The takes that ship, and why. `wire` refuses to run without both |

## Prompts

- Say what makes the sound, how close, and what is not there: "Recorded close, dry, no reverb, no
  music, no voice." The game adds the room.
- Name the period and rule out the wrong genre: "1980s", "no science-fiction laser or synthesizer".
- **One event per generation.** "A pop, then a second of crackle" came back as crackle five times
  out of five. Generate the pop as its own `layer_only` sound and join them with `layer`.
- The API takes 0.5 to 30 s. Ask for a little more than the sound; the trim cuts the rest.
- `prompt_influence` 0.6 to 0.8. Higher follows the words more closely and varies less.
- Two or three takes per sound. If none fits, change the prompt, not the count.

## Review: reading a sound you cannot hear

`process` prints one line per take and writes `sound/review/<id>_<take>.png`: the waveform above,
the spectrogram to 16 kHz below, time left to right. Read every picture.

- **Order and timing.** Is the loud part where the event starts? `peak at` over about 100 ms on a
  handling sound means it trails the hand in the animation.
- **One event or several.** A dry-fire click is one burst. Two bursts with a gap are two clicks.
- **Broadband or tonal.** Sparks, clicks and scrapes are vertical stripes from top to bottom. A
  horizontal line is a pitched tone, and swept lines are a synthetic "zap": reject them for a
  mechanical or electrical sound.
- **Evenness, for a loop.** The same density from left to right. A gap or a swell repeats every loop.
- **The tail.** A faint horizontal line that carries on after the sound is a generator artefact: set
  `trim_db`.
- **Variety.** Several picks are chosen at random per play. Use that for handling. A weapon's shot
  gets one take: two different shots read as two different weapons.

Write the reason in `sounds.json` in those terms. A maintainer still has to listen: the PR's
manual steps name the files.

## Wiring

- **Weapon:** in the weapon prefab, override `WeaponSoundComponent` by the inherited GUID and list
  the new projects in `Filenames` in place of the vanilla shot and handling projects. Keep the
  shared melee, pick-up and drop projects. `Handgun_Taser.et` is the example.
- **A sound a script plays** (a deploy, a loop on a body): `SCR_SoundManagerModule.CreateAudioSource`
  with the project and the event, as `LF_StunBuzz` and `LF_SiteAlarmComponent` do. It returns null
  when the listener is out of range. Start and stop it on clients from replicated state; a
  dedicated server has no listener.
- Add a test that loads the projects and reads the prefab's `Filenames` (`LF_Test_TaserSound`).

## Proof

- `tools\cli validate` runs `asset-sound-selftest`: the format of every committed sample, and
  every committed project against what the generator writes from `sounds.json`.
- In a session with sound (`session up --profile full --sound`), the harness verb `sound [EVENT]`
  reports, for the weapon in hand, which events its projects know, plays the named one, and reports
  the stun buzz. `session.mjs taser-sound-proof` is the example; it records what the client plays to
  `<session>/taser-sound-client1.wav`. A client that has just respawned plays muffled for some
  seconds, so judge levels on a client that has not.
- `--sound` plays through this PC's default output. Keep such a session short.

## Credits

- The key is read at call time from `~/.life-framework/elevenlabs-key` (`LF_ELEVENLABS_KEY_FILE`
  overrides). It is never printed, logged, or put in an error; the selftest proves the last.
- `budget.generations` caps the asset (20 by default, 30 at most). `generate` refuses a run that
  would cross it, before any call.
- A take that exists is never generated again. `--again <id>` adds one take.
- **Cost:** short sounds cost about 200 credits each, whatever the docs' per-second rate says. The
  account's counter runs minutes behind, so read it with `--settle` when you are done and report
  that number.
- The licence terms are in `docs/credits.md` and `docs/decisions.md` (2026-11-13). Samples ship
  inside the addon; never publish them as a sound pack.
