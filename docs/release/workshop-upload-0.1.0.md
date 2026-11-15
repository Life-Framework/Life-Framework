# Workshop upload pack: 0.1.0 (core and the Everon 1989 pack)

For the maintainer, after the M3 sign-off playtest. You press **Publish** in your own Workbench, signed in
with the Bohemia Account that will own both items; agents never do. This page is the order, the field values
and the checks. The longer background is [release-first-upload.md](../release-first-upload.md) (the
runbook this follows), [playtest/workshop-publish.md](../playtest/workshop-publish.md) (the click-by-click and
the player check) and [marketing/workshop-flagship.md](../marketing/workshop-flagship.md) (the page copy and
the server-browser text). Where those and this page differ, this page is newer (see "Stale kit files" at the end).

| Item | Addon | Workshop ID (the `.gproj` GUID) | Depends on |
|---|---|---|---|
| Core, **Life Framework (Core)** | `LifeFramework` | `79636E668EA37AC9` | the base game |
| Pack, **Life Framework: Everon 1989** | `LF_Everon1989` | `B1E84D5687E11724` | the core (`79636E668EA37AC9`) |

Scenario for the pack and the server config: `{C638C03B775C31BB}Missions/LF_Everon1989.conf`, listed in the
game as "Life Framework: Everon 1989". Server name for the project's own server: `Life Framework | Everon 1989 (Alpha)`.
Both items publish at version `0.1.0`. The licence is the Arma Public License (APL) for both; the repository stays MIT.

## Before you open Workbench

1. **Pick the commit.** Publish from the exact `main` commit you playtested, and write its short SHA down
   (`git rev-parse --short HEAD`). If the playtest checklist's "lands before you play" PR (#1361, heldground, save
   version 34) merged before you played, it is in that commit; if you played without it, publish without it.
   Do not publish a newer `main` than you played.
2. **A worktree, never the main checkout** (the main checkout is your world-editor copy):
   ```
   tools\cli wt new first-upload --wait
   cd ..\Life-Framework-ws-first-upload
   git checkout --detach <the playtested sha>
   ```
3. **Dry run.** `tools\cli release 0.1.0` changes nothing and prints the plan and any refusal. It accepts `main`,
   `stable`, or a detached HEAD exactly on `origin/main` or `origin/stable`. If `main` moved after you played, your
   playtested commit is behind it and the dry run prints `WOULD REFUSE ... release runs from main or stable`: that
   line is only about the real `--confirm` (see Post-upload check 3). A dirty tree or an existing `v0.1.0` tag is a stop.
4. **Gate and pack** (a green commit only):
   ```
   tools\cli wt gate first-upload
   tools\cli pack                    # core: expect "pack OK ... data.pak"
   tools\cli pack LF_Everon1989      # pack: same
   git status                        # clean
   ```
   A `Failed to load metafile of mission config image` line means a mission image lost its source: stop and fix.
5. **The pack move is complete** (the D3 gate in [release-process.md](../release-process.md)): batches 1 to 4 of #323
   are all merged (#1185, #1189, #1203, #1305). Check the issue shows no open batch.
6. Workbench: **Arma Reforger Tools, the stable build** (an experimental Workbench publishes to a separate
   Workshop). **Workbench > Link** is signed in. You have read the Workshop Terms of Service once.
7. `addons/LifeFramework/Scripts/WorkbenchGame/EnfusionMCP/` must not exist in the worktree.

## Workbench steps, in dependency order

### 1. Core first

1. Workbench launcher > **Add Existing** > `addons\LifeFramework\LifeFramework.gproj` **in the worktree**.
   Wait for the Resource Browser to finish.
2. Script Editor: **Build > Compile and Reload Scripts**. No errors. Stop if there are any.
3. **Workbench > Publish Project**. Fill:

   | Field | Value |
   |---|---|
   | Project Name | `Life Framework (Core)` |
   | Working Dir | leave the default (`%USERPROFILE%\Documents\My Games\ArmaReforger\publish\LifeFramework`); never a folder inside the addon |
   | Preview Image | see "Pictures": the shipped `docs/marketing/workshop/preview.jpg` says "PLAYTEST TOWN" and must not go up as is |
   | Screenshots | none (no neutral art yet) |
   | Category | Game Mode |
   | Tags | see "Tags" |
   | License | Arma Public License (APL) |
   | Version | `0.1.0` |
   | Visibility | see "Visibility": `Unlisted` |
   | Summary | the core summary below |
   | Description | the core description below |
   | Change Notes | the core change line below |

   The core item already exists on the Workshop (the boot check found its ID on 2026-11-14). If the dialog already
   shows a version at or above `0.1.0` for it, stop: every publish takes a higher number, even a text-only change.
4. **Publish**, check the upload size, wait for it to finish. Copy the page address
   (`https://reforger.armaplatform.com/workshop/79636E668EA37AC9`) and open it once.

### 2. Then the pack

1. **Add Existing** > `addons\LF_Everon1989\LF_Everon1989.gproj` in the same worktree. Compile with no errors.
   The pack's dependency list shows the core (`79636E668EA37AC9`); it must resolve to the item you just published.
2. **Publish Project**:

   | Field | Value |
   |---|---|
   | Project Name | `Life Framework: Everon 1989` (27 of 30 characters) |
   | Working Dir | the default |
   | Preview Image | see "Pictures" |
   | Screenshots | see "Pictures" (1280 px wide, each under 500 KB) |
   | Category | Game Mode, and Scenario too if the dialog offers it |
   | Tags | see "Tags" |
   | License | Arma Public License (APL) |
   | Version | `0.1.0` |
   | Visibility | `Unlisted` |
   | Summary | the pack summary below |
   | Description | the pack description below |
   | Change Notes | the pack change line below |

3. **Publish**. Copy `https://reforger.armaplatform.com/workshop/B1E84D5687E11724`.
4. Open both pages in a browser: the preview, the screenshots, the description text and its headings (the
   description uses BBCode `[h2]` and lists; if the dialog does not render them, tell the issue so, the text still reads).

## Visibility

**Unlisted** for both items. Decision 2026-11-08: the first release is private rounds, and `Public` comes together
with the showcase video (#390), as a new version. Unlisted items still download by ID and subscription, which is
what the boot check and a server's `game.mods` need; a `Private` item would fail the boot check at the `mods` stage.
Switching to Public later is a new publish with a higher version. If you want Public today, that decision is yours
to reverse; nothing in the tooling depends on it.

## Text to paste

### Core

Title: `Life Framework (Core)`

Summary (about 330 of 1024 characters):

```
An open-source roleplay framework for Arma Reforger: jobs, money, vehicles, police, crime, medical, organizations and saves that survive a restart. It is setting-neutral and needs a pack to play. Looking to play? Subscribe to Life Framework: Everon 1989, which pulls this item in. Unfinished; report what breaks on Discord or GitHub.
```

Description (BBCode, under 5000): paste `docs/marketing/workshop/description.bbcode` as it stands (it names APL and MIT, sends
players to the pack, lists the framework systems and links the host guide). Read its "Licence and credits" section once
before you paste.

Change notes:

```
First Workshop build of the Life Framework core. Alpha.
- The framework systems: jobs and levels, cash, bank and shops, vehicles, crime, police, medical, organizations.
- Saves carry over a server restart.
- Setting-neutral: no map of its own. Looking to play? Subscribe to Life Framework: Everon 1989.
```

### Pack

Title: `Life Framework: Everon 1989`

Short description (summary, 331 of 1024 characters):

```
Everon, 1989. A village where everyone knows everyone and nobody has told you the rules. Fill in form R-89, get your ID card, take a job, bank your pay and buy a car. Or take a darker road and meet the police. Needs Life Framework (Core), which comes with it. Alpha: report what breaks on Discord or GitHub.
```

Long description (BBCode, about 2900 of 5000):

```
[h1]Life Framework: Everon 1989[/h1]

Everon, 1989. You have just arrived in a village where everyone knows everyone and nobody has told you the rules. You fill in form R-89, collect an ID card from the clerk on the square and find your feet. Honest work pays: parcels, fuel, fares. The bank keeps your money and the police keep order. At the edge of the village a few people make more on a good night than on a good week, and the police would like a word.

This is the flagship content pack for [b]Life Framework (Core)[/b], an open-source roleplay framework for Arma Reforger. It is an alpha. You will find rough edges, and things change between versions.

[h2]What it needs[/h2]
[b]Life Framework (Core)[/b], version 0.1.0 or later. Subscribing to this item pulls it in, so you install nothing else.

[h2]How to play[/h2]
[list]
[*]Subscribe to this item.
[*]Alone: open [b]Scenarios[/b], pick [b]Life Framework: Everon 1989[/b] and press Play.
[*]With friends: one of you hosts it from the Scenarios list, or you join a server that runs it. Look for [b]Life Framework | Everon 1989 (Alpha)[/b] in the server browser.
[/list]

[h2]What to try[/h2]
[list]
[*]Fill in form R-89 when you arrive, then ask the clerk on the square for your ID card.
[*]Take the parcel job at the depot. Fuel delivery and the taxi open as you level up.
[*]Bank your pay at the ATM and buy food at the general store.
[*]Collect your car from the garage, get the other one out of the impound, or buy one at the car lot.
[*]Rob a till, drill the bank vault or the cash van, and see the police get the call.
[*]Clock in at the police desk, work the wanted board, fine and jail.
[*]Get hurt and wait for the medic, or give up and respawn.
[*]Found an organization, use the hideout stash, sell to the fence, and run the moonshine still.
[/list]

[h2]Rules[/h2]
How the jobs, money, police and crime systems work, and what is allowed on a server, is written down in one place:
[url=https://life-framework.org/rules]life-framework.org/rules[/url]

[h2]Run your own server[/h2]
Mod IDs [b]79636E668EA37AC9[/b] (the core) and [b]B1E84D5687E11724[/b] (this pack), scenario [b]{C638C03B775C31BB}Missions/LF_Everon1989.conf[/b]. The example server folder has a ready config and a start script, and the host guide walks through it:
[url=https://github.com/Life-Framework/Life-Framework/blob/main/docs/playtest/workshop-host.md]Host guide[/url]
[url=https://github.com/Life-Framework/Life-Framework/tree/main/server/example]Example server[/url]

[h2]Feedback[/h2]
Tell us what broke, what confused you and what you liked. One report per problem, with what you did, what you expected and what happened. Screenshots help.
[list]
[*]Discord: [url=https://discord.gg/MjMexJteqz]discord.gg/MjMexJteqz[/url]
[*]Issues: [url=https://github.com/Life-Framework/Life-Framework/issues]github.com/Life-Framework/Life-Framework/issues[/url]
[*]Website: [url=https://life-framework.org]life-framework.org[/url]
[/list]

[h2]Licence[/h2]
This Workshop build is licensed under the Arma Public License (APL). The source on GitHub is MIT. Base-game assets belong to Bohemia Interactive. Full credits are in the repository.
```

This is the page copy of [marketing/workshop-flagship.md](../marketing/workshop-flagship.md), unchanged. It does not
mention cash top-ups or test cars: the flagship header closes the playtest helpers. The scenario's own details text
(shown in the game's Scenarios list) already says a local game is a preview.
`life-framework.org/rules` answers with a page (HTTP 200) as this was written; confirm it shows the rules text, or
delete the "Rules" section before you paste.

Change notes:

```
First Workshop build of Life Framework: Everon 1989. Alpha. Needs Life Framework (Core) 0.1.0 or later; subscribing pulls it in.
- A village on Everon in 1989: solo in a local game (a preview), hosted, or on a dedicated server.
- Arrival: form R-89, the town clerk, and the ID card.
- Jobs: parcel courier, fuel delivery, and taxi, unlocked by level.
- Money: cash, ATM and bank account, general store.
- Vehicles: car lot, garage, impound, keys and locks.
- Crime: till robbery, bank vault and cash van heists, breaching, the fence, moonshine, the chop shop, the lawless quarry.
- Police: duty desk, ranks, dispatch, wanted board, fines, and jail.
- Medical: downed state, medics, the hospital.
- Organizations and the hideout stash.
```

## Tags

The dialog may offer a fixed tag list rather than free text. Pick the nearest of these and note in the issue which
were missing.

- Core: `Roleplay`, `Life`, `RPG`, `Economy`, `Jobs`, `Police`, `Crime`, `Multiplayer`, `Framework`
- Pack: `Roleplay`, `Life`, `RPG`, `Economy`, `Jobs`, `Police`, `Crime`, `Multiplayer`, `Everon`, `1989`

(The core is setting-neutral, so it does not carry the setting tags; `release-first-upload.md` listed "Everon 1989"
on the core and is corrected to match the kit.)

## Pictures

Gallery pictures are 1280 px wide and each under 500 KB; every file named below is 1280x720 and 300 to 487 KB.
All are real game frames with no HUD or dialogue box. Paths are under `website/public/docs-images/`.

| Use | File | Notes |
|---|---|---|
| Pack screenshot 1, the square at arrival | `318/square-after.png` | The one kept for #318; shows the town board |
| Pack screenshot 2, the bank | `area-2-bank/wide.png` | Exterior, bank sign, market, street. Stand-in for the planned `318/bank-after.png`. Taken 2026-11-10, before the 2026-11-14 teller move (#931); the exterior is unchanged |
| Pack screenshot 3, the police post | `area-6-police/angle.png` (or `area-6-police/wide.png`) | Stand-in for the planned `318/police-after.png`; dispatch and wanted boards, the station sign, two jeeps. Taken 2026-11-10 |
| Pack screenshots 4 and on, optional | the rest of `docs/marketing/workshop/screenshots/` (store counter, car lot, fuel station, hospital, cash van, hideout bench) | Already sized as 1280x720 JPEGs under 360 KB. Not re-viewed for this page, and they predate the 2026-11-14 polish; glance at each before you use it |

Not usable: the contact-sheet pictures (`look-fixes/bank-after.jpg`, `place-audit/police-facade-after.jpg`) carry yellow
debug labels and red probe rays. `931/bank-after.png` is a clean bank interior but it is a plain grey lobby; skip it.

### Gaps (no good picture exists)

1. **Pack preview image.** `docs/marketing/workshop/everon-1989/preview.jpg` is a flat dark "DRAFT PLACEHOLDER" card
   with cut-off text. Nothing in the repo is a 1920x1080 in-game frame of the village without text. The spec is in
   `docs/marketing/workshop/everon-1989/README.md` (1920x1080 JPEG under 600 KB, late-afternoon, no HUD, no baked
   text). The frames above are 1280x720 and would be upscaled. Fastest honest option: a 1920x1080 retake of the
   square, from the `318/square-after.png` viewpoint, on a `--profile full` session. This is the one picture that
   blocks an upload that looks right.
2. **Core preview image.** `docs/marketing/workshop/preview.jpg` (1920x1080) shows the village with the logo and the
   words "LIFE FRAMEWORK / PLAYTEST TOWN" baked in. That name is retired and the core brief wants neutral art, so it
   should not go up. A logo-only card is enough for a setting-neutral core. `make-images.ps1` builds these.
3. **The planned bank and police pictures** (`318/bank-after.png`, `318/police-after.png`) were never taken: the
   2026-11-14 session frames carried the clerk's dialogue box, a letterbox and the HUD. `tools\cli look bank` and
   `tools\cli look police-station` give clean frames without a session, and `tools\cli evidence 318 after <area> --name bank`
   (then `--name police`) keeps them. The stand-ins above are good enough for 0.1.0.
4. **No picture of play.** Nothing shows a job, an arrest, a robbery or the form R-89 screen. The Workshop page shows places only.
5. **The scenario image** the game list shows is `Missions/LF_PlaytestTown_Preview.edds` (the pack's icon, preview and
   loading screen). It is the same "Playtest Town" art as the core preview in gap 2, so check it in the in-game
   Scenarios list after publish. Replacing it means rebuilding its `.edds` in Workbench and a new version.
6. **Showcase video (#390).** Required for Public, not for Unlisted.

## Post-upload checks

From the worktree, after both items are published:

1. **Workshop boot check** (#371). It boots a clean dedicated server on lf-cloud with no local addons and only the
   Workshop IDs, then a client with no local addons:
   ```
   tools\cli workshop boot-check 79636E668EA37AC9 B1E84D5687E11724
   ```
   Pass is `SUMMARY workshop-boot-check ok stage=pass`. A failure names the stage: `ids` (a placeholder), `clean`
   (a local LifeFramework addon on the box, or UDP 2399 busy), `mods` (the server could not get a mod: a wrong ID,
   or the item is still Private), `scripts` (a `SCRIPT (E)` or `VME:` line), `ready` (no `Entered online game state.`
   in 420 s; raise it with `--budget <s>`), `join` (the client never joined: join by hand from a normal game and keep
   the server log). The full log is `tmp\boot-check\console-<stamp>.log`. `--no-client` skips the join.
2. **Player check, same day**, as a normal game install: Workshop, search `Life Framework: Everon 1989`, subscribe, wait
   for both downloads. Scenarios lists exactly one Life Framework entry ("Life Framework: Everon 1989", 16 players, the
   picture and the details text) and no "Playtest Town". Press Play, pass the rules quiz and form R-89 and stand on the
   square. Check the Workshop page's scenario list too. If it shows Playtest Town, the backend ignores
   `m_bShowInScenarioMenu`: see `workshop-publish.md`, "Check it as a player".
3. **`tools\cli release 0.1.0`** again (dry run, then `--confirm`). `--confirm` stamps `Version "0.1.0"` into both `.gproj`
   files and both kit READMEs, runs every gate (validate --full, release-ready --strict, the suite, the config harness,
   the placement audit, both packs), writes `tmp\release\notes-0.1.0.md` and tags `v0.1.0` on a release commit without
   moving a branch. It must run from `main`, or a detached HEAD at `origin/main`; if `main` moved since the playtested commit, the tag
   lands on the newer commit, so read `git log <playtested sha>..origin/main` first. It never uploads; `--push` pushes only the tag. Two cautions: (a) the stamp adds a `Version` line to the `.gproj`
   that the upload did not contain, so the tag differs from the uploaded commit by exactly those lines; if you want the
   tag to equal the upload, run `--confirm` before step 1 of "Workbench steps" and publish from the tagged commit, but
   first confirm Workbench still opens a `.gproj` with that line (the open question on #369). (b) It takes the test
   lane for the suite; expect it to queue.
4. **Record it:** the page URLs and the passing SUMMARY line in `docs/setup.md` and on #370 and #371; the pack's ID
   into `tools/release/example-zip.json` then `tools\cli example-zip --final`; and the
   [after-publishing list](../playtest/workshop-publish.md#after-publishing) (Discord post, website news, playtest page).
   `docs/marketing/workshop/discord-announcement.md` and `website-news.md` still announce the single playtest item
   and need a rewrite for two items before they are posted.

## Stale kit files (known, not changed by this page)

- `docs/marketing/workshop/everon-1989/description.bbcode` and the pack block of `docs/marketing/workshop/changelog.md`
  are the 0.1.0 draft from before the flagship header closed the playtest helpers: they have a "Playtest settings"
  section (cash top-ups, two test cars, the first player made police) that no longer matches the listed scenario. Paste
  the text on this page instead. (The pack change block in `changelog.md` is updated to match.)
- `docs/marketing/workshop/everon-1989/README.md` still says "DRAFT" and "the in-game briefing (#374) has no PR yet";
  #374 landed in #1327 and #1372. Maintainer sign-off on the premise and voice (#375) is still open.
- `docs/marketing/workshop/preview.jpg` and `everon-1989/preview.jpg`: see gaps 1 and 2.
