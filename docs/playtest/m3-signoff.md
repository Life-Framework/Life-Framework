# M3 sign-off playtest: 2026-11-15

91 issue(s) on 85 line(s). About 170 min including the 6 smoke checks (was about 321 before the trim). 36 issue(s) need a person (about 117 min with smoke); 55 are agent-proven and get a one-minute spot-check from the appendix (about 53 min).

**Lands before you play.** [#1361](https://github.com/Life-Framework/Life-Framework/pull/1361) (heldground sidecar payload, save version 34, `Refs #1217`) is in the ship gate and not yet on `main`. It adds no line of its own: it is a save-version bump with no player-visible change, and smoke S3 to S5 plus the vehicle-state spot-check below cover its restart path. Once it merges, play from `main` so the console says `save loaded: version 34`, and read the version in step 1 of #1216 as 34. Its own manual steps (host an existing v33 save: `save loaded: version 33`, lease a hideout, restart, the gate is still leased and locked, `applied 0 of 0 held-ground row(s)`) take 3 minutes and fit inside S4 and S5.

Tick `[x]` at the front when the check passes; tick `fail [x]` at the end when it does not, and put the reason on a `> note:` line under it.
Leave a box empty to skip: nothing happens to that issue. Then `tools\cli playtest verdicts <this file>` prints the `gh` commands for review; it does not run them.
Tags: `[2P]` needs a second client, `[DEDICATED]` needs the dedicated server, `[RESTART]` needs a server restart, `[JIP]` needs a late joiner, `[LOOK]` is a visual or audio check only a person can judge.

**Scope.** The 91 open issues in the "M3 Framework Release" milestone that are `needs-playtest` or `in-dev` and have a line here, plus the two epics #224 and #229 that now carry `needs-playtest` and are signed off by their child lines (93 carry a label in all). Added since the 2026-11-14 revision: #374 (flagship listed once, [#1327](https://github.com/Life-Framework/Life-Framework/pull/1327) and [#1372](https://github.com/Life-Framework/Life-Framework/pull/1372)) and #1216 (vehiclestate payload, [#1309](https://github.com/Life-Framework/Life-Framework/pull/1309)); #1217 follows when #1361 merges. Epics #225, #227, #228, #366, #381, #1218 carry no label; see [the release notes](../releases/m3-framework-release.md#needs-the-maintainer). The 2026-11-14 revision was made on main at `abd31b49` with `tools\cli playtest checklist --all --from-json`, then re-sorted by what automation already proves. This revision re-ran `tools\cli playtest checklist` on `main` at `eaacf589` (254 open `needs-playtest`/`in-dev` issues across all milestones, 898 min unsorted) and kept the M3 scope and the sorting.

**How the lines were sorted.** Each line was checked against its merged PR (body and proof output) for automated in-game evidence of the same behaviour: a harness proof verb in `agent-box/proofs`, a config-harness client or boot fixture, an e2e restart run, or an `LF_Test` at WORLD or PERSISTENCE tier. A LOGIC-tier test counts for tooling, validators and pure data; it does not count for what a player sees.

- **Human-only (the sections below, first).** Feel, looks, audio, two real players, a late joiner, or a step the evidence does not reach. A line the evidence only partly reaches keeps just the steps it does not reach, under `do:`, and lists the rest under `already proven:`.
- **Agent-proven (the appendix, last).** The PR already carries the evidence, or it was run for this revision (named on each line). One minute each, to look once and tick. Their restart evidence is a real stop, `-loadSessionSave` and reload (`wt e2e`); the smoke list's S3 to S5 is the one human restart pass that covers them all.
- **Run for this revision.** One boot-stage run on fresh dedicated servers (7 fixtures PASS), one one-client session (`config-harness-proof` 11 fixtures PASS, `credits-proof` PASS, `quarry-proof` PASS), a two-client session (`disconnect-proof`, FAIL that day; fixed as proof drift in [#1318](https://github.com/Life-Framework/Life-Framework/pull/1318) and PASS since, see #691), and the offline tool runs. Three boot fixtures and one client fixture were added for this (`tools/test/config-fixtures`).

**Time.** Smoke 15, solo walk 31, two players 13, dedicated server 17, desk 18, blocked on the Workshop upload 23, spot-check appendix 53: about 170 min. If one sitting must be shorter: the smoke list, then the solo walk, two players and the dedicated block (76 min). The desk and blocked blocks can wait for the Workshop upload; #370 and #371 cannot pass before it.

**Findings from this revision.**
- `session disconnect-proof` failed two checks on 2026-11-14. [#1318](https://github.com/Life-Framework/Life-Framework/pull/1318) found proof drift (a rejoin is now the same identity since #1159; a logout sentence is created already paused), not a regression, and the proof passes on a two-client full-profile session. #691 stays human-only for the two real Steam accounts and the restart. See its line.
- A config section can be read the first time it is used rather than at server start: `jobs` and `zones` lines are absent from a freshly booted server's log until a gather, a zone check or the like. The old #1106 and #1108 steps said "start the server and expect" them; the lines below say where to look.
- The pack section range line (#553) is not logged at boot either; `config-check` is where it shows.

After ticking: `tools\cli playtest verdicts docs/playtest/m3-signoff.md` prints the `gh` commands. The maintainer runs them; agents do not.

## Smoke list (every promotion, first; a fail blocks promotion)

- [ ] S1 Boot. `tools\cli playtest` (or `host start` and join) reaches the world with no `SCRIPT (E)` and no `VME:` in the server console log (`tools\cli logs "SCRIPT \(E\)|VME:"` is empty).   fail [ ]
- [ ] S2 Join. A fresh profile signs the registration form and spawns in town with a character, cash, and the HUD. A second client joins the running server and spawns too.   fail [ ]
- [ ] S3 Save. Take an item and some cash (apple from the general store, a bank deposit), then wait for or force the autosave; the server log names the save and no persistence error.   fail [ ]
- [ ] S4 Restart. Stop the dedicated server cleanly and start it with `-loadSessionSave`: the world loads, no `Unable to find`, no `Wrong GUID/name`.   fail [ ]
- [ ] S5 Reload. Rejoin after the restart: same character, same cash, same item and bank balance as before the restart.   fail [ ]
- [ ] S6 Late joiner. With the first player mid-action (a half-grown plot, a downed or wanted player), a second client joins late and sees the same state, not a blank or default one.   fail [ ]

## Solo walk, in route order (~31 min)

- [ ] #374 M3-Release: Flagship mission is hostable and playable solo as a local game (~3 min) [LOOK]   fail [ ]
  - do: Main menu, Play, Scenarios: exactly one Life Framework entry, "Life Framework: Everon 1989", with the Everon picture, 16 players and a details text ending "A local game is a preview...". No "Playtest Town" entry.
  - do: Host it as a local game (no dedicated server, core and pack loaded). Pass the rules quiz, sign form R-89 and spawn (no "account is still loading"). The console has no `SCRIPT (E)` or `VME:` and shows `platform identity of player 1 arrived` then `character registered`. Smoke S1 and S2 can ride on this run.
  - already proven: listen-host on the flagship header, lf-cloud, no backend, fresh profile: PASS, no `VME`; `validate-pack-missions` rejects two listed headers on one world ([#1327](https://github.com/Life-Framework/Life-Framework/pull/1327), [#1372](https://github.com/Life-Framework/Life-Framework/pull/1372)). The harness boots straight into `-server`, so the menu list entry is only checked here.

- [ ] #325 M3-Addons: Split the string table into core keys and pack flavour (~2 min)   fail [ ]
  - do: Boot the playtest (`tools\cli playtest`): the town board on the square reads "Welcome to Everon, Town Board"; a new character's form reads "Civil Registry of Everon, Form R-89".
  - do: Rename `addons/LF_Everon1989/LF_Everon1989.gproj` away and boot again: the board reads "Notice", the form "Civil Registry, Form R-89", no raw `#LF-` text anywhere.
  - already proven: the suite is green with the pack (481/481) and with the pack moved away, and the core-only run is 500/500 ([#1121](https://github.com/Life-Framework/Life-Framework/pull/1121), [#1144](https://github.com/Life-Framework/Life-Framework/pull/1144)). `strings/split` is LOGIC, so what a player reads on the board is not proven in game.
- [ ] #1128 M3-Packs: move the remaining hard prefab paths onto prefab slots (~2 min) [LOOK]   fail [ ]
  - do: In DebugWorld: hand over a fine ticket, buy from a store (receipt), drop an item (evidence bag via the locker flow), place a sign and a wanted-poster board. All look as before.
  - already proven: prefab-slot lookups and spawns are WORLD tests (`prefabs/slots`, `prefabs/slot-swap-spawn`, [#1139](https://github.com/Life-Framework/Life-Framework/pull/1139), [#1239](https://github.com/Life-Framework/Life-Framework/pull/1239)). Only the look is left.
- [ ] #754 M3-Spec: Police career schema: ranks, pay and qualifications as config (~3 min)   fail [ ]
  - Join as police, clock in at the station desk, draw the kit (M9 for patrol).
  - Promote: a second on-duty senior officer promotes you, or accrue 3600 s on duty. Re-draw after clocking out and in: the M16A2 comes out, and the ticket form's fine cap rises from 1000 to 5000.
  - Clock out: shift pay matches the on-duty seconds times 0.5.
  - Change a rank row in LF_Police.conf (e.g. patrol fine cap) and see the ticket cap follow. No restart proof needed; the rank int is the shipped one.
- [ ] #755 M3-Spec: Zone rule sets as data: lawless, event and safe overrides per zone (~2 min) [LOOK]   fail [ ]
  - do: Start the playtest town and walk into the quarry (centre 178 0 172, radius 32): the red LAWLESS AREA tag shows and an enter toast fires. Walk out: tag and toast go, exit toast fires. Do it twice.
  - already proven: zone state at the warning sign, quarry sign and plaza: `quarry-proof` PASS in this revision's session (normal, lawless, safe).
  - already proven: the fountain zone refuses a hit: WORLD `LF_Test_SafeZone`; zone rule data: WORLD `LF_Test_QuarryZone` ([#826](https://github.com/Life-Framework/Life-Framework/pull/826)).
- [ ] #1077 M3-Config: Member initializers beat .conf values on LF_ZoneRuleSet (pack overlays would be ignored) (~3 min)   fail [ ]
  - do: In Workbench open `Worlds/CoreOnEveron/CoreOnEveron.ent` (vanilla Everon, core only) and play. Spawn as police and aim at a wanted player: no arrest action is offered.
  - do: Place a prefab with `LF_JailCellComponent` (and an ATM) on Everon and play again: the Jail off line is gone and arrest and banking work there.
  - already proven: core boots alone on vanilla Everon: `test --tier fast --world Worlds/CoreOnEveron/CoreOnEveron.ent` 242/242, with the four `no <Kind> placed on this map: off` lines and no VME ([#1113](https://github.com/Life-Framework/Life-Framework/pull/1113), [#1153](https://github.com/Life-Framework/Life-Framework/pull/1153)).
  - already proven: `.conf` zeros are read: WORLD `LF_Test_ConfZeroConfigs` ([#1124](https://github.com/Life-Framework/Life-Framework/pull/1124)).
- [ ] #1047 M3-Packs: Core boots on vanilla Everon; a system with no place logs once and stays off (~1 min)   fail [ ]
  - do: Open DebugWorld in Workbench, play, and shoot a building or destructible prop: it still breaks (the destruction manager now comes from the layer).
  - already proven: the Everon boot has no `already registered` and no `needs a entity catalog manager` line: 246/246 and an empty grep ([#1153](https://github.com/Life-Framework/Life-Framework/pull/1153)).
  - already proven: same steps as #1077 for the rest.
- [ ] #693 M3-Patterns: Every client request is checked on the server: an audit and a lint (~4 min)   fail [ ]
  - Spam a shop buy (DebugWorld, or a dedicated server with one client):
  - Open any shop, pick an item and click Buy about 10 times as fast as you can.
  - The first 4 buys go through. The slip then reads "One order at a time. Give the till a moment." and the further clicks charge nothing.
  - The server log has one [LFDebug:RateLimit] shop trade refused: player N is over the PURCHASE limit line, then one ... more request(s) refused in the last window line on the next buy after 2 s.
  - (+8 more steps: PR #824, PR #834, or `--steps`)
  - not dedicated: the first step runs in DebugWorld. covered: the request-audit lint (`request-audit-selftest`) and LOGIC `LF_Test_RateLimit`, [#824](https://github.com/Life-Framework/Life-Framework/pull/824), [#834](https://github.com/Life-Framework/Life-Framework/pull/834); the live rate-limit slip is not run in game.
- [ ] #966 M3-Packs: Packs override the UI theme (colours, fonts, paper texture) (~5 min) [LOOK]   fail [ ]
  - Build a pack overlay .conf of class LF_UIThemeConfig (copy Configs/Tests/LF_Test_UIThemePack.conf, give each record its own GUID) and list it in the mission header's m_aLF_ConfigOverlays.
  - Join the server, open the ATM, the wanted card and the org ledger: each should follow the pack's palette. Check the typed blank (stays beige; known), button focus, and the Notice sign card accent.
  - Set m_PaperTexture on paper to a real .edds: the sheet should show it tinted by the paper colour. Leave it empty: the flat sheet.
  - Misspell a colour (#ZZZZZZ): [LFDebug:UITheme] logs once and that token takes the 1989 colour; the sheet still opens.
  - (+1 more steps: PR #1138, or `--steps`)
  - covered: WORLD `ui-kit/theme-pack` and the `ui-theme` flow pictures ([#1138](https://github.com/Life-Framework/Life-Framework/pull/1138)). Left: a real client reading the pack palette on a dedicated server.
- [ ] #323 M3-Addons: Move pack resources into LF_Everon1989 in batches (~6 min) [LOOK]   fail [ ]
  - do: Audio walk ([#1211](https://github.com/Life-Framework/Life-Framework/pull/1211)): at the ATM the ambient hum plays; at the mine and the furnace/chop bench the work-area ambience plays; on the street the cash van, pub and store radios play; the clerk's voice plays.
  - do: Take a pickaxe from the miner stand, holster it, mine a rock: it works as before.
  - do: In Workbench open DebugWorld: the village (square, police station, car lot, quarry with the mining sign) shows as before, its layers listed under the `Everon1989Town` SubScene.
  - already proven: a save made before the move loads after it: `e2e --pre-move 502ebf39 --mission playtest` exit 0, pass 2 passed=44 ([#1185](https://github.com/Life-Framework/Life-Framework/pull/1185), [#1246](https://github.com/Life-Framework/Life-Framework/pull/1246)).
  - already proven: the suite is green with and without the village layers ([#1112](https://github.com/Life-Framework/Life-Framework/pull/1112)), with the pack (532/532) and core-only ([#1211](https://github.com/Life-Framework/Life-Framework/pull/1211)); annex-twin WORLD tests [#843](https://github.com/Life-Framework/Life-Framework/pull/843), [#844](https://github.com/Life-Framework/Life-Framework/pull/844), [#846](https://github.com/Life-Framework/Life-Framework/pull/846), [#847](https://github.com/Life-Framework/Life-Framework/pull/847), [#849](https://github.com/Life-Framework/Life-Framework/pull/849).
  - already proven: a new character owns a garaged and an impounded car: `LF_Test_Roadmap_Vehicles` and the e2e narrative.
  - already proven: sounds, textures and the Workbench view are not testable without a person.

## Two players (second client on this machine or LAN) (~13 min)

- [ ] #964 M3-Packs: Prefab slots: systems ask config for the prefab, packs swap the look (~3 min) [2P] [LOOK] [chapter #1005]   fail [ ]
  - do: Cuff a second player after the overlay swaps `handcuffs_worn` to another model: the new model shows on the wrist and uncuffing removes it.
  - already proven: overlay applies and `[LFDebug:PrefabSlots] slot ... (pack)` logs: WORLD `prefabs/slots`, `prefabs/slot-swap-spawn` ([#1139](https://github.com/Life-Framework/Life-Framework/pull/1139), [#1239](https://github.com/Life-Framework/Life-Framework/pull/1239)).
  - already proven: earning or withdrawing cash with a swapped cash prefab still counts and spends: WORLD `prefabs/cash-slot-swap` and `e2e --cash-swap` over a real restart ([#1186](https://github.com/Life-Framework/Life-Framework/pull/1186)).
- [ ] #1074 M3-Packs: Switch the economy systems (Banking, ATM, Shop, Trader, Jobs, Level, License, Whitelist, Organiza... (~2 min) [2P] [JIP]   fail [ ]
  - do: Put Whitelist off and join with an unlisted account: it gets in.
  - do: With the economy switches off, join a second client late and walk to the vault, rob kit stand, a shop counter, the ATM, the job depot, the registry desk and a trader: nothing is offered on the late joiner either.
  - already proven: each economy system off and on, the Banking dependency and held saved values: WORLD `systems/economy-gates` ([#1100](https://github.com/Life-Framework/Life-Framework/pull/1100)).
  - already proven: a switched-off ATM is hidden on a joined client: client fixture `presets/client-system-off` PASS in this revision; the boot log has one `<Id> is off (owner config)` line per system: boot fixture `presets/m3-systems-off-boot` PASS.
  - already proven: a late joiner receives config through the stream: `config-stream-proof` ([#856](https://github.com/Life-Framework/Life-Framework/pull/856)).
- [ ] #958 M3-Addons: Terms layer: packs and owners rename the framework's terms, in every language (~3 min) [2P] [JIP] [chapter #1005]   fail [ ]
  - do: Switch the game to German with `"town": {"en_us": "village", "de_de": "Dorf"}`: town reads "Dorf", police "Sheriff", other text unchanged.
  - do: In English: pay a fine ticket and read the toasts, the wanted board title ("Sheriff notices"), the dispatch service card, an ID request and a robbery toast (£ before the amount).
  - already proven: £ on shop prices, "Everon Sheriff" and "Gang" on the police application and org register, and the en client ignoring a de_de-only term: client fixtures `client-term-currency`, `client-term-sheriff-gang`, `client-term-language-match`, `client-term-language-other` PASS in this revision ([#1241](https://github.com/Life-Framework/Life-Framework/pull/1241)).
  - already proven: a second client sees the same words: they ride the config stream ([#856](https://github.com/Life-Framework/Life-Framework/pull/856)). The harness cannot switch the client language, so German is only checked by you.
- [ ] #750 M3-Spec: Organisation ranks with rights bits and a persisted conflict record (~5 min) [2P] [JIP]   fail [ ]
  - Host a listen or dedicated server and join with two clients (A and B). No restart is needed except in step 6.
  - As A, found an organisation at the registry desk. The ledger shows rank "Founder".
  - A faces B and uses "Invite to <org>". B faces A and uses "Join <org>". B's ledger shows "Member".
  - Try a right you lack: as B, face A. B gets no Remove, Promote or Demote entry. At the desk, B's ledger has no Withdraw button.
  - (+4 more steps: PR #857, or `--steps`)
  - covered: LOGIC `OrgRights`, PERSISTENCE `OrgRightsPersist`, and `wt e2e m3-org-rights` with `--legacy-save` over a real restart ([#857](https://github.com/Life-Framework/Life-Framework/pull/857)), so step 6 (restart, B is still Officer) is proven. Left: the face-to-face invite, join, promote and demote entries on two clients.

## Dedicated server (`tools\cli host start`; one server start with the override config for the first four lines) (~17 min)

- [ ] #957 #1109 M3-Addons: Content packs add, replace and patch NPC dialogue by id (~2 min) [DEDICATED] [chapter #1005]   fail [ ]
  - do: Use `tools/test/config-fixtures/presets/m3-override-model-boot.json` as the server's `config.json` (one server start for the first four lines).
  - do: At the car lot the S105 costs $1,500 and the UAZ 469 is not offered. With dev tools open, `harness.mjs --client 1 reload dialogue` shows a changed clerk caption.
  - already proven: the `vehicles.vehicles.AAAABBBBCCCCDDDD: patch names an id no lower layer has` line: boot fixture `presets/m3-override-model-boot` PASS in this revision.
  - already proven: a vehicle patch reaches the client's lot sheet ($1,250 vs $1,000): client fixture `presets/client-economy-override` PASS; dialogue patch from a pack: `dialogue-proof` PASS ([#1237](https://github.com/Life-Framework/Life-Framework/pull/1237)); `reload dialogue`: `live-reload-proof` PASS ([#1056](https://github.com/Life-Framework/Life-Framework/pull/1056)).
- [ ] #962 M3-Packs: One override model for all content: add, replace, patch and remove by id (~2 min) [DEDICATED] [chapter #1005]   fail [ ]
  - do: Same config.json. Open the clothing rack: the green raincoat is not listed. As an officer open the ticket form: robbery shows a $400 fine.
  - already proven: the `crime.crimes.test-missing: patch names an id no lower layer has` line and no other crime error: boot fixture `presets/m3-override-model-boot` PASS; a removed offer is gone on the client for a shop: `client-economy-override` (M9 gone).
- [ ] #1106 M3-Packs: Jobs lists on the override model (tracks, xpAwards, licenceGates) (~2 min) [DEDICATED]   fail [ ]
  - do: Same config.json. Join and gather a resource: the FARMER XP it awards is 9 (shipped 2), `[LFDebug:Level]` lines. The `jobs:` and `GHOST: patch names an id no lower layer has` lines are written the first time jobs are used, not at server start, so read the log after the gather.
  - already proven: config merge and the three lists: LOGIC `config/jobs-override-model` and WORLD `config/jobs-pack-overlay` ([#1127](https://github.com/Life-Framework/Life-Framework/pull/1127)). The booted server shows no `jobs:` line before first use (checked in this revision).
- [ ] #1108 M3-Packs: Police, organizations, zones, licences and systems lists on the override model (~2 min) [DEDICATED]   fail [ ]
  - do: Same config.json. Join as a client: the police menu lists the patrol and senior ranks. Read the log after standing in or hitting inside a safe zone: `zones.ruleSets SAFE` is refused with "cannot be removed" (written on first zone use, not at start). SAFE zones still block player damage.
  - already proven: `police.ranks.patrol: a shipped rank cannot be removed` and `police.ranks.ghost: patch names an id no lower layer has`: boot fixture `presets/m3-override-model-boot` PASS. Not shown at boot: the zones refusal and the jobs lines.
- [ ] #283 M3-Config: Vehicles, zones, and persistence sections read through the config service (~3 min) [DEDICATED] [RESTART] [chapter #224 #1004]   fail [ ]
  - do: Buy the S105 at the lot desk (211, 262) with `vehicles.vehicles.54C3CC22DEBD57BE.basePrice` 1200 and check the charge. Add `"impoundOverride": 100`, restart, collect an impounded S105 at the desk: fee $100.
  - do: Set `basePrice` 2000000: the log shows `[LFDebug:Config]` refusing it and the lot keeps $1,000.
  - already proven: the lot sheet reads the config price on the client ($1,250 vs $1,000): client fixture `presets/client-economy-override` PASS in this revision; the owner vehicles section: LOGIC `config/vehicle-prices-owner` ([#864](https://github.com/Life-Framework/Life-Framework/pull/864), [#904](https://github.com/Life-Framework/Life-Framework/pull/904)).
- [ ] #691 M3-Patterns: Disconnect rules: cuffed, downed, wanted or jailed players (~6 min) [DEDICATED] [RESTART]   fail [ ]
  - Dedicated server, two Steam accounts, A and B.
  - A clocks in as police, downs and cuffs B, then B quits the game. Server log shows left cuffed,downed...: jail and jailed for leaving custody. B rejoins: hospital wake, then the cell with a sentence slip, wanted +1, no...
  - Repeat with a server restart between B's logout and rejoin. Same result.
  - Set "disconnect": {"cuffedRule": "fine"} and repeat: B's bank drops by 1000 and B spawns free.
  - (+2 more steps: PR #1154, PR #1159, or `--steps`)
  - covered: LOGIC `disconnect/rules`, WORLD `disconnect/jail`, PERSISTENCE `disconnect/cuffed`, `wt e2e disconnect-rules` 41/41 on both boots, and a live downed logout ([#1154](https://github.com/Life-Framework/Life-Framework/pull/1154)).
  - already proven: `session disconnect-proof` (2 clients, full profile, police roster) PASS after [#1318](https://github.com/Life-Framework/Life-Framework/pull/1318): the leaver's sentence waits offline (no `sentence served offline`, `sentence resumed` equals `sentence started on logout`) and the rejoin as the same identity goes back to the cell. The two failures of 2026-11-14 were proof drift after [#1159](https://github.com/Life-Framework/Life-Framework/pull/1159), not a regression. Still by hand: two real Steam accounts and the restart pass.

## Desk, no game needed (~18 min)

- [ ] #751 M3-Spec: Held ground: hold time, points and member minimum on the hideout record (~2 min) [desk]   fail [ ]
  - do: Read the held-ground spec (see the PR) and answer the open questions at its end.
  - already proven: nothing runs: [#1182](https://github.com/Life-Framework/Life-Framework/pull/1182) is documentation only. The old line said "tick on the PR evidence", but the PR asks for the maintainer's answers.
- [ ] #752 M3-Spec: Vehicle record fields: garage slot, tow state, inspection date, hidden compartment, cargo tuning (~2 min) [desk]   fail [ ]
  - do: Read the vehicle-state spec (see the PR) and answer the open questions at its end.
  - already proven: documentation only, [#1182](https://github.com/Life-Framework/Life-Framework/pull/1182).
- [ ] #291 M3-Docs: Config reference pages generated from the schema (~2 min) [LOOK]   fail [ ]
  - Open /config/reference on the Pages preview: check the section anchors, the tables on a phone width, and the generator and footer links.
  - covered: `npm run build` and `npm run check` pass ([#872](https://github.com/Life-Framework/Life-Framework/pull/872)). Left: the look at phone width.
- [ ] #375 M3-Release: Separate branding and narrative for core and Everon 1989 (~6 min) [LOOK]   fail [ ]
  - #375 (branding and narrative)
  - Read docs/branding/core.md and docs/branding/everon-1989.md. Sign off, or edit, the premise ("Everon, 1989. You have just arrived ...") and the voice rules.
  - Read both descriptions and summaries (docs/marketing/workshop/description.bbcode, summary.txt, and the same under everon-1989/). The core page must read neutral and send players to the pack; the pack page must read as...
  - Look at both preview.jpg files. Both are placeholders (the core one shows the village, the pack one is flat grey). Pick or commission the final art per the briefs, and decide which of the nine village screenshots go i...
  - (+8 more steps: PR #873, PR #1222, or `--steps`)
- [ ] #369 M3-Release: tools\cli release stamps versions, gates, and tags (~2 min)   fail [ ]
  - Open question: .gproj has no version field today. The tool adds Version "x.y.z" after TITLE. Before the first real release, confirm Workbench loads a .gproj with that line (stamp one by hand, open the project). If not...
  - Maintainer only, at release time: tools\cli release 0.1.0 (dry run), then --confirm, then git push origin refs/tags/v0.1.0 or --push.
  - covered: `release-cli-selftest` (stamp, gates, tag) and a `tools\cli release 0.1.0` dry run on main ([#850](https://github.com/Life-Framework/Life-Framework/pull/850), [#1226](https://github.com/Life-Framework/Life-Framework/pull/1226)). Left: the `.gproj` Version question and the real `--confirm` and tag push.
- [ ] #289 M3-Generator: Generator presets: flagship 1989, small server, hardcore economy (~2 min) [LOOK] [chapter #225 #1004]   fail [ ]
  - do: Open /config, click each preset (flagship 1989, small server, hardcore economy): the form and JSON pane fill. Set the admin password, download both files.
  - already proven: the three presets are real fixtures that pass `config-check` and the harness ([#1116](https://github.com/Life-Framework/Life-Framework/pull/1116)); the dedicated boot on the downloaded files needs the Workshop ids: see the blocked block (#370).
- [ ] #296 #967 M3-Example: Release download built from the template (~2 min) [chapter #227 #1004]   fail [ ]
  - do: Read `docs/architecture/pack-licence.md` once for tone and accuracy against your intent.
  - do: After #370: put the real pack id in `tools/release/example-zip.json`, run `example-zip --final`, unzip, `config-check config.json`, and start a server from the folder with `start.cmd`.
  - already proven: `tools\cli example-zip` writes `tmp/release/LifeFramework-ExampleServer-DRAFT.zip` with the placeholder note, and `example-zip --final` exits 1 naming `mods.pack.modId`: both run in this revision ([#1210](https://github.com/Life-Framework/Life-Framework/pull/1210)).

## Blocked until the Workshop upload (#370) (~23 min)

- [ ] #109 M1: M1-24 Playtest release: guide, Workshop, showcase (~8 min) [2P] [RESTART]   fail [ ]
  - Before Publish: read the kit in docs/marketing/workshop/ (description, summary, preview, screenshots) and sign it off. Nothing has been posted to Discord, the website, or the Workshop. Then follow docs/playtest/worksh...
  - After pressing Publish (Unlisted), on a normal game install:
  - Workshop: subscribe to "Life Framework" and wait for the download.
  - Scenarios: "Life Framework: Everon 1989" is listed (once; the Playtest Town entry is hidden since #1372) with 16 players and the Everon picture. Press Play and sign form R-89. You should stand on the square with cash.
  - (+3 more steps: PR #711, or `--steps`)
  - covered: a dedicated server plus one client from the packed addon came up clean and reached form R-89 ([#711](https://github.com/Life-Framework/Life-Framework/pull/711)); only the Workshop download and the Publish are untested.
- [ ] #1004 M3 chapter: Your town in five minutes (~3 min) [LOOK] [chapter #1004]   fail [ ]
  - Follow docs/owner/install.md on a machine with Workshop access once both items are published (#370): unzip the example server, run start.cmd, join, pass the quiz, buy at a shop.
  - On life-framework.org/config check the intro and server.json text read right.
  - No screenshots: docs and one website sentence, no in-game visual change.
  - covered: the owner walk on lf-cloud with the generator's own files, `config-check` 0 errors ([#1245](https://github.com/Life-Framework/Life-Framework/pull/1245)). Left: the real Workshop install.
- [ ] #290 #292 M3-Docs: Install guide from zero to a joinable server (~5 min) [RESTART] [chapter #226 #1004]   fail [ ]
  - also proves #292: M3-Docs: Walkthrough: add a job with prefabs and config only
  - person who did not write them, on lf-testbox:
  - Follow docs/owner/install.md from a fresh server install to a connected client.
  - Boot the example job and shop pack the walkthroughs build, buy from the counter, run a courier job, restart the server and confirm the character and stock survive.
  - That lf-testbox acceptance is still owed on #290/#292/#293.
  - covered: every config snippet in the pages passes `config-check` ([#1110](https://github.com/Life-Framework/Life-Framework/pull/1110)); the `owner-add-shop` fixture proves the shop walkthrough in game (PASS in this revision). Left: a person who did not write the pages, on lf-testbox.
- [ ] #295 M3-Example: Example server template under server/ boots core and pack (~5 min) [RESTART] [chapter #227 #1004]   fail [ ]
  - Copy server/example/ to a clean folder, set game.passwordAdmin, set LF_REFORGER_SERVER, run start.cmd (needs the mods on the Workshop).
  - Join from a client; #login <password>; check profile/logs/*/console.log for [LFDebug:Config] owner config loaded.
  - Edit a price in config.json, restart, see shops: owner config read change.
  - covered: `session up --mission template` boots the template server, loads both addons and logs `owner config loaded` ([#1166](https://github.com/Life-Framework/Life-Framework/pull/1166)), and `template-core-only` boots with one client joined ([#1171](https://github.com/Life-Framework/Life-Framework/pull/1171)). Left: the real Workshop mods through `start.cmd`.
- [ ] #370 #371 M3-Release: First Workshop upload of core and pack (maintainer) (~2 min)   fail [ ]
  - also proves #371: M3-Release: A clean dedicated server boots from the Workshop items alone
  - After the maintainer's first upload (#370): from a worktree run tools\cli workshop boot-check 4550A2B656E1B569 B1E84D5687E11724. Expect SUMMARY workshop-boot-check ok stage=pass. The client-join stage (a client starte...
  - Follow docs/release-first-upload.md steps 1 to 5 and record the page URLs and passing line in docs/setup.md.
  - covered: `workshop boot-check` ran on lf-cloud and failed at `stage=mods` exactly as expected before the upload ([#1226](https://github.com/Life-Framework/Life-Framework/pull/1226)).

## Appendix: agent-proven, spot-check only (~53 min)

Each line names the proof. Tick it from that evidence; do the one named spot-check only if you are standing there anyway.

- [ ] #1216 M3-Reshape: vehiclestate sidecar payload (#752a build) (~1 min) [RESTART]   fail [ ]
  - agent-proven: PERSISTENCE `persist/vehicle-state`, `persist/vehicle-state-legacy`, LOGIC `vehicles/state-bad-row`; `wt e2e vehiclestate` over a real restart: the garaged car comes back TOWED with its tow and inspection stamps and compartment 1 (`applied 1 of 1 vehicle state row(s)`) ([#1309](https://github.com/Life-Framework/Life-Framework/pull/1309)). No player-visible change.
  - spot-check: inside the S4 and S5 restart pass, buy a car at the lot and leave it on the street before the restart: after it, the car is at the impound desk with the same fee and text as before, and the server log shows `save loaded: version 33` (34 once #1361 is in).

- [ ] #1045 M3-Packs: Map independence: cash van route, money bag and dispatch console find places by component (~1 min)   fail [ ]
  - agent-proven: WORLD `places/cash-van`, `places/bank-bag`, `places/dispatch`, each red-proofed with the old name lookup ([#1058](https://github.com/Life-Framework/Life-Framework/pull/1058)).
  - spot-check: rob the vault once and see the bag filled.
- [ ] #1073 M3-Packs: Bank vault, heist and rob kit robbery obey the Robbery switch (~1 min)   fail [ ]
  - agent-proven: WORLD `LF_Test_SystemsWorld` and `LF_Test_SystemsEconomy` (vault, heist and rob kit refuse with Robbery off; a drill that ends after the switch goes off pays nothing) ([#1123](https://github.com/Life-Framework/Life-Framework/pull/1123), [#1100](https://github.com/Life-Framework/Life-Framework/pull/1100)); boot fixture `presets/m3-systems-off-boot` PASS.
  - spot-check: one `/configreload` mid-drill if you are in the vault anyway.
- [ ] #1005 M3 chapter: Make it yours (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: client fixtures PASS in this revision (11 of 11, [#1238](https://github.com/Life-Framework/Life-Framework/pull/1238), [#1241](https://github.com/Life-Framework/Life-Framework/pull/1241)); `pack new`, `asset dialogue`, `validate-dialogue-patch` and `pack check` ran green on a scaffold ([#1232](https://github.com/Life-Framework/Life-Framework/pull/1232), [#1234](https://github.com/Life-Framework/Life-Framework/pull/1234)). A pack in a booted world is not proven yet; the pack lines (#964, #966, #957, #958) carry that.
  - spot-check: tick when #964, #966, #957 and #958 pass.
- [ ] #322 M3-Addons: Load the LF_Everon1989 pack in build, test, serve and playtest (~1 min) [chapter #223]   fail [ ]
  - agent-proven: `session up --mission playtest` logs both addons; `test --tier all` runs with both ([#1103](https://github.com/Life-Framework/Life-Framework/pull/1103)).
  - spot-check: `tools\cli host status` names the pack header.
- [ ] #223 M3 epic: Two addons in one repo (LifeFramework core + LF_Everon1989 pack) (~1 min) [chapter #223]   fail [ ]
  - agent-proven: roll-up of its child issues ([#818](https://github.com/Life-Framework/Life-Framework/pull/818)); the epic has no step of its own.
  - spot-check: tick when the lines under it are ticked.
- [ ] #226 M3 epic: Owner docs (install, config reference, add a job or shop) (~1 min) [chapter #226 #1004]   fail [ ]
  - agent-proven: roll-up of its child issues ([#1228](https://github.com/Life-Framework/Life-Framework/pull/1228)); the epic has no step of its own.
  - spot-check: tick when the lines under it are ticked.
- [ ] #684 M3 epic: Patterns to adopt before the framework release (from Altis Life) (~1 min) [chapter #684]   fail [ ]
  - agent-proven: roll-up of its child issues ([#694](https://github.com/Life-Framework/Life-Framework/pull/694), [#863](https://github.com/Life-Framework/Life-Framework/pull/863)); the epic has no step of its own.
  - spot-check: tick when the lines under it are ticked.
- [ ] #746 M3 epic: Record shapes and shared pieces before the framework release (~1 min) [chapter #746]   fail [ ]
  - agent-proven: roll-up of its child issues ([#863](https://github.com/Life-Framework/Life-Framework/pull/863)); the epic has no step of its own.
  - spot-check: tick when the lines under it are ticked.
- [ ] #287 #288 M3-Generator: Generator validates with config-check's rules and exports only a valid file (~1 min) [chapter #225 #1004]   fail [ ]
  - agent-proven: `configgen-selftest` (`npm test` in website/): every config-check fixture gives identical messages on the page and in config-check; import and export round trip ([#1097](https://github.com/Life-Framework/Life-Framework/pull/1097)).
  - spot-check: paste `tools/validation/fixtures/config-check/bad.json` into /config once.
- [ ] #294 M3-Docs: Upgrade page per release (~1 min) [chapter #226 #1004]   fail [ ]
  - agent-proven: `upgrade-page-selftest` ([#1230](https://github.com/Life-Framework/Life-Framework/pull/1230)); `upgrade-page HEAD~40 HEAD --version 0.1.0` printed a filled page in this revision.
  - spot-check: read the config and save sections once.
- [ ] #299 M3-Harness: Harness boots a dedicated server per fixture (~1 min) [chapter #228 #1004]   fail [ ]
  - agent-proven: `tools\cli config-harness --stage boot`: 7 of 7 fixtures PASS on a fresh dedicated server each, in this revision.
  - spot-check: none beyond ticking.
- [ ] #963 M3-Packs: Pack manifest with a core version range, and a declared public surface (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: LOGIC `packs/manifest`, red-proofed ([#1088](https://github.com/Life-Framework/Life-Framework/pull/1088)). Nothing is player-visible.
  - spot-check: none.
- [ ] #297 M3-Example: The template boots with the pack swapped out (~1 min) [chapter #227 #1004]   fail [ ]
  - agent-proven: `session up --mission template-core-only --wait`: ready, 1 client joined, town spawns ([#1171](https://github.com/Life-Framework/Life-Framework/pull/1171)).
  - spot-check: none.
- [ ] #327 M3-Addons: wt e2e loads a save made before the move (~1 min) [chapter #223]   fail [ ]
  - agent-proven: `e2e --pre-move 502ebf39 --mission playtest` exit 0, pass 1 shutdown save, pass 2 passed=44 failed=0 ([#1246](https://github.com/Life-Framework/Life-Framework/pull/1246), [#1185](https://github.com/Life-Framework/Life-Framework/pull/1185)).
  - spot-check: none.
- [ ] #277 M3-Config: Inventory every config attribute and hard-coded constant (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: `config-constants-selftest` and a planted constant fails the validator ([#820](https://github.com/Life-Framework/Life-Framework/pull/820)).
  - spot-check: none.
- [ ] #280 M3-Config: Generate config.schema.json from the config classes (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: `config-schema-selftest` and `validate-config-schema` ([#848](https://github.com/Life-Framework/Life-Framework/pull/848)).
  - spot-check: none.
- [ ] #281 M3-Config: tools\cli config-check validates an owner file offline (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: `config-check-selftest`, red-proofed twice ([#854](https://github.com/Life-Framework/Life-Framework/pull/854)).
  - spot-check: none.
- [ ] #284 M3-Config: Move hard-coded tuning constants onto config classes (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: WORLD `config/constants-batch1` plus the green suite ([#875](https://github.com/Life-Framework/Life-Framework/pull/875)); defaults are today's values.
  - spot-check: none.
- [ ] #298 M3-Harness: Harness fixtures and the parse and validate stages (~1 min) [chapter #228 #1004]   fail [ ]
  - agent-proven: `config-harness-selftest`; `config-harness` parse and validate stages OK on 36 fixtures in this revision ([#1093](https://github.com/Life-Framework/Life-Framework/pull/1093)).
  - spot-check: none.
- [ ] #300 M3-Harness: Harness joins a client and checks a client-visible value (~1 min) [chapter #228 #1004]   fail [ ]
  - agent-proven: client stage: `session config-harness-proof` PASS on 11 fixtures in this revision, each owner value visible on the client ([#1169](https://github.com/Life-Framework/Life-Framework/pull/1169)).
  - spot-check: none.
- [ ] #301 M3-Harness: Config harness runs in wt gate and before every release (~1 min) [chapter #228 #1004]   fail [ ]
  - agent-proven: the harness runs in `wt gate` and `ci` ([#1180](https://github.com/Life-Framework/Life-Framework/pull/1180)); stages parse, validate, boot and client all ran in this revision.
  - spot-check: none.
- [ ] #303 M3-Upgrade: Config migrations run at boot after a backup (~1 min) [chapter #229]   fail [ ]
  - agent-proven: LOGIC `config/migration-chain` and `config/migration-file`; no shipped migration runs yet ([#852](https://github.com/Life-Framework/Life-Framework/pull/852)).
  - spot-check: none.
- [ ] #304 M3-Upgrade: config-check reports which migrations would run (~1 min) [chapter #229]   fail [ ]
  - agent-proven: `config-check docs/config/config.example.json --no-loops` printed `Migrations: ... schemaVersion 1 is this build's; none needed` in this revision ([#1174](https://github.com/Life-Framework/Life-Framework/pull/1174)).
  - spot-check: none.
- [ ] #326 M3-Addons: The core boots DebugWorld and passes the suite without the pack (~1 min) [chapter #223]   fail [ ]
  - agent-proven: `test --tier all --core-only` 500/500 with no pack on `-addonsDir` ([#1144](https://github.com/Life-Framework/Life-Framework/pull/1144)).
  - spot-check: none.
- [ ] #553 M3-Config: Packs add namespaced config sections merged into the schema (~1 min)   fail [ ]
  - agent-proven: LOGIC `config/pack-section` ([#1172](https://github.com/Life-Framework/Life-Framework/pull/1172)); `config-check` on `{"everon1989":{"festivalDay":9}}` reports `above the maximum 7`, and a server booted on the playtest mission logs no `unknown top-level key everon1989`, both in this revision. The range line itself is not logged at boot (read on use).
  - spot-check: none.
- [ ] #481 In-game credits screen (~1 min)   fail [ ]
  - agent-proven: WORLD `credits/screen`; `session credits-proof` PASS in this revision: the screen opened and a picture was captured ([#1190](https://github.com/Life-Framework/Life-Framework/pull/1190)). The row count line was not read.
  - spot-check: compare the sections with `docs/credits.md` once.
- [ ] #690 M3-Patterns: World cleanup service for abandoned vehicles, dropped items and bodies (~1 min)   fail [ ]
  - agent-proven: WORLD `cleanup/sweep` and PERSISTENCE `cleanup/persist` (owned vehicles are never swept) ([#1122](https://github.com/Life-Framework/Life-Framework/pull/1122)).
  - spot-check: none.
- [ ] #367 M3-Release: Fix Workshop item names and version scheme (~1 min)   fail [ ]
  - agent-proven: `validate-release-ready --strict` passes for both addons ([#871](https://github.com/Life-Framework/Life-Framework/pull/871), [#1222](https://github.com/Life-Framework/Life-Framework/pull/1222)).
  - spot-check: none.
- [ ] #686 M3-Patterns: No-profit-loop check across every price in config (~1 min)   fail [ ]
  - agent-proven: `profit-loop-selftest`, `validate-profit-loops` ([#851](https://github.com/Life-Framework/Life-Framework/pull/851)).
  - spot-check: none.
- [ ] #688 M3-Patterns: Vehicle prices: one base price and per-action multipliers (~1 min)   fail [ ]
  - agent-proven: LOGIC vehicle price tests ([#841](https://github.com/Life-Framework/Life-Framework/pull/841)); the lot reads the config price on a client: `client-economy-override` PASS in this revision.
  - spot-check: none.
- [ ] #368 M3-Release: validate-release-ready checks both addons for publishing (~1 min)   fail [ ]
  - agent-proven: `release-ready-selftest` plants 16 defects ([#821](https://github.com/Life-Framework/Life-Framework/pull/821)).
  - spot-check: none.
- [ ] #822 M3: validate-annex-fixtures stops new tests depending on village entities (~1 min)   fail [ ]
  - agent-proven: `annex-fixtures-selftest` ([#823](https://github.com/Life-Framework/Life-Framework/pull/823)).
  - spot-check: none.
- [ ] #761 M3-Spec: One record-shape pass before the persistent launch, with append-only save versions (~1 min)   fail [ ]
  - agent-proven: `record-shapes-selftest` and `validate-record-shapes` over 38 record classes ([#1219](https://github.com/Life-Framework/Life-Framework/pull/1219)).
  - spot-check: none.
- [ ] #969 M3-Packs: pack check and an explain view of the merged config (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: `pack check LF_Everon1989` exits 0 in this revision; the selftest covers the patch-on-missing-id report ([#1125](https://github.com/Life-Framework/Life-Framework/pull/1125)).
  - spot-check: none.
- [ ] #1075 M3-Packs: Switch the world-resource systems (Gathering, Mining, Processing, Resources, Chains, Hideouts, Ho... (~1 min)   fail [ ]
  - agent-proven: WORLD `systems/world-resources`, `systems/world-places`; PERSISTENCE `systems/keep-data`; `e2e` pass1/pass2 OK on lf-testbox ([#1120](https://github.com/Life-Framework/Life-Framework/pull/1120), [#1080](https://github.com/Life-Framework/Life-Framework/pull/1080)); boot fixture `m3-systems-off-boot` PASS.
  - spot-check: swing a pickaxe at a rock with Mining off once.
- [ ] #1076 M3-Packs: Switch the service systems (Comms phones, Chat, Medic, Police, Crime, Investigation, Vehicles, Ve... (~1 min)   fail [ ]
  - agent-proven: WORLD `LF_Test_SystemsServices`, one gate removed per system and observed red ([#1102](https://github.com/Life-Framework/Life-Framework/pull/1102)); boot fixture `m3-systems-off-boot` PASS.
  - spot-check: none.
- [ ] #960 M3-Packs: Every system can be turned on or off from config (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: WORLD `systems/world-gates`; PERSISTENCE `systems/keep-data` (a crop survives a restart with Farming off); `e2e` pass1/pass2 OK ([#1080](https://github.com/Life-Framework/Life-Framework/pull/1080)); boot fixture `m3-systems-off-boot` PASS.
  - spot-check: none.
- [ ] #961 M3-Packs: Map independence: systems find their places by component, never by coordinates or names (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: WORLD `places/*` and `places/unnamed-door`, each red-proofed against the old name lookup ([#1048](https://github.com/Life-Framework/Life-Framework/pull/1048), [#1058](https://github.com/Life-Framework/Life-Framework/pull/1058)).
  - spot-check: none.
- [ ] #1130 M3-Packs: cash slot swap after a save leaves old stacks uncounted (~1 min)   fail [ ]
  - agent-proven: WORLD `prefabs/cash-slot-swap` and `e2e --cash-swap` over a real restart ([#1186](https://github.com/Life-Framework/Life-Framework/pull/1186)).
  - spot-check: none.
- [ ] #689 M3-Patterns: Persistence switches per field for server owners (~1 min)   fail [ ]
  - agent-proven: PERSISTENCE `persistence/switches` and `wt e2e persist-switches` over a real restart ([#1188](https://github.com/Life-Framework/Life-Framework/pull/1188)).
  - spot-check: none.
- [ ] #1046 M3-Packs: validate-map-independence: no coordinates, entity or place names in core script (~1 min)   fail [ ]
  - agent-proven: `validate-map-independence` plus WORLD `places/org-hideout` ([#1048](https://github.com/Life-Framework/Life-Framework/pull/1048), [#1057](https://github.com/Life-Framework/Life-Framework/pull/1057)).
  - spot-check: none.
- [ ] #278 M3-Config: Config service merges config.json over shipped defaults, starting with shops (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: client fixtures PASS in this revision (`client-bad-field`: Apple $13 from the owner file, Water back to $6 after the bad "x"); boot fixture `m3-bad-field-boot` PASS (`owner config loaded from $profile:LifeFramework/config.json`, the `Drinks/.buyPrice` path) ([#836](https://github.com/Life-Framework/Life-Framework/pull/836)).
  - spot-check: none.
- [ ] #279 #302 M3-Config: A bad config field falls back to its default; the server stays up (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: boot fixtures `m3-bad-field-boot` (unknown top-level key, unknown nested key, `above the maximum 1000000; keeping 50`) and `broken/schema-too-new` (`written for schemaVersion 9 but this build reads up to 1`) PASS in this revision ([#842](https://github.com/Life-Framework/Life-Framework/pull/842)).
  - spot-check: none.
- [ ] #282 M3-Config: Jobs, crime, and licences sections read through the config service (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: boot fixture `epic-224-boot` (`police`, `shops`, `persistence` read, `autosave scheduled every 60`) and client fixture `epic-224-client` PASS in this revision ([#904](https://github.com/Life-Framework/Life-Framework/pull/904)). The `organizations` and `zones` sections read when first used, not at start.
  - spot-check: none.
- [ ] #285 M3-Config: Clients receive config values through a versioned stream (~1 min) [chapter #224 #1004]   fail [ ]
  - agent-proven: `config-stream-proof` on a dedicated lowspec session: $17 on the list and slip, $23 after `/configreload`, $23 for a late joiner, shipped $6 after the file is removed ([#856](https://github.com/Life-Framework/Life-Framework/pull/856), [#866](https://github.com/Life-Framework/Life-Framework/pull/866)).
  - spot-check: none.
- [ ] #692 M3-Patterns: Playtime per account and per job (~1 min)   fail [ ]
  - agent-proven: PERSISTENCE `playtime/persist`; `wt e2e m3-playtime` OK, `applied 2 of 2 playtime row(s)` on reload ([#829](https://github.com/Life-Framework/Life-Framework/pull/829)).
  - spot-check: none.
- [ ] #749 M3-Spec: Skills on job levels: one XP track per job, unlock gates per level (~1 min)   fail [ ]
  - agent-proven: PERSISTENCE `JobLevelsPersist`, WORLD `jobs/job-level-sync`, `wt e2e m3-job-levels` and `--legacy-save` OK ([#880](https://github.com/Life-Framework/Life-Framework/pull/880)).
  - spot-check: do five parcel runs once if you are at the post office.
- [ ] #965 M3-Packs: Saves keep content by stable id, with an alias table for renames (~1 min) [chapter #1005]   fail [ ]
  - agent-proven: `wt e2e --content-ids`, three real boots: alias used, missing content held, restored ([#1132](https://github.com/Life-Framework/Life-Framework/pull/1132), [#1088](https://github.com/Life-Framework/Life-Framework/pull/1088)); standard `wt e2e` 39/39 on both boots.
  - spot-check: none.
- [ ] #293 M3-Docs: Walkthrough: add a shop with prefabs and config only (~1 min) [chapter #226 #1004]   fail [ ]
  - agent-proven: client fixture `presets/owner-add-shop` PASS in this revision (`session config-harness-proof`, the command the PR asks for) ([#1242](https://github.com/Life-Framework/Life-Framework/pull/1242)).
  - spot-check: none.
- [ ] #685 M3-Patterns: Crime catalog: typed crime codes behind wanted, fines and jail (~1 min)   fail [ ]
  - agent-proven: LOGIC `crime/catalog-backcompat` (every offence's wanted, the ticket form, the sentence) and `crime/catalog-owner` ([#876](https://github.com/Life-Framework/Life-Framework/pull/876)).
  - spot-check: none.
- [ ] #286 M3-Generator: Generator page renders a form from config.schema.json (~1 min) [chapter #225 #1004]   fail [ ]
  - agent-proven: `configgen-selftest` round trip of the generated file ([#853](https://github.com/Life-Framework/Life-Framework/pull/853), [#1097](https://github.com/Life-Framework/Life-Framework/pull/1097)).
  - spot-check: open /config on the preview once.
- [ ] #372 M3-Release: Release process doc for updates and rollback (~1 min)   fail [ ]
  - agent-proven: docs and one website build-list edit ([#373](https://github.com/Life-Framework/Life-Framework/pull/373), [#1164](https://github.com/Life-Framework/Life-Framework/pull/1164)).
  - spot-check: open `/docs/release-process` and `/docs/upgrades` after the next site build.
