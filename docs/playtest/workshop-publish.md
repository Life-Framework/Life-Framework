# Publish a release to the Workshop and the 0grind server

One page, top to bottom, for the maintainer (or the lead under its sign-off authority). It is written for
**0.3.0 on Sunday 2026-11-29** and works for any later update: change the version. Checked against the repo and the
live read-only commands on 2026-11-28.

Only the **Publish** step uploads, and only the maintainer's saved Bohemia login can do it. Every field value for a
by-hand publish is in [`docs/marketing/workshop/`](../marketing/workshop/README.md). Decision log for the flow:
[`docs/release/chapter-release.md`](../release/chapter-release.md) (the command), [`docs/ops/game-updates.md`](../ops/game-updates.md)
(saves across a game update), [`docs/owner/host-the-flagship.md`](../owner/host-the-flagship.md) "0grind" (the server).

## What is true today (2026-11-28)

| Thing | State |
|---|---|
| Items | Core `LifeFramework` `4550A2B656E1B569` and pack `LF_Everon1989` `B1E84D5687E11724`. Both exist; **both pages show 0.2.0**. Every release is an update, never a first publish. |
| Game | Client and server on this PC: **1.9.0.42** (builds 26639613 and 26634883), `tools\cli game-update --check` agrees. The 0grind box already logged `version 1.9.0.42` at its 06:02 UTC restart on 2026-11-28, so it is probably on the current build (step 3 confirms; the box's own update is a panel action, `game-update` never touches it). |
| Branch | `release-0.3.0`, cut at `0f6b0f1eb` (merge of #5186); its head is `443d21451` (adds the release smoke doc, verdict ship). `main` is already 15 commits ahead with next-release work, so **publish from the `release-0.3.0` head, never from the tip of `main`**. |
| Server | One 0grind service `298627`, `144.76.90.77`, game port 2023, A2S 3023, name "Life Framework \| Everon 1989 (Alpha)", 0 players right now, restarts 06:00 and 18:00 UTC. It is both the live alpha and the playtest server. |
| Versions | Latest tag `v0.1.2`; Workshop 0.2.0; `CORE_VERSION` in the repo `0.2.0`. One number goes into both items. |

## Where to run it

On the PC that has **both** the saved Workbench login (`Documents\My Games\ArmaReforgerWorkbench`) and
`%LOCALAPPDATA%\LifeFramework\host-0grind\secrets.json` (join and admin passwords and the Discord
`announceWebhookUrl`), plus the 0grind API token in `%USERPROFILE%\.life-framework\0grind-token`. From a worktree
(`tools\cli wt new publish-030`), never the main checkout, in the foreground. **On a PC without that `secrets.json`
(PC1 had none on 2026-11-28) a plain `config push --yes` blanks both passwords:** it shows
`password: (set, masked) -> (empty)` in the dry run. Run from the right PC, or pass `--only` (step 5).

Close Workbench first if you can: the headless publish shares its default profile.

## 1. Read-only checks (2 minutes, change nothing)

```
tools\cli status
tools\cli game-update --check
tools\cli host remote 0grind status
tools\cli host remote 0grind config push
```

You want: `game: client 1.9.0.42 and server 1.9.0.42 in step`; the 0grind line `status="running" ... players=0` and
`a2s: 144.76.90.77:3023 up`, no `WARNING` about System settings (`host remote 0grind system fix` shows the fix as a
diff); the `config push` dry run (it writes nothing) lists only what you mean to change and **no `password` or
`admin_password` line going to `(empty)`**. A `401 Unauthorized` is the API token or this PC's IPv4 missing from
0grind's allowed list ([one-time API access](../owner/host-the-flagship.md#one-time-api-access)).

## 2. Unblock the pack, once (the one code change; a PR on `release-0.3.0`)

**This is a blocker found 2026-11-28.** `LF_PackManifest.conf` on `release-0.3.0` (and on `main`) says
`m_sCoreMin "0.2.0"`, `m_sCoreBelow "0.3.0"`: the pack **refuses a 0.3.0 core**, and
`tools\cli release chapter 0.3.0 --dry-run --at <release head>` fails at preflight with exactly that
(`... accepts core 0.2.0 to below 0.3.0; 0.3.0 is outside it`). A published 0.3.0 pair would load as a core with no pack.

1. In a worktree `tools\cli wt new pack-range-030 --base release-0.3.0`, set `m_sCoreBelow "0.4.0"` in
   `addons/LF_Everon1989/Configs/LF_PackManifest.conf` (min stays `0.2.0`), and `CORE_VERSION = "0.3.0"` in
   `addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c` (path A stamps it itself; path B does not,
   and verify needs the build to say 0.3.0). Ship it with `wt ship pack-range-030`; it follows the recorded base.
   `validate-pack-manifest` and `LF_Test_PackManifest` tie `CORE_VERSION` to the range, so run `tools\cli validate`
   and `tools\cli test --no-build --only LF_Test_PackManifest` first; the PR needs a `## Manual testing` section.
2. `tools\cli wt sync-base main --source release-0.3.0` opens the PR that merges `release-0.3.0` into `main`. Wait for it
   to merge: `release chapter` refuses a commit that is not an ancestor of `origin/main`. The `release-0.3.0` head is
   then one parent of that merge, and **that head's sha is the `--at <sha>` below** (`git rev-parse origin/release-0.3.0`).
3. Any other fix from the 19:00 UK community session goes the same way (`--base release-0.3.0`), before step 5.

## 3. The game update on 0grind, with 0 players

The game update comes **first**; a mod update on an older game build, or both at once, is how a server ends up on a
mismatched build. `tools\cli game-update` updates only this PC's own install and never the 0grind box. For the box:

1. Pull the saves and check who is on:

   ```
   tools\cli host remote 0grind saves pull
   tools\cli host remote 0grind status
   ```

   `saves pull` prints the folder it copied to and checks every file's size. `players=0` is the go; with players on, post
   in Discord and wait (there is no in-game message for an unscheduled restart on 0grind).
2. Read the box's build from its newest log (the pulled `console.log` starts with it):

   ```
   tools\cli host remote 0grind logs pull
   ```

   Open the `console.log` it prints and find `Creating game instance(ArmaReforgerScripted), version 1.9.0.42`. If it
   already says **1.9.0.42** (it did at 06:02 UTC on 2026-11-28), the box is current: go to step 4.
3. If it is older, update it in the 0grind panel (app.0grind.io, service `298627`). The repo has no command for this
   and no record of the button's name: use the panel's server-update action (not tested here; confirm what it is
   called), then `host remote 0grind restart --yes`, and read the next log for the new version line **and** for
   `save loaded:` with no `save load damaged` ([game updates](../ops/game-updates.md): the engine writes the build into
   every save, and a mismatch logs a warning that is expected once). `host preflight` is for a host on a PC, not for 0grind;
   your offsite copy from step 3.1 is the rollback.
4. Wipe or keep the saves? A 0.2.x save on 0.3.0 is covered by the suite's `OnAbsent` tests but **no real 0.2.1 save was
   loaded on 0.3.0** ([RC doc](../releases/0.3.0-rc.md) "Manual testing"). The default is keep. A wipe needs its 24 hour
   notice first ([wipe policy](../community/wipe-policy.md)); the repo has no remote wipe command, only the panel.

## 4. Dry run of the release

Always first. It runs preflight for real (git, `gh`, both Workshop pages), the read-only half of the server step, and
prints the Workbench commands and the exact Discord text:

```
tools\cli release chapter 0.3.0 --dry-run --at <release-0.3.0 head sha> --notes docs/releases/0.3.0-notes.md
```

Without `--notes` the change note is the list of PR titles since the last `v*` tag, and the newest tag is `v0.1.2`
(1,790 merges ago): a useless change note. Read the Discord block the dry run prints (it carries the leading bullet lines of the notes up to
1,900 characters). It must end `SUMMARY release-chapter ok dry-run`. Anything else is a stop; the table at the end says what
each refusal means.

## 5. Publish, restart, verify (path A, recommended: one command)

```
tools\cli release chapter 0.3.0 --at <release-0.3.0 head sha> --notes docs/releases/0.3.0-notes.md
```

On a PC without `secrets.json` add `--only scenarioId,mods,staff,owner` (it pushes just those fields and leaves the
passwords alone; the full field list is in `tools\cli host help`, `remote 0grind`). Add `--no-gproj-stamp` if the pack
step fails on the `.gproj` `Version` line. It runs 30
to 60 minutes: stamp, pack both addons headless, publish the core then the pack, wait for each page to show 0.3.0
(up to 10 minutes), `config push --yes`, `restart --yes` when A2S shows 0 players, wait for A2S and a pulled log that
says `[LFDebug:Packs] core 0.3.0 accepted by pack LF_Everon1989`, `server-doctor` PASS, then **step 6 below runs inside
the command**: it posts to Discord and pushes the tag `v0.3.0`. Each wait has a limit; the command stops at the first
failure with `SUMMARY release-chapter fail step=<n>` and runs nothing after. With players on and no `--now`, it leaves the
update for the 06:00 or 18:00 restart: after that run `tools\cli release chapter 0.3.0 --verify`.

The headless publish keeps each item's **last visibility** and page text. If an item is Unlisted and should be Public, or
the description must change, use path B.

**Path B (by hand in Workbench, when you must change visibility or text).** Open the item's `.gproj` from a clean
worktree at the release head (Workbench launcher > Add Existing: `addons/LifeFramework/LifeFramework.gproj`, then
`addons/LF_Everon1989/LF_Everon1989.gproj`), wait for the Resource Browser, **Build > Compile and Reload Scripts** with
no errors, **Workbench > Publish Project**, **Version** `0.3.0`, **Visibility** as wanted, **Change Notes** pasted from
`docs/releases/0.3.0-notes.md`. The dialog keeps the last publish's name, summary, description, screenshots and tags
(kit values: [`docs/marketing/workshop/`](../marketing/workshop/README.md)); change a field only on purpose. Core first,
then pack. **Never a folder inside the addon as Working Dir.** Confirm the upload (about 55 MB, a minute to bundle). Then
`tools\cli release chapter 0.3.0 --published --at <sha> --notes docs/releases/0.3.0-notes.md --dry-run`, and without
`--dry-run`: it needs both pages to show exactly `0.3.0`, skips build and publish, and runs the server step, verify,
post and tag. `tools\cli pack` first (it prints `pack OK ... data.pak`) catches a mission image that lost its source.

**What to expect on the box.** The mods are on "latest", so the box downloads the new items while it boots after
`restart --yes`. If A2S does not answer within 15 minutes the command fails at step 5: read the newest log
(`host remote 0grind logs pull`, then `server-doctor` on it), and do not restart again until you have.

## 6. Smoke test, then the post

There is no flag to hold the Discord post: step 5 posts the moment the log verifies, and the changelog says "is out on
the Workshop (core and Everon 1989)". So the post and the smoke test run close together. If you want your own smoke test
**before** the post, do path B and the server part by hand and run the command last:

```
tools\cli host remote 0grind config push --yes
tools\cli host remote 0grind restart --yes
tools\cli host remote 0grind logs pull
tools\cli server-doctor <the console.log it prints>
```

then smoke, then `release chapter 0.3.0 --published ...` as above (the box restarts once more, nobody is on).

Smoke test, 10 minutes, in the real game on the Workshop version (Arma Reforger from Steam, **Workshop**, subscribe or
update `Life Framework: Everon 1989`, join the server by name with the join password):

1. The loading screen and the registration form (R-89) appear; spawn at the hostel with the starting cash only.
2. The first steps of the route in [release-smoke-2026-11-28](release-smoke-2026-11-28.md): the clerk and ID card, the
   starter errands (car, apples, juice), the market sale, F2 "What to do next".
3. A second player joins and sees the first.
4. After the run: `tools\cli host remote 0grind logs pull`, `tools\cli server-doctor <log>` PASS, `tools\cli host triage <log>`
   shows no new error group, and the log has `save loaded:` and **no** `save load damaged`.

A fail here is a decision, not a panic: `release-0.3.0` gets the fix (`--base release-0.3.0`), and a new version number goes
out (`0.3.1`; every publish takes a higher number). The tag already exists once posted.

## After publishing

- [ ] **Discord:** the command's post is the changelog. For the longer announcement use
      [`discord-announcement.md`](../marketing/workshop/discord-announcement.md) with the item URLs
      (`https://reforger.armaplatform.com/workshop/4550A2B656E1B569` and `.../B1E84D5687E11724`), and the
      [session post](../ops/session-runbook.md) for the next Saturday 19:00 UK session
      (`tools\cli host announce --at <time> --build 1.9.0.42`, a draft; `--send` is the maintainer's).
- [ ] **Website:** rebuild and deploy on the day (the 0.3.0 gallery is merged; `website/README.md`), add
      [`website-news.md`](../marketing/workshop/website-news.md)'s 0.3.0 entry, and the Workshop links next to the
      zip button (`docs/playtest.md`).
- [ ] **Save copy:** `tools\cli host remote 0grind saves pull` after the first session on 0.3.0. For a standing copy,
      `saves pull --schedule` (today `--schedule-status` says: not scheduled).
- [ ] **Branches:** `tools\cli wt sync-base main --source release-0.3.0` once more if the release branch moved after step 2;
      `tools\cli wt prune release-0-3-0` (the command's worktree), and prune `pack-range-030` and your publish worktree.
- [ ] **RC doc:** tick the Workshop publish and `wt sync-base` boxes in [`0.3.0-rc.md`](../releases/0.3.0-rc.md).
- [ ] Record the community session round in `docs/playtest/` with its date, players and the issues it filed.
- [ ] Issue #109 stays open until the public publish and the showcase video (#390) are done.

## When a step stops

| What you see | Meaning and fix |
|---|---|
| `FAIL ... the pack manifest ... accepts core 0.2.0 to below 0.3.0` | Step 2 not merged into the commit you named. |
| `... is not on origin/main` | The `--at` commit is not an ancestor of `origin/main`; finish `wt sync-base` first. |
| `tag v0.3.0 already exists` or `0.3.0 is not newer than ...` | A half-run: read `git tag -l 'v0.*'` and the Workshop pages before anything else. |
| `gates not green on <sha>: ... no passing "validate" check` | GitHub has no green `validate` on that exact commit. Use the release head's merge commit that has one, or wait. |
| `Publish Addon: Game Identity is not linked with Bohemia Account!` or `Backend authentication timeout!` | Open Workbench once, **Workbench > Link**, sign in with the owner's Bohemia Account, close it, rerun. Never `-wbBackendLogin`. |
| `cannot read the Workshop page` / `does not show 0.3.0 after 600 s` | The page is CDN-cached or the upload failed: open the page, and if it shows 0.3.0 continue by hand (`host remote 0grind config push --yes`, restart, `release chapter 0.3.0 --verify`). |
| `401 Unauthorized` from `host remote 0grind` | API token or this PC's IPv4 is not in 0grind's allowed list. |
| `server-doctor` FAIL or no `accepted by pack` line | Read the log it names; do not post. A core that loaded without its pack is the step 2 range. |
| `save load damaged` in the log | Saving is off for that session on purpose. `host remote 0grind stop --yes`, put the pulled copy back through the panel, read [game updates](../ops/game-updates.md); lead decision. |

## Reference: first publish and the as-player check

The core's first id `79636E668EA37AC9` belongs to an unrelated Workshop item published by someone else on 2026-08-30,
so the core moved to `4550A2B656E1B569` on 2026-11-15. Only a new item needs the rest of this section:

- Arma Reforger Tools, the **stable** Workbench (experimental Tools publish to a separate Workshop); **Workbench > Link**
  with the Bohemia Account; read the Workshop Terms once
  ([Mod Publishing Process](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Publishing_Process)). The build is APL,
  the repo stays MIT.
- A new id must return 404 at `https://reforger.armaplatform.com/workshop/<id>` before it goes in a `.gproj`. The
  Workshop addon folder `addons/LifeFramework/Scripts/WorkbenchGame/EnfusionMCP/` must not exist (run `wb_cleanup`).
- **Publish Project** dialog: Project Name `Life Framework (Core)` or `Life Framework: Everon 1989`, Working Dir the
  default (`%USERPROFILE%\Documents\My Games\ArmaReforger\publish\<addon>`), Preview Image
  `docs/marketing/workshop/preview.jpg`, Screenshots (the pack takes the nine village files in
  `docs/marketing/workshop/screenshots/`, in name order; the core none), Category Game Mode (and Scenario), License APL,
  Summary `summary.txt`, Description `description.bbcode`, Change Notes from `changelog.md`.
- The pack's one listed scenario is `Life Framework: Everon 1989` (`{C638C03B775C31BB}Missions/LF_Everon1989.conf`); the
  developer header `LF_PlaytestTown.conf` ships with `m_bShowInScenarioMenu 0`. If a page shows the Playtest Town, the
  backend ignored that flag: move it out of the pack folder for the publish and file an issue. A scenario name that
  reads `#LF-...` or has no picture means the header text did not reach the backend: fix `Missions/LF_Everon1989.conf`,
  raise the version, publish again.
- Dedicated server from Workshop ids: [`workshop-host.md`](workshop-host.md) with
  [`server-config.example.json`](server-config.example.json); `tools\cli workshop boot-check <coreId> <packId>` boots
  a clean server from the ids.
- The local test before the first publish used `tools\cli pack` (the same `data.pak` Publish bundles) and a test driver
  addon; the details live in the PR that added it. The Scenarios detail pane should show your account or "Life
  Framework Team" as author, not "Bohemia Interactive".
