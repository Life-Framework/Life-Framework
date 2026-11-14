# M3 release notes: Life Framework for server owners

Status on 2026-11-14: M3 is built and merged to `main`. It is **not verified and not published**.
The playtest pass is in [playtest/m3-signoff.md](../playtest/m3-signoff.md), the Workshop upload is
still the maintainer's ([release-first-upload.md](../release-first-upload.md)), and the first
version number is chosen at the cut (the Workshop kit changelog is written as `0.1.0`).

M3 turns the town into something other people can run. You get two Workshop items, one config file,
and a server folder you can start. You do not need Workbench or a checkout.

## What you get

**Two Workshop items.** The core (`Life Framework (Core)`, `LifeFramework`, `79636E668EA37AC9`) is the
framework and has no map of its own. The pack (`Life Framework: Everon 1989`, `LF_Everon1989`,
`B1E84D5687E11724`) is the flagship town and needs the core. Both are APL; the source stays MIT.
Names and versions: [release-process.md](../release-process.md#names-and-versions).

**One owner config.** Put a `config.json` in `$profile:LifeFramework/` and it layers over the shipped
defaults. Prices, wages, fines, autosave, zone rules, police ranks, job XP and more are values in
that file. [Owner config check](../config/owner-config.md) · [config reference](https://life-framework.org/config/reference).

- A bad value is refused and the shipped one stays; the server still boots. `tools\cli config-check config.json` tells you before a restart.
- Clients get the values through a versioned stream, so late joiners see the same prices. `/configreload` (admin) re-reads the file without a restart.
- Every system has an on/off switch (`systems.switches`), so you can run a town without robbery or without farming.
- Lists (shops, vehicles, crimes, jobs, dialogue) follow one rule: add, replace, patch or remove by id. [Override model](../config/override-model.md).
- Owner pages for the choices that matter: [persistence switches](../owner/persistence-switches.md) (what survives a restart), [disconnect rules](../owner/disconnect-rules.md) (logging out while cuffed or downed), [renaming terms](../owner/rename-terms.md) ("police" to "Sheriff", the currency symbol, in every language).
- Guard rails you do not configure: the server checks every client request and rate-limits purchases, and a price table that lets players make money for nothing is flagged by `config-check`.

**A config generator.** [life-framework.org/config](https://life-framework.org/config) builds the file from the schema: pick a preset (flagship 1989, small server, hardcore economy), change fields, import an existing file, and download `config.json` and a matching `server.json`. It only exports a file that passes the same checks as `config-check`.

**An example server.** [`server/example/`](../../server/example/README.md) is `server.json`, `config.json` and `start.cmd` / `start.sh`. Set a name and an admin password, run the start script, and the core and the 1989 pack boot as a dedicated server. A zip of it is built by `tools\cli example-zip`; the zip stays a draft until the Workshop ids are real. Delete the pack from `game.mods` to run the core alone, or swap in your own ([swap the pack out](../../server/example/README.md#swap-the-pack-out)).

**An upgrade path.** The owner file carries a `schemaVersion`. At boot the server copies your file to `config.json.bak-v<N>-<time>`, runs any migrations, and refuses a file written for a newer version. `config-check` prints which migrations would run. Each release gets an [upgrade page](../upgrades/README.md). Saves carry a version, and record shapes are append-only from here ([persistence.md](../persistence.md)).

**Packs.** Anyone can add a pack beside the flagship without touching the core.

- `tools\cli pack new <Name>` scaffolds one, `pack check` validates it and `pack explain <key>` shows the merged value and which layer set it.
- A pack can add or change content, rename terms, swap prefabs (a different cash or handcuff model), override the UI theme, patch NPC dialogue, and add its own config sections.
- A pack names the core versions it supports and sticks to the declared [public surface](../architecture/public-surface.md). Saves keep content by stable id with an alias table, so a rename does not lose items.
- The core runs on a map it knows nothing about. On vanilla Everon, a system with no place logs once and stays off.
- Rules: [core vs pack](../architecture/core-vs-pack.md), [licence note for pack authors](../architecture/pack-licence.md), and the two walkthroughs [add a job](../owner/add-job.md) and [add a shop](../owner/add-shop.md).

**Install and release docs.** [Install a server from zero](../owner/install.md). For the maintainer side: [release process](../release-process.md) (branches, `release`, rollback) and `tools\cli workshop boot-check <coreId> <packId>`, which boots a clean server from the Workshop ids alone.

## Also in M3

Smaller changes owners will notice: world cleanup of abandoned vehicles, dropped items and bodies; playtime per account and per job; one crime table behind wanted, fines and jail; one base price per vehicle with per-action multipliers; organisation ranks with rights; zone rule sets as data; and an in-game `/credits` screen. [features.md](../features.md) has the row for each.

## What is not in it

- Solo play from the scenario list with a first-run guide (#374) has no PR yet.
- The two sidecar save payloads for vehicle state and held ground (#1216, #1217) have no PR yet, and the record-shape freeze (#1218) is set at the persistent launch. The shapes are specified in [record-shape-audit.md](../architecture/record-shape-audit.md).
- A real upgrade page: `docs/upgrades/` has the template and the generator but no release page yet.

## Needs the maintainer

Nothing below can be done by an agent.

| Item | What you do |
|---|---|
| **#370** First Workshop upload of core and pack | Publish the core, then the pack, from Workbench with your Bohemia account. Steps: [release-first-upload.md](../release-first-upload.md). Record both page URLs and fix the ids in `docs/setup.md` and `tools/release/example-zip.json`. Labels: `needs-human`, `needs-playtest`. |
| **#371** A clean server boots from the Workshop items alone | After #370, run `tools\cli workshop boot-check 79636E668EA37AC9 B1E84D5687E11724` and record the passing line. If the client-join stage fails on a ready server, join by hand and keep the server log. Labels: `needs-human`, `needs-playtest`. |
| Sign-off playtest | [playtest/m3-signoff.md](../playtest/m3-signoff.md), then `tools\cli playtest verdicts` and your own `gh` run; `promote` only after. |
| Version and the `.gproj` Version field (#369) | Pick the first version. `.gproj` has no version field today and `release` adds one; open the project in Workbench once with the stamped line before the real release. |
| Branding and art (#375) | Sign off the premise and voice rules in [branding](../branding/core.md), and replace the two placeholder `preview.jpg` files. |
| Docs acceptance (#290, #292, #293, #1004) | Someone who did not write the install guide and walkthroughs follows them on lf-testbox from a fresh server, after #370. |
| `docs/upgrades/<version>.md` | Fill the first upgrade page from `tools\cli upgrade-page` before promoting ([upgrades README](../upgrades/README.md): no page, not ready). |

`needs-human` is on ten open issues. Only #370 and #371 are M3. The other eight belong to later work:
#404 and #405 (Dev-Gates: first promotion and the M2 audit tag), #567 (M2.5: outsider playtest access),
#378, #555 and #556 (M4: hosting trial, invite-only alpha, wipe policy), #664 and #995 (M5: AI 3D licence gate,
ArchBuilder harness).
