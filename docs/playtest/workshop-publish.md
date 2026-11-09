# Publish the M1 build to the Workshop

Checklist for the maintainer. Nothing here publishes by itself; only the Publish
button in Workbench uploads, and only the maintainer clicks it.

Workbench publishes a bundled copy of the project, so the build has to compile
first. Keep the source tree: packaged files cannot be extracted back.

## Before you start

- [ ] `main` is green: `tools\cli validate --full` and
      `tools\cli test --no-build --tier all` pass in a worktree (or run
      `tools\cli wt gate <slug>`).
- [ ] The release commit is tagged, for example `git tag m1.0` and
      `git push origin m1.0`. Use the same number as the Workshop version.
- [ ] No generated files are staged (`resourceDatabase.rdb`, `*.rdb.lock`, the
      addon `log`, `*.gproj.user`).
- [ ] The EnfusionMCP handler scripts are gone from the mod
      (`addons/LifeFramework/Scripts/WorkbenchGame/EnfusionMCP/` is absent).
- [ ] The licence is APL. Decision 2026-11-08: the Workshop build is APL and the
      GitHub repo stays MIT; the description names APL and links the repo.
- [ ] A Bohemia Account is linked to the Steam account that will own the item:
      **Workbench > Link** (one time).
- [ ] Use the stable Workbench, not Experimental Tools. They publish to separate
      Workshops.

## Publish in Workbench

1. Open `addons/LifeFramework/LifeFramework.gproj` in Workbench.
2. Build for PC (`tools\cli build` in a worktree, or the Workbench build) and fix
   any script error before publishing.
3. Choose **Workbench > Publish Project**.
4. **Working Dir:** leave the default
   (`%USERPROFILE%\Documents\My Games\ArmaReforger\publish\LifeFramework`).
   Never point it inside the addon folder.
5. Fill the metadata below. The preview image and every screenshot are JPG or
   PNG and under 2 MB each.
6. **Category:** pick the roleplay/gameplay category that fits. At least one is
   required.
7. **Visibility:** `Private` or `Unlisted` for the private rounds. `Public` only
   for the release, which ships with the showcase video (decision 2026-11-08:
   two or three private rounds first).
8. **Version:** `1.0` for the first publish. Increment on every update, even a
   description-only change; Workshop updates are published as new versions.
9. **Change Notes:** one line per change since the last publish. First publish:
   `M1 playtest: the 1989 Everon town, jobs, bank, shop, vehicles, police, organizations, and hideouts.`
10. Click **Publish**. Workbench bundles, shows the upload size, and asks to
    continue. Confirm and wait for the upload.
11. Open the Workshop page, subscribe, join the hosted server, and finish the
    arrival chain on a clean profile. That is the M1-24 acceptance proof.
12. Copy the item ID and URL into `docs/playtest.md`, the README, and the
    release notes.

## Workshop metadata (draft)

**Project Name** (30 characters max):

```
Life Framework - 1989 Everon
```

**Summary** (1024 characters max):

```
A 1989 Everon roleplay town in public playtest. Take a parcel job, bank your pay, buy a car, rob a till, and clock in as a police officer. Found an organization, lease the alley hideout, and run contraband. Built on Life Framework. Expect rough edges; report them on GitHub.
```

**Description** (5000 characters max):

```
Life Framework - 1989 Everon is the first public playtest of Life Framework, a roleplay framework for Arma Reforger. It runs a small 1989 Everon village on one street, hosted from the maintainer's PC.

WHAT YOU CAN DO
- Register on form R-89 and follow the town clerk through your first day.
- Take a parcel job at the post office, then fuel delivery and the taxi as you level up.
- Bank your pay at the ATM, shop for food and supplies, and buy a car at the lot.
- Rob a till, breach a door, or hit the bank vault; the police get a radio dispatch and a wanted board.
- Clock in as an officer, read the wanted board, arrest, fine, and jail.
- Found an organization, lease the alley yard, use the hideout stash, and run contraband from the barn to the pub.
- Rejoin after a restart: your cash, bank, job, organization, and stored cars are still there.

HOW TO PLAY
Subscribe to this item, then find "Life Framework Playtest Town" in the Arma Reforger server browser. The game enables the mod for you. The server is not up all the time; session times are announced on Discord. On your first join, fill in form R-89. Everyone starts as a civilian; police clock in at the station desk.

PLAYTEST, NOT A RELEASE
This is M1: the loop works, but expect rough edges, missing polish, and changes between sessions. One issue per problem on GitHub, with what you did, what you expected, and what happened. Screenshots help.

LICENCE AND CREDITS
The Workshop build is licensed under the Arma Public License (APL), as the Workshop Terms of Use require. The source on GitHub is MIT. Built on Everon Life by Arkensor (MIT) and code from Overthrow (used with permission, MIT). Base-game assets belong to Bohemia Interactive.

LINKS
Source and issues: https://github.com/Life-Framework/Life-Framework
Playtest guide: https://github.com/Life-Framework/Life-Framework/blob/main/docs/playtest.md
Discord: https://discord.gg/MjMexJteqz
Website: https://life-framework.org
```

**Tags** (space-separated):

```
Roleplay RPG Life Simulation Economy Jobs Police Crime Multiplayer Everon 1989
```

**Licence:** Arma Public License (APL).

## Screenshots

Shoot at 1920x1080 on a clean client, with no debug overlays, no dev-menu text,
and no white test meshes. Each image under 2 MB.

1. **Preview image (Workshop tile):** the main street from the spawn square, the
   clerk and the town board in frame, daylight.
2. **Arrival:** registration form R-89 filled in, the clerk visible.
3. **Job:** the post office counter with the parcel prompt.
4. **Bank:** the ATM terminal showing the balance.
5. **Shop:** the general store price sheet.
6. **Vehicle:** the car lot price sheet and a bought car.
7. **Police:** the duty desk, an officer in kit, and the wanted board.
8. **Organization:** the registry desk ledger and the hideout gate.
9. **Hideout:** the stash open as a member.
10. **Contraband:** the barn pickup or the pub delivery.
11. **Group (optional):** two or three players on the street.

## After publishing

- [ ] The Workshop URL is in `docs/playtest.md`, the README, and the release
      notes.
- [ ] Each private round is recorded in `docs/playtest/` with its date, players,
      and the issues it filed.
- [ ] The public publish and the showcase video ship together, and the video is
      linked on the Workshop page.
- [ ] Issue #109 stays open until the public publish and the video are done.
