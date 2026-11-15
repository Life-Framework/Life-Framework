# First Workshop upload: maintainer runbook (#370, #371)

About 15 minutes of your time, plus Workbench's bundling and one boot check. Only you can do it: the
upload needs your Bohemia Account. Agents never press Publish, run `promote`, or push tags.
Background and the field values: [release-process.md](release-process.md),
[playtest/workshop-publish.md](playtest/workshop-publish.md), the kits in
[marketing/workshop/](marketing/workshop/README.md).

| Item | Workshop ID (its `.gproj` GUID) | Kit |
|---|---|---|
| Core, `LifeFramework` | `4550A2B656E1B569` | `docs/marketing/workshop/` |
| Pack, `LF_Everon1989` | `B1E84D5687E11724` | `docs/marketing/workshop/everon-1989/` |

The pack depends on the core, so the core goes first. Both are APL (decision D2); the repo stays MIT.

## 0. Once

1. Arma Reforger Tools, the **stable** Workbench. **Workbench > Link**: sign in with the Bohemia Account
   that will own both items. Read the Workshop Terms of Service once.
2. The checks below ran green on 2026-11-14 on `main` (`release 0.1.0` dry run, `release-ready --strict`,
   `example-zip` draft). Re-run them in step 1; do not skip it, `main` moves.
3. The first publish waits for the whole pack move (decision D3 in [roadmap/m3-plan.md](roadmap/m3-plan.md)):
   check that batches 1 to 4 of #323 are all merged before you press Publish. Until then a release is
   tagged and promoted, not uploaded ([release-process.md](release-process.md), "The first Workshop publish").

## 1. Prepare (a worktree, never the main checkout)

```
tools\cli wt new first-upload --wait
cd ..\Life-Framework-ws-first-upload
tools\cli release 0.1.0            # dry run: plan, refusals, release-ready --strict. Nothing changes.
tools\cli wt gate first-upload     # validate --full, build, full suite. Publish only from a green commit.
tools\cli pack                     # the core, packed the way Publish bundles it; "pack OK ... data.pak"
tools\cli pack LF_Everon1989      # the pack, same check
git status                         # must be clean
```

The field values, page text, pictures and gap list for the two Publish dialogs are on one page:
[release/workshop-upload-0.1.0.md](release/workshop-upload-0.1.0.md).

The version is `0.1.0` for both items (the maintainer's choice). The core item already exists on the
Workshop (the boot check found its ID on 2026-11-14). If the Publish dialog shows a version at or above
`0.1.0` for it, stop and ask: every publish takes a higher number, even a text-only change. The pack is
new. The scenario in the server config is `LF_Everon1989`
(`{C638C03B775C31BB}Missions/LF_Everon1989.conf`).

`tools\cli release 0.1.0 --confirm` (stamps both `.gproj` and the kit READMEs, runs every gate, tags
`v0.1.0` locally) is optional for this first upload; if you use it, upload from the tagged commit.
The release's upgrade page is [upgrades/0.1.0.md](upgrades/0.1.0.md).

## 2. Publish the core

1. Workbench launcher > **Add Existing** > `addons\LifeFramework\LifeFramework.gproj` **from the
   worktree**. Wait for the Resource Browser. Script Editor: **Build > Compile and Reload Scripts**, no errors.
2. **Workbench > Publish Project**. Fill it from `docs/marketing/workshop/README.md`:
   Project Name `Life Framework (Core)`; Working Dir default; Preview Image `docs/marketing/workshop/preview.jpg`;
   Screenshots the nine files in `docs/marketing/workshop/screenshots/`; Category Game Mode (+ Scenario);
   Tags `Roleplay Life RPG Economy Jobs Police Crime Multiplayer Framework`; License **Arma Public
   License (APL)**; Version (step 1); Visibility **Unlisted** for the first round; Summary `summary.txt`;
   Description `description.bbcode` (page text names APL, links the repo and the install guide, lists the
   Workshop dependencies from `docs/ecosystem.md`); Change Notes from `changelog.md`.
3. **Publish**, confirm the size, wait for the upload. Copy the page address
   (`https://reforger.armaplatform.com/workshop/4550A2B656E1B569`) and open it once.

## 3. Publish the pack

1. Same worktree: **Add Existing** > `addons\LF_Everon1989\LF_Everon1989.gproj`. Compile with no errors.
   Workbench lists the core as a dependency; it must resolve to the item from step 2.
2. **Publish Project** with `docs/marketing/workshop/everon-1989/README.md`: Project Name
   `Life Framework: Everon 1989`; Preview Image `everon-1989/preview.jpg`; License **APL**; Version `0.1.0`;
   Visibility **Unlisted**; Summary `summary.txt`; Description `description.bbcode`.
3. **Publish**, then copy the page address (`.../workshop/B1E84D5687E11724`).

## 4. Prove a stranger can boot it (#371)

The check boots a **clean** dedicated server on lf-cloud: its own empty profile and addon folder, no
`-addons`, no local copy of either addon. The server has to download both items. Then it starts a game
client with no local addons and waits for the join.

```
tools\cli workshop boot-check 4550A2B656E1B569 B1E84D5687E11724
```

It prints one line: `SUMMARY workshop-boot-check ok stage=pass ...`, or the stage that failed and the log
line that decided it. The full log is `tmp\boot-check\console-<stamp>.log`.

| Stage | Meaning when it fails |
|---|---|
| `ids` | an id is not 16 hex digits (a placeholder) |
| `clean` | the box's server install holds a LifeFramework addon, or UDP 2399 is busy |
| `mods` | the server could not get a mod: `Addon <id> - Addon was not found on workshop.` Wrong id, or the item is still private |
| `scripts` | a `SCRIPT (E)` or VME line in the boot log |
| `ready` | no `Entered online game state.` within the budget (`--budget <s>`, default 420) |
| `join` | the client never produced a player line on the server |

`--no-client` skips the join (server only). `--plan` prints what it would do. The client join needs a
signed-in desktop on the box; if it fails at `join` with a ready server, join by hand from a normal game
(Workshop, subscribe, server browser) and keep the server log.

## 5. Record it

1. `docs/setup.md`: both IDs, the page URLs, and the boot-check command and its passing line (#371).
2. `tools/release/example-zip.json`: replace the pack's `PLACEHOLDER-...` with `B1E84D5687E11724`, then
   `tools\cli example-zip --final` builds the release zip (`server/example/server.json` already lists both IDs).
3. Issues: #370 gets both IDs and page URLs; #371 gets the passing SUMMARY line and the log path.
4. After a public publish: the [after-publishing list](playtest/workshop-publish.md#after-publishing).

## What only you can do

- The Bohemia Account link and the Publish clicks (steps 2 and 3).
- A real preview image and gallery screenshots for the pack: the shipped `everon-1989/preview.jpg` is a
  flat DRAFT placeholder (see the kit README's spec), and the pack kit has no tags, category or change
  notes yet. Suggested: tags as the core minus `Jobs Police Crime`, Category Game Mode or Scenario
  (whichever the dialog offers), change note "First release: the 1989 Everon pack for Life Framework".
- Deciding Unlisted vs Public (public only with the showcase video, #390).
- Whether the Workshop accepts a lower version number (rollback; record it in #367).
