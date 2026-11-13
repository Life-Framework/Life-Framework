# Dialogue stage: a voiced NPC conversation

`tools\cli asset dialogue <npc>` turns one data file into a conversation the game can play:
recorded lines, the audio project, the dialogue config and the caption strings. What the scene
does and why: [`docs/architecture/npc-dialogue.md`](../architecture/npc-dialogue.md).

```
tools\cli asset dialogue <npc> --dry-run          what would be generated and what it costs; no network
tools\cli asset dialogue <npc>                    generate what changed, write everything
tools\cli asset dialogue <npc> --force <line>     a new take of one line (paid)
tools\cli asset dialogue <npc> --force all        a new take of every line (paid)
tools\cli asset dialogue status                   every NPC: voice, characters spent, stale lines
```

Run it in a worktree. Speech is generated here and committed. The game never calls ElevenLabs.

## What it does

| Step | Does |
|---|---|
| check | Refuses a dialogue that would strand the player or waste characters: a reply to a missing node, a node nothing reaches, more than four replies, a node whose every reply is conditional, markup in a spoken line, a line nobody says |
| generate | ElevenLabs text-to-speech, one request per changed line. Logs the account balance before and after |
| process | ffmpeg: trim silence, normalise to -20 LUFS, mono 44.1 kHz 16-bit wav |
| import | Writes `Sounds/Dialogue/<npc>/<npc>_<line>.wav` with its `.meta`, and `Dialogue_<npc>.acp` with one sound event per line. The next `test` or `session up` registers them |
| lip timing | Measures each wav's loudness every 50 ms and stores it as the line's mouth envelope |
| config | Writes `Configs/Dialogue/LF_Dialogue_<Name>.conf` |
| strings | Appends `LF-Dialogue_<Name>_<Line>` and `LF-Dialogue_<Name>_Say<Reply>` to the string table, or updates their English text. Then run `tools\cli regen-localization` |

## Cached and resumable

- A line is bought again only when its text, voice, model or voice settings change. The hash is
  kept in `tools/assets/dialogue/<npc>/elevenlabs.json`, which is committed, so a fresh worktree
  pays nothing for lines already in the addon.
- Raw downloads are kept in `tmp/assets/dialogue/<npc>/` by hash. A run that died after paying
  picks the download up again.
- The provenance file is saved after every line.
- `budget_chars` caps what one NPC may ever cost (at most 6,000). A run that would cross it stops
  before any call. When that happens the lines are too long; do not just raise the number.
- `tools\cli validate` fails when a `dialogue.json` and the committed files disagree
  (`dialogue-pipeline-selftest.mjs`). The fix it prints is to run the stage.

## A new NPC

1. **Write `tools/assets/dialogue/<npc>/dialogue.json`.** Copy the clerk's.

   ```json
   {
     "id": "baker", "name": "Baker", "speaker_key": "#LF-Shop_Baker_Name",
     "voice": { "id": "...", "name": "...", "public_owner_id": "...", "model": "eleven_multilingual_v2" },
     "budget_chars": 2000, "hear_range_m": 12, "start": "greet",
     "nodes": [
       { "id": "greet", "line": "greet", "options": [
         { "text": "buy", "next": "after", "action": "ShopOpen" },
         { "text": "bye", "next": "bye" } ] },
       { "id": "after", "options_from": "greet" },
       { "id": "bye", "line": "bye" }
     ],
     "lines": { "greet": { "text": "Good morning. Fresh bread today." }, "bye": { "text": "Good day." } },
     "replies": { "buy": "A loaf, please.", "bye": "Goodbye." }
   }
   ```

   - A node says its `line` (or nothing) and offers up to four `options`. A node with no options
     ends the talk after its line. `options_from` copies another node's options.
   - An option has a reply `text`, the `next` node, and optionally a `condition`, an `action` and
     a `param` handed to both.
   - A line with `for_key` is spoken whenever an action hands that string key back. This is how
     an existing feature's toast becomes speech without the feature knowing about dialogue.
   - Write for the ear: short sentences, no brackets, numbers as words. 1989 Everon: an official
     is formal, a shopkeeper is brisk, nobody says "okay".
2. **Pick a voice** (below) and run `--dry-run`, then the stage. Read the character count first.
3. **Hooks.** For each `condition` or `action` name, write `class LF_DialogueHook_<Name> :
   LF_DialogueHook` in the feature that owns the work (`Feature/Quests/LF_QuestDialogue.c` is the
   model). `Allows` answers a condition. `Run` does the work on the server and must check
   everything itself; it may add string keys to `spoken` for the NPC to say. Reuse the feature's
   existing server entry point, as `QuestConverse` reuses `RequestConverse`.
4. **Put `LF_DialogueComponent` on the NPC prefab** with `m_sDialogue` set to the generated conf,
   and make the NPC's talk action call `LF_DialogueScene.Begin(npc)` on the client when
   `LF_DialogueComponent.ConfigOf(npc)` is not null (`LF_QuestAction.PerformAction` is the model;
   the action must be local-effect-only). Keep the old path as the fallback.
5. **Test.** A WORLD test like `LF_Test_DialogueClerk`: the config loads, every hook exists, the
   action does what the old interaction did. Red-proof it.
6. **Look and listen.** `session up --profile full --clients 2 --sound`, then drive it as
   `agent-box/proofs/dialogue-proof.mjs` does. `node agent-box/harness.mjs dialogue` prints the
   scene's state; `dialogue pick 0`, `dialogue skip`, `dialogue face 1`, `dialogue mouth 1` drive
   it. An agent cannot hear: list the wav paths in the PR for the maintainer.
7. **Classify.** The dialogue file, conf and sounds of a town NPC are pack content: add the rows
   to `docs/architecture/core-pack-inventory.md`. Credit the voice in `docs/credits.md`.

## Voices

- `GET /v1/voices` lists the account's voices; the premade ones are American and British.
- The voice library has accented English: `GET /v1/shared-voices?language=en&accent=czech` (also
  `french`, `search=slavic`). Put the voice's `voice_id` and `public_owner_id` in the spec and the
  stage adds it to the account on first use. A library voice has a notice period; lines already
  generated stay usable after it is withdrawn, new ones cannot be made.
- One voice per NPC. Say why you picked it in the spec's `voice.why`.
- `settings` are ElevenLabs voice settings. The clerk uses stability 0.55, similarity 0.75,
  style 0.15. Changing them regenerates every line.

## Key and cost

- The key is read at call time from `~/.life-framework/elevenlabs-key` (`LF_ELEVENLABS_KEY_FILE`
  overrides). It is never printed or put in an error; the selftest proves it.
- Every paid call is appended to `tmp/assets/dialogue/<npc>/chars.jsonl`, and each line's running
  total is in `elevenlabs.json` (`chars_total`).
- The clerk: 10 lines, 1,165 characters of text. The account is shared with the sound stage.

## What the stage does not do

- It does not judge the performance. Listen; `--force <line>` buys another take.
- It does not write hooks, put the component on the prefab, or write tests.
- It does not translate. Captions go through the string table; the voice is English.
