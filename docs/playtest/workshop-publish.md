# Publish the playtest town to the Workshop

Click-by-click for the maintainer. Only the **Publish** button uploads, and only the maintainer
presses it, signed in with the Bohemia Account that will own the item. Every field value is in
[`docs/marketing/workshop/`](../marketing/workshop/README.md).

What gets published: the `LifeFramework` addon (mod ID `79636E668EA37AC9`, no dependencies beyond
the base game). Its scenario **Life Framework: Playtest Town**
(`{1054CC8646357708}Missions/LF_PlaytestTown.conf`) is the DebugWorld town with the playtest
settings on: cash and bank top-up, two test cars per new character, the first player on an empty
police roster, saves across restarts, and the debug menu closed to everyone but admins. The
spawn hub (#607) lands in DebugWorld, so it ships in the first publish after it merges.

The Workshop item sits beside the one-click zip, not in place of it. The zip (`tools\cli playtest
release`, first cut as `playtest-m2-1`) stays the download button on the website and is described
in [`docs/playtest.md`](../playtest.md). Both carry the same addon; publish the Workshop item from
the same commit as the latest zip so the two do not drift.

## Before you start (once)

- [ ] Arma Reforger Tools, the **stable** Workbench. Experimental Tools publish to a separate
      Workshop.
- [ ] **Workbench > Link**: sign in with the Bohemia Account. It links to the Steam account that
      is running. Use the same pair for every later update.
- [ ] Read the Arma Reforger Workshop Terms of Service once (linked from the BIKI page
      [Mod Publishing Process](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Publishing_Process)).
      The build is APL (decision 2026-11-08); the repo stays MIT.

## Prepare the build

- [ ] Pull `main` into a worktree and run `tools\cli wt gate <slug>` (validate --full, build,
      full suite). Publish only from a green commit.
- [ ] `tools\cli pack` in that worktree. It packs the addon headless the way Publish bundles it and
      prints `pack OK ... data.pak`. A `Failed to load metafile of mission config image` line
      means a mission image lost its source; fix that before publishing.
- [ ] `addons/LifeFramework/Scripts/WorkbenchGame/EnfusionMCP/` does not exist (run `wb_cleanup`
      if it does). The handler scripts are developer tooling.
- [ ] No generated file is staged or modified: `git status` in the worktree is clean.
- [ ] Tag the commit: `git tag workshop-0.1.0 && git push origin workshop-0.1.0`.

## Publish in Workbench

1. Open Workbench and load `addons/LifeFramework/LifeFramework.gproj` **from that worktree**
   (Workbench launcher > Add Existing > the `.gproj`). The main checkout's world-editor state
   does not belong in the upload.
2. Wait for the Resource Browser to finish loading. In the Script Editor, **Build > Compile and
   Reload Scripts** must finish with no errors.
3. **Workbench > Publish Project**.
4. Fill the dialog from [`docs/marketing/workshop/README.md`](../marketing/workshop/README.md):
   1. **Project Name**: `Life Framework`
   2. **Working Dir**: leave the default
      (`%USERPROFILE%\Documents\My Games\ArmaReforger\publish\LifeFramework`). Never a folder
      inside the addon.
   3. **Preview Image**: `docs/marketing/workshop/preview.jpg`
   4. **Screenshots**: add the nine files in `docs/marketing/workshop/screenshots/`, in name order.
   5. **Contributors**: leave empty, or add a second maintainer who may publish updates.
   6. **Category**: Game Mode. Add Scenario as well if the list offers it.
   7. **Tags**: `Roleplay Life RPG Economy Jobs Police Crime Multiplayer Everon 1989`
   8. **License**: Arma Public License (APL).
   9. **Version**: `0.1.0`. Raise it on every later publish, even a text-only change.
   10. **Visibility**: `Unlisted` for the private rounds (decision 2026-11-08: two or three
       rounds first). `Public` only together with the showcase video.
   11. **Summary**: paste `summary.txt`.
   12. **Description**: open the large editor and paste `description.bbcode`.
   13. **Change Notes**: paste the 0.1.0 block from `changelog.md`.
5. Click **Publish**. Workbench bundles (about a minute), then shows the upload size, about
   55 MB. Confirm and wait for the upload to finish.
6. Workbench reports the item published. The Workshop page address starts with
   `https://reforger.armaplatform.com/workshop/79636E668EA37AC9`; copy the full address from the
   browser. Open it in a browser and check
   the preview, the screenshots, and the description.

## Check it as a player (same day)

The local test (below) proved everything except the Workshop download itself, which only exists
after publish.

1. Start Arma Reforger normally. **Workshop**, search `Life Framework` (or open the Unlisted link
   from the page above), **Subscribe**, wait for the download.
2. **Scenarios**: `Life Framework: Playtest Town` is listed with 16 players, the preview image,
   and the description. Select it and press **Play**.
3. The loading screen shows the town picture and the playtest text. In town, form R-89 opens;
   sign it. You stand on the square with the "New in town" note, cash in hand.
4. Back in **Scenarios**, select it and press **Host**. A friend joins from the server browser.
5. Start a dedicated server with [`server-config.example.json`](server-config.example.json) and
   [`workshop-host.md`](workshop-host.md); it downloads the mod and a client joins from the
   browser.

If step 2 shows the scenario name as `#LF-...` or no picture, the header's text did not reach
the backend: fix `Missions/LF_PlaytestTown.conf`, raise the version, and publish again.

## After publishing

- [ ] Add the Workshop URL next to the zip download button on the website (the playtest download
      section built from `docs/playtest.md`).
- [ ] Post [`discord-announcement.md`](../marketing/workshop/discord-announcement.md) with the
      URL filled in, and add [`website-news.md`](../marketing/workshop/website-news.md) to the
      site's news.
- [ ] Put the Workshop URL in `docs/playtest.md` (and drop its "Until a public Workshop item
      exists" line) and in the README.
- [ ] Record each private round in `docs/playtest/` with its date, players, and the issues it
      filed.
- [ ] When the showcase video (#390) is ready, add it to the description, switch visibility to
      Public, and publish as a new version.
- [ ] Issue #109 stays open until the public publish and the video are done.

## Updating later

Open the same `.gproj` from a green worktree, **Publish Project**, raise **Version**, write the
**Change Notes**, and publish. The dialog keeps the last publish's text. Packaged files cannot be
turned back into source; the source is this repository.

## How the local test was done

`tools\cli pack` produced the same `data.pak` Publish bundles. A clean client profile got it in
`profile/addons/LifeFramework_79636E668EA37AC9/` (where the Workshop puts downloads) with a copy of
the backend `meta` file a download carries, because the in-game Scenarios list reads its entries
from that file. A test-only driver addon then pressed through the title screen, opened
Scenarios, selected the town, pressed Play, and signed form R-89. A dedicated server
(`-server <scenarioId> -addons 79636E668EA37AC9`, the packed copy in `-addonsDir`) then took a
client from another clean profile: form R-89, spawn on the square, the cash top-up and both test
cars. A `-config` start with the example config could not be proven before publish: the server
fetches `game.mods` from the Workshop and does not use a local copy of an unpublished item.
Evidence is in the PR that added this checklist.

The Scenarios detail pane of that local test named "Bohemia Interactive" as the author, because
the stand-in item had no real owner. After publish, check that it shows your account or "Life
Framework Team".
