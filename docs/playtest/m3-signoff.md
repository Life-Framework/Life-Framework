# M3 sign-off playtest: 2026-11-14

89 issue(s) on 83 line(s), about 321 min including the 6 smoke checks; 89 open in-dev or needs-playtest in all.

Tick `[x]` at the front when the check passes; tick `fail [x]` at the end when it does not, and put the reason on a `> note:` line under it.
Leave a box empty to skip: nothing happens to that issue. Then `tools\cli playtest verdicts <this file>` prints the `gh` commands for review; it does not run them.
Tags: `[2P]` needs a second client, `[DEDICATED]` needs the dedicated server, `[RESTART]` needs a server restart, `[JIP]` needs a late joiner, `[LOOK]` is a visual only a person can judge.

**Scope.** The 89 open issues in the "M3 Framework Release" milestone that are `needs-playtest` or `in-dev` (100 are open in all; the other 11 carry neither label: epics #224, #225, #227, #228, #229 and #366, plus #374, #381 and #1216 to #1218, covered in [the release notes](../releases/m3-framework-release.md#needs-the-maintainer)). Made on main at `abd31b49` with `tools\cli playtest checklist --all --from-json`: the command has no milestone flag, so the issues, merged PRs and M3 chapter and epic sub-issues were fetched with `gh`, cut to the M3 milestone, and fed in as fixtures.

**Time.** About 321 min for everything: smoke 15, solo walk 85, two players 17, dedicated server 190, desk 14. That is the sum of per-line estimates. A line that says "same steps as #N" repeats work already done, so a real pass on one server session likely runs shorter. Nothing is cut here; if one sitting must be shorter, do the smoke list, the solo walk and the two-player block, then pick the dedicated lines under the chapters #1004 and #1005 first.

**Reading it.** 22 lines say "no manual steps in PR #N": tooling, docs and validation items. Tick them from the PR evidence; they need no game time. The dedicated-server block is where config, restart and late-joiner proof lives, so host once and work down it. #370 and #371 wait for the maintainer's Workshop upload and cannot pass before it.

After ticking: `tools\cli playtest verdicts docs/playtest/m3-signoff.md` prints the `gh` commands. The maintainer runs them; agents do not.

## Smoke list (every promotion, first; a fail here blocks promotion)

- [ ] S1 Boot. `tools\cli playtest` (or `host start` and join) reaches the world with no `SCRIPT (E)` and no `VME:` in the server console log (`tools\cli logs "SCRIPT \(E\)|VME:"` is empty).   fail [ ]
- [ ] S2 Join. A fresh profile signs the registration form and spawns in town with a character, cash, and the HUD. A second client joins the running server and spawns too.   fail [ ]
- [ ] S3 Save. Take an item and some cash (apple from the general store, a bank deposit), then wait for or force the autosave; the server log names the save and no persistence error.   fail [ ]
- [ ] S4 Restart. Stop the dedicated server cleanly and start it with `-loadSessionSave`: the world loads, no `Unable to find`, no `Wrong GUID/name`.   fail [ ]
- [ ] S5 Reload. Rejoin after the restart: same character, same cash, same item and bank balance as before the restart.   fail [ ]
- [ ] S6 Late joiner. With the first player mid-action (a half-grown plot, a downed or wanted player), a second client joins late and sees the same state, not a blank or default one.   fail [ ]

## Solo walk, in route order

### Spawn square (119, 243) (~4 min)

- [ ] #325 M3-Addons: Split the string table into core keys and pack flavour (~3 min) [chapter #223]   fail [ ]
  - Boot the playtest (tools\cli playtest): read the town board on the square. Expect "Welcome to Everon, Town Board". Start a new character: the form reads "Civil Registry of Everon, Form R-89".
  - Rename addons/LF_Everon1989/LF_Everon1989.gproj away and boot again: the board reads "Notice" and the form "Civil Registry, Form R-89"; no raw #LF- text anywhere.
  - No restart or multiplayer step is needed.

### Bank and bank street (177, 258) (~8 min)

- [ ] #1045 M3-Packs: Map independence: cash van route, money bag and dispatch console find places by component (~3 min)   fail [ ]
  - Boot DebugWorld (playtest town), wait for the cash van's dwell (300 s) at the bank with nobody within 40 m: it moves to the general store, then the fuel station, then back, and its box restocks ([LFDebug:Bank] cash va...
  - Rob the town vault with the robbery kit: the bag inside is filled with cash.
  - In the debug game mode, use *Test police call* on the dispatch console: the call is reported from the fuel till.
- [ ] #1073 M3-Packs: Bank vault, heist and rob kit robbery obey the Robbery switch (~4 min)   fail [ ]
  - Host DebugWorld with $profile:LifeFramework/config.json of {"schemaVersion":1,"systems":{"switches":{"Robbery":{"enabled":false}}}}.
  - Check the console for [LFDebug:Robbery] Robbery is off (owner config).
  - Walk to the bank vault, the cash van box and the armory store, and to the rob kit stand; start a vault drill with Robbery on first, then /configreload with it off before the fuse ends.
  - Remove the switch, /configreload, and repeat.
  - (+1 more steps: PR #1123, or `--steps`)

### General store, market and car lot (179, 244) (~5 min)

- [ ] #1005 M3 chapter: Make it yours (~4 min) [chapter #1005]   fail [ ]
  - tools\cli pack new Foo, write a tools/assets/dialogue/clerk_foo/dialogue.json with "patches": "clerk", an about_town line and a node using "options_from": "menu".
  - Run tools\cli asset dialogue clerk_foo --addon LF_Foo; expect the .conf written, the string rows added to the pack's table and the overlay line printed.
  - Run tools\cli validate; expect validate-dialogue-patch OK.
  - Not needed for players. Proof in game: session up --clients 1, node agent-box/session.mjs config-harness-proof --only presets/ gave PASS on 5 fixtures (Apple $23 vs shipped $8, M9 gone vs shipped $500, S105 $1,250 vs...
  - (+2 more steps: PR #1231, PR #1232, PR #1234, PR #1238, PR #1239, PR #1241, or `--steps`)

### Police station, jail and hospital (240, 242) (~6 min)

- [ ] #1128 M3-Packs: move the remaining hard prefab paths onto prefab slots (~2 min)   fail [ ]
  - Smoke only: in DebugWorld hand over a fine ticket, buy from a store (receipt), drop an item (evidence bag via the locker flow), place a sign and a wanted-poster board. All look as before.
- [ ] #754 M3-Spec: Police career schema: ranks, pay and qualifications as config (~3 min)   fail [ ]
  - Join as police, clock in at the station desk, draw the kit (M9 for patrol).
  - Promote: a second on-duty senior officer promotes you, or accrue 3600 s on duty. Re-draw after clocking out and in: the M16A2 comes out, and the ticket form's fine cap rises from 1000 to 5000.
  - Clock out: shift pay matches the on-duty seconds times 0.5.
  - Change a rank row in LF_Police.conf (e.g. patrol fine cap) and see the ticket cap follow. No restart proof needed; the rank int is the shipped one.

### Registry desk, hideout and barn (302, 288) (~2 min)

- [ ] #751 M3-Spec: Held ground: hold time, points and member minimum on the hideout record (~1 min)   fail [ ]
  - no manual steps in PR #1182: confirm it in play, or tick on the PR evidence

### Chop shop and moonshine (322, 300) (~6 min)

- [ ] #969 M3-Packs: pack check and an explain view of the merged config (~2 min) [chapter #1005]   fail [ ]
  - No game needed. On a pack you author: tools\cli pack check <addon> should exit 0 on a clean pack; add m_sOp "patch" on a made-up id to an overlay and it should report ids <file> <list> <id>: patch on an id no lower la...
- [ ] #322 M3-Addons: Load the LF_Everon1989 pack in build, test, serve and playtest (~3 min) [chapter #223]   fail [ ]
  - In a worktree, run tools\cli host status, then tools\cli playtest. Your existing host save is on LF_PlaytestTown, so the host line should read continuing the latest save ...; mission {1054CC8646357708}Missions/LF_Play...
  - Fresh header: in PowerShell, $env:LF_HOST_DIR="$env:TEMP\lf-host-pack"; tools\cli playtest. The host line should read no save yet ...; mission {C638C03B775C31BB}Missions/LF_Everon1989.conf. The client joins the town....
  - Optional, the scenario list: start the game with -addonsDir <clone>\addons -addons LifeFramework,LF_Everon1989. Both "Life Framework: Playtest Town" and "Life Framework: Everon 1989" should be listed.
  - No restart is needed beyond step 1, which continues the existing save.

### Anywhere in town (inventory, HUD, vehicles, items) (~54 min)

- [ ] #223 M3 epic: Two addons in one repo (LifeFramework core + LF_Everon1989 pack) (~1 min) [chapter #223]   fail [ ]
  - no manual steps in PR #818: confirm it in play, or tick on the PR evidence
- [ ] #226 M3 epic: Owner docs (install, config reference, add a job or shop) (~1 min) [chapter #226 #1004]   fail [ ]
  - no manual steps in PR #1228: confirm it in play, or tick on the PR evidence
- [ ] #684 M3 epic: Patterns to adopt before the framework release (from Altis Life) (~1 min) [chapter #684]   fail [ ]
  - no manual steps in PR #694, #863: confirm it in play, or tick on the PR evidence
- [ ] #746 M3 epic: Record shapes and shared pieces before the framework release (~1 min) [chapter #746]   fail [ ]
  - no manual steps in PR #863: confirm it in play, or tick on the PR evidence
- [ ] #287 #288 M3-Generator: Generator validates with config-check's rules and exports only a valid file (~2 min) [chapter #225 #1004]   fail [ ]
  - also proves #288: M3-Generator: Import an existing config.json into the generator
  - Open /config, paste a broken config.json (for instance tools/validation/fixtures/config-check/bad.json), import it: errors are marked per field, the problem list matches tools\cli config-check, Download is disabled. F...
- [ ] #291 M3-Docs: Config reference pages generated from the schema (~2 min) [chapter #226 #1004]   fail [ ]
  - Open /config/reference on the Pages preview: check the section anchors, the tables on a phone width, and the generator and footer links.
- [ ] #294 M3-Docs: Upgrade page per release (~2 min) [chapter #226 #1004]   fail [ ]
  - Tooling only, no in-game steps. Run tools\cli upgrade-page <older ref> HEAD --version 0.1.0 in a worktree and check the config and save sections read sensibly.
- [ ] #299 M3-Harness: Harness boots a dedicated server per fixture (~2 min) [chapter #228 #1004]   fail [ ]
  - Run tools\cli config-harness --stage boot from a worktree; expect two PASS lines and config-harness boot: OK.
- [ ] #963 M3-Packs: Pack manifest with a core version range, and a declared public surface (~2 min) [chapter #1005]   fail [ ]
  - One [LFDebug:Packs] pack '...' refused: it needs core ... line and no pack overlays applied. Follow-ups: the CORE_VERSION constant is not yet stamped by tools\cli release; the surface list covers config classes only u...
- [ ] #297 M3-Example: The template boots with the pack swapped out (~3 min) [chapter #227 #1004]   fail [ ]
  - Copy server/example, delete the second game.mods entry, set scenarioId to {1054CC8646357708}Missions/LF_PlaytestTown.conf, run start.cmd.
  - Join with a client that has only the core: the join succeeds and the town spawns.
  - No screenshots: server config and tooling, nothing a player sees changes.
- [ ] #327 M3-Addons: wt e2e loads a save made before the move (~3 min) [chapter #223]   fail [ ]
  - No in-game steps; this is tooling and test-gating only.
  - In a worktree, run tools\cli e2e --pre-move 502ebf39 --mission playtest.
  - Expect pass 1 shutdown save written after the host stop, then pass 2 passed=44 failed=0. On a machine whose rdb refresh fails, expect the retry message naming the stale resource database, not a server crash.
- [ ] #277 M3-Config: Inventory every config attribute and hard-coded constant (~1 min) [chapter #224 #1004]   fail [ ]
  - no manual steps in PR #820: confirm it in play, or tick on the PR evidence
- [ ] #280 M3-Config: Generate config.schema.json from the config classes (~1 min) [chapter #224 #1004]   fail [ ]
  - no manual steps in PR #848: confirm it in play, or tick on the PR evidence
- [ ] #281 M3-Config: tools\cli config-check validates an owner file offline (~1 min) [chapter #224 #1004]   fail [ ]
  - no manual steps in PR #854: confirm it in play, or tick on the PR evidence
- [ ] #284 M3-Config: Move hard-coded tuning constants onto config classes (~1 min) [chapter #224 #1004]   fail [ ]
  - no manual steps in PR #875: confirm it in play, or tick on the PR evidence
- [ ] #298 M3-Harness: Harness fixtures and the parse and validate stages (~1 min) [chapter #228 #1004]   fail [ ]
  - no manual steps in PR #1093: confirm it in play, or tick on the PR evidence
- [ ] #300 M3-Harness: Harness joins a client and checks a client-visible value (~1 min) [chapter #228 #1004]   fail [ ]
  - no manual steps in PR #1169: confirm it in play, or tick on the PR evidence
- [ ] #301 M3-Harness: Config harness runs in wt gate and before every release (~1 min) [chapter #228 #1004]   fail [ ]
  - no manual steps in PR #1180: confirm it in play, or tick on the PR evidence
- [ ] #303 M3-Upgrade: Config migrations run at boot after a backup (~1 min) [chapter #229]   fail [ ]
  - no manual steps in PR #852: confirm it in play, or tick on the PR evidence
- [ ] #304 M3-Upgrade: config-check reports which migrations would run (~1 min) [chapter #229]   fail [ ]
  - no manual steps in PR #1174: confirm it in play, or tick on the PR evidence
- [ ] #326 M3-Addons: The core boots DebugWorld and passes the suite without the pack (~1 min) [chapter #223]   fail [ ]
  - no manual steps in PR #1144: confirm it in play, or tick on the PR evidence
- [ ] #369 M3-Release: tools\cli release stamps versions, gates, and tags (~2 min)   fail [ ]
  - Open question: .gproj has no version field today. The tool adds Version "x.y.z" after TITLE. Before the first real release, confirm Workbench loads a .gproj with that line (stamp one by hand, open the project). If not...
  - Maintainer only, at release time: tools\cli release 0.2.0 (dry run), then --confirm, then git push origin refs/tags/v0.2.0 or --push.
- [ ] #553 M3-Config: Packs add namespaced config sections merged into the schema (~2 min)   fail [ ]
  - Put { "everon1989": { "festivalDay": 9 } } in $profile:LifeFramework/config.json on a server running LF_Everon1989; tools\cli config-check shows everon1989.festivalDay = 9 is above the maximum 7. Boot and confirm the...
  - Remove the pack from -addons; the same key warns as an unknown top-level key.
- [ ] #481 In-game credits screen (~3 min)   fail [ ]
  - Open DebugWorld, spawn in, type /credits in chat.
  - A paper sheet shows "Credits", "Page 1 of N" and the first section; Next and Previous turn the page, wrapping at the ends.
  - Compare the sections and rows with docs/credits.md / the website /credits page. Close shuts it. No restart needed.
- [ ] #690 M3-Patterns: World cleanup service for abandoned vehicles, dropped items and bodies (~3 min)   fail [ ]
  - Host the playtest town with {"schemaVersion":1,"cleanup":{"sweepIntervalSeconds":10,"rules":{"item":{"maxAgeSeconds":20}}}} in $profile:LifeFramework/config.json.
  - Drop an item, walk over 50 m away, wait about 30 s. Check an owned car you bought stays put.
  - The item is gone and [LFDebug:Cleanup] removed item ... is logged; the owned car and an item next to you stay. No restart needed.
- [ ] #375 M3-Release: Separate branding and narrative for core and Everon 1989 (~6 min)   fail [ ]
  - #375 (branding and narrative)
  - Read docs/branding/core.md and docs/branding/everon-1989.md. Sign off, or edit, the premise ("Everon, 1989. You have just arrived ...") and the voice rules.
  - Read both descriptions and summaries (docs/marketing/workshop/description.bbcode, summary.txt, and the same under everon-1989/). The core page must read neutral and send players to the pack; the pack page must read as...
  - Look at both preview.jpg files. Both are placeholders (the core one shows the village, the pack one is flat grey). Pick or commission the final art per the briefs, and decide which of the nine village screenshots go i...
  - (+8 more steps: PR #873, PR #1222, or `--steps`)
- [ ] #367 M3-Release: Fix Workshop item names and version scheme (~1 min)   fail [ ]
  - no manual steps in PR #871: confirm it in play, or tick on the PR evidence
- [ ] #686 M3-Patterns: No-profit-loop check across every price in config (~1 min)   fail [ ]
  - no manual steps in PR #851: confirm it in play, or tick on the PR evidence
- [ ] #688 M3-Patterns: Vehicle prices: one base price and per-action multipliers (~1 min)   fail [ ]
  - no manual steps in PR #841: confirm it in play, or tick on the PR evidence
- [ ] #752 M3-Spec: Vehicle record fields: garage slot, tow state, inspection date, hidden compartment, cargo tuning (~1 min)   fail [ ]
  - no manual steps in PR #1182: confirm it in play, or tick on the PR evidence
- [ ] #761 M3-Spec: One record-shape pass before the persistent launch, with append-only save versions (~1 min)   fail [ ]
  - no manual steps in PR #1219: confirm it in play, or tick on the PR evidence
- [ ] #368 M3-Release: validate-release-ready checks both addons for publishing (~1 min)   fail [ ]
  - no manual steps in PR #821: confirm it in play, or tick on the PR evidence
- [ ] #822 M3: validate-annex-fixtures stops new tests depending on village entities (~1 min)   fail [ ]
  - no manual steps in PR #823: confirm it in play, or tick on the PR evidence

## Two players (second client on this machine or LAN) (~17 min)

- [ ] #755 M3-Spec: Zone rule sets as data: lawless, event and safe overrides per zone (~5 min) [2P]   fail [ ]
  - Lawless HUD (the quarry, centre 178 0 172, radius 32; docs/playtest.md):
  - Start the playtest town and walk into the quarry: the red LAWLESS AREA tag shows and an enter toast fires.
  - Walk out: tag and tag toast go, exit toast fires.
  - Walk in and out a second time to confirm it re-triggers. No restart needed.
  - (+1 more steps: PR #826, or `--steps`)
- [ ] #1074 M3-Packs: Switch the economy systems (Banking, ATM, Shop, Trader, Jobs, Level, License, Whitelist, Organiza... (~5 min) [2P]   fail [ ]
  - Host DebugWorld with $profile:LifeFramework/config.json {"schemaVersion":1,"systems":{"switches":{"Robbery":{"enabled":false},"Shop":{"enabled":false},"ATM":{"enabled":false},"Jobs":{"enabled":false},"Organizations":{...
  - Read the console for one [LFDebug:<Id>] ... is off line per system.
  - Walk to the bank vault, rob kit stand, a shop counter, the ATM, the job depot, the registry desk and a trader. Join a second client late and repeat.
  - Remove the switches, /configreload, and check they all come back. Put Whitelist off and join with an unlisted account.
  - (+1 more steps: PR #1100, or `--steps`)
- [ ] #964 M3-Packs: Prefab slots: systems ask config for the prefab, packs swap the look (~5 min) [2P] [chapter #1005]   fail [ ]
  - Make a test overlay: a .conf with root LF_PrefabSlotsConfig holding LF_PrefabSlot { m_sOp "patch" m_sSlot "cash" m_Prefab "<another item prefab with InventoryItemComponent and LF_QuantityComponent>" } and the same for...
  - Host DebugWorld. The console shows [LFDebug:PrefabSlots] slot 'cash' -> ... (pack) and the same for handcuffs_worn.
  - Earn or withdraw cash: the new prefab lands in the inventory, and the HUD balance and paying in a shop still work.
  - Cuff a second player: the new model shows on the wrist and uncuffing removes it.
  - (+1 more steps: PR #1139, or `--steps`)

## Dedicated server: restart and late joiner (`tools\cli host start`, stop, start with `-loadSessionSave`, rejoin) (~190 min)

- [ ] #957 #1109 M3-Addons: Content packs add, replace and patch NPC dialogue by id (~6 min) [2P] [DEDICATED] [chapter #1005]   fail [ ]
  - also proves #1109: M3-Packs: Vehicle prices and dialogue patches on the override model
  - Dedicated server (a pack is not loaded until M3, so use the owner file for vehicles):
  - In $profile:LifeFramework/config.json put {"schemaVersion":1,"vehicles":{"vehicles":{"C8B16ABCA57679A2":{"op":"remove"},"54C3CC22DEBD57BE":{"op":"patch","basePrice":1500},"AAAABBBBCCCCDDDD":{"op":"patch","basePrice":5...
  - Start the server. In console.log expect [LFDebug:Config] vehicles.vehicles AAAABBBBCCCCDDDD: patch names an id no lower layer has and no other vehicle error.
  - At the car lot the S105 costs 1500 and the UAZ 469 is not offered.
  - (+4 more steps: PR #1134, PR #1162, or `--steps`)
- [ ] #323 M3-Addons: Move pack resources into LF_Everon1989 in batches (~9 min) [2P] [RESTART] [chapter #223]   fail [ ]
  - Playtest town: take a pickaxe from the miner stand, holster it, open the inventory. Mining a rock with the pickaxe in hand still works as before.
  - On the playtest town, make a new character: it still owns one garaged and one impounded car.
  - Optional: node agent-box/session.mjs up --mission playtest, then journey; the stash and garage steps should still pass.
  - In Workbench, open DebugWorld. The village (square, police station, car lot, quarry with the mining sign) shows as before; its layers are listed under the Everon1989Town SubScene.
  - (+20 more steps: PR #843, PR #844, PR #845, PR #846, PR #847, PR #849, PR #955, PR #1112, PR #1185, PR #1189, PR #1200, PR #1203, PR #1211, PR #1237, or `--steps`)
- [ ] #693 M3-Patterns: Every client request is checked on the server: an audit and a lint (~6 min) [DEDICATED]   fail [ ]
  - Spam a shop buy (DebugWorld, or a dedicated server with one client):
  - Open any shop, pick an item and click Buy about 10 times as fast as you can.
  - The first 4 buys go through. The slip then reads "One order at a time. Give the till a moment." and the further clicks charge nothing.
  - The server log has one [LFDebug:RateLimit] shop trade refused: player N is over the PURCHASE limit line, then one ... more request(s) refused in the last window line on the next buy after 2 s.
  - (+8 more steps: PR #824, PR #834, or `--steps`)
- [ ] #1075 M3-Packs: Switch the world-resource systems (Gathering, Mining, Processing, Resources, Chains, Hideouts, Ho... (~6 min) [RESTART]   fail [ ]
  - same steps as #1074
  - Read the console: one [LFDebug:<Id>] <Id> is off (owner config) ... line per system.
  - Walk the town: gather piles, the sawmill and lathe, the pub radio (about 122, 287), the benches, a locked house door, and swing a pickaxe at an ore rock.
  - Open the emote radial (hold F1). Check hunger and thirst stay put for a minute.
  - Remove the file, run /configreload, repeat step 3 and 4. Restart with a saved lease and a spent rock to check they survive.
  - (+2 more steps: PR #1120, or `--steps`)
- [ ] #109 M1: M1-24 Playtest release: guide, Workshop, showcase (~8 min) [2P] [RESTART]   fail [ ]
  - Before Publish: read the kit in docs/marketing/workshop/ (description, summary, preview, screenshots) and sign it off. Nothing has been posted to Discord, the website, or the Workshop. Then follow docs/playtest/worksh...
  - After pressing Publish (Unlisted), on a normal game install:
  - Workshop: subscribe to "Life Framework" and wait for the download.
  - Scenarios: "Life Framework: Playtest Town" is listed with 16 players and the town picture. Press Play and sign form R-89. You should stand on the square with cash.
  - (+3 more steps: PR #711, or `--steps`)
- [ ] #960 M3-Packs: Every system can be turned on or off from config (~10 min) [2P] [RESTART] [JIP] [chapter #1005]   fail [ ]
  - Host DebugWorld with $profile:LifeFramework/config.json containing {"schemaVersion":1,"systems":{"switches":{"Robbery":{"enabled":false},"Pager":{"enabled":false},"Farming":{"enabled":false}}}}.
  - Read the server console: look for one [LFDebug:Robbery] Robbery is off (owner config) ... line, one each for Pager and Farming, and [LFDebug:Systems] systems: 53 of 56 on; off: Farming, Robbery, Pager.
  - Join as police (or go on duty): check the kit.
  - Walk to the fuel station attendant with a weapon drawn, and to any robbery prop. Walk to the farm plots.
  - (+9 more steps: PR #1080, or `--steps`)
- [ ] #961 M3-Packs: Map independence: systems find their places by component, never by coordinates or names (~7 min) [RESTART] [chapter #1005]   fail [ ]
  - Host DebugWorld (tools\cli serve or Workbench play).
  - Rob the bank vault on the street with a charge. The vault door opens, police get the alert at the vault, and bags spill at the vault.
  - Breach Roadmap_Breach_Door. It opens; repair it as police and it locks.
  - Jail a suspect, then breach the jail door. The sentence pauses while the door is open.
  - (+3 more steps: PR #1048, or `--steps`)
- [ ] #966 M3-Packs: Packs override the UI theme (colours, fonts, paper texture) (~7 min) [RESTART] [JIP] [chapter #1005]   fail [ ]
  - Build a pack overlay .conf of class LF_UIThemeConfig (copy Configs/Tests/LF_Test_UIThemePack.conf, give each record its own GUID) and list it in the mission header's m_aLF_ConfigOverlays.
  - Join the server, open the ATM, the wanted card and the org ledger: each should follow the pack's palette. Check the typed blank (stays beige; known), button focus, and the Notice sign card accent.
  - Set m_PaperTexture on paper to a real .edds: the sheet should show it tinted by the paper colour. Leave it empty: the flat sheet.
  - Misspell a colour (#ZZZZZZ): [LFDebug:UITheme] logs once and that token takes the 1989 colour; the sheet still opens.
  - (+1 more steps: PR #1138, or `--steps`)
- [ ] #691 M3-Patterns: Disconnect rules: cuffed, downed, wanted or jailed players (~6 min) [RESTART]   fail [ ]
  - Dedicated server, two Steam accounts, A and B.
  - A clocks in as police, downs and cuffs B, then B quits the game. Server log shows left cuffed,downed...: jail and jailed for leaving custody. B rejoins: hospital wake, then the cell with a sentence slip, wanted +1, no...
  - Repeat with a server restart between B's logout and rejoin. Same result.
  - Set "disconnect": {"cuffedRule": "fine"} and repeat: B's bank drops by 1000 and B spawns free.
  - (+2 more steps: PR #1154, PR #1159, or `--steps`)
- [ ] #1130 M3-Packs: cash slot swap after a save leaves old stacks uncounted (~4 min) [RESTART]   fail [ ]
  - Optional, on a server with a pack that changes the cash look: carry cash, restart onto the pack, confirm the balance (HUD, ATM deposit) is unchanged; spend some and confirm both looks drain without losing money.
- [ ] #689 M3-Patterns: Persistence switches per field for server owners (~5 min) [RESTART]   fail [ ]
  - Host a dedicated server with a save. Give a character cash on hand, a warrant and a licence.
  - Stop the server (host stop), add "persistence": {"keepCash": false, "keepWanted": false} to $profile:LifeFramework/config.json, start it again with -loadSessionSave.
  - Rejoin: the cash on hand is gone (bank balance unchanged), the warrant and its posters are gone, the licence is still held. The log shows [LFDebug:Persistence] keepCash off: ... and keepWanted off: ....
  - Remove the two keys and restart again: nothing else changes (the save after step 3 holds the defaults).
- [ ] #1077 M3-Config: Member initializers beat .conf values on LF_ZoneRuleSet (pack overlays would be ignored) (~6 min) [RESTART]   fail [ ]
  - In Workbench, open Worlds/CoreOnEveron/CoreOnEveron.ent (vanilla Everon, core only) and press play, or host it on a dedicated server with -server Worlds/CoreOnEveron/CoreOnEveron.ent -addons LifeFramework.
  - Check the server console. It should show the four no <Kind> placed on this map: off lines, with no VME and no LF SCRIPT (E).
  - Spawn as police. Aim at a wanted player: no arrest action is offered.
  - Place a prefab with LF_JailCellComponent (and an ATM) on Everon. Restart play. The Jail off line is gone, and arrest and banking work there with no script change.
  - (+1 more steps: PR #1113, PR #1124, or `--steps`)
- [ ] #1047 M3-Packs: Core boots on vanilla Everon; a system with no place logs once and stays off (~5 min) [RESTART]   fail [ ]
  - same steps as #1077
  - Host Worlds/CoreOnEveron/CoreOnEveron.ent (-server Worlds/CoreOnEveron/CoreOnEveron.ent -addons LifeFramework) and read the console: no already registered and no needs a entity catalog manager lines.
  - Open DebugWorld in Workbench, play, and shoot a building or destructible prop: it still breaks (the destruction manager now comes from the layer, not the game-mode child).
  - A restart is not needed.
- [ ] #290 #292 M3-Docs: Install guide from zero to a joinable server (~5 min) [RESTART] [chapter #226 #1004]   fail [ ]
  - also proves #292: M3-Docs: Walkthrough: add a job with prefabs and config only
  - person who did not write them, on lf-testbox:
  - Follow docs/owner/install.md from a fresh server install to a connected client.
  - Boot the example job and shop pack the walkthroughs build, buy from the counter, run a courier job, restart the server and confirm the character and stock survive.
  - That lf-testbox acceptance is still owed on #290/#292/#293.
- [ ] #278 M3-Config: Config service merges config.json over shipped defaults, starting with shops (~6 min) [RESTART] [chapter #224 #1004]   fail [ ]
  - Host a dedicated server on this build. Put this in <profile>/profile/LifeFramework/config.json: {"schemaVersion":1,"shops":{"priceRules":{"Prefabs/Items/Food/":{"buyPrice":13}}}} Restart the server.
  - The server log shows [LFDebug:Config] owner config loaded from $profile:LifeFramework/config.json and shops: owner config read 1 fields, rejected 0.
  - Join, carry at least 13 cash, and buy one apple at the general store price list. The server takes 13 cash, not 8. Until #285, the list itself still shows 8.
  - Change the file to "buyPrice":"x" and restart. The log names shops.priceRules.Prefabs/Items/Food/.buyPrice and an apple costs 8 again.
  - (+1 more steps: PR #836, PR #865, or `--steps`)
- [ ] #283 M3-Config: Vehicles, zones, and persistence sections read through the config service (~5 min) [RESTART] [chapter #224 #1004]   fail [ ]
  - same steps as #282
  - Put {"schemaVersion":1,"vehicles":{"vehicles":{"54C3CC22DEBD57BE":{"basePrice":1200}}}} in $profile:LifeFramework/config.json, restart the server.
  - Join, open the car lot desk sheet (211, 262): the S105 reads $1,200 (was $1,000); buy it and check the charge.
  - Add "impoundOverride": 100 to that record, restart, collect an impounded S105 at the desk: fee $100.
  - Set "basePrice": 2000000: the log shows [LFDebug:Config] refusing it and the lot keeps $1,000.
- [ ] #293 M3-Docs: Walkthrough: add a shop with prefabs and config only (~4 min) [RESTART] [chapter #226 #1004]   fail [ ]
  - same steps as #290
  - tools\cli session config-harness-proof --only owner-add-shop: expect PASS presets/owner-add-shop (2 client read(s)).
- [ ] #285 M3-Config: Clients receive config values through a versioned stream (~9 min) [2P] [RESTART] [JIP] [chapter #224 #1004]   fail [ ]
  - Host a dedicated server on this build. Put this in <server profile>/profile/LifeFramework/config.json: {"schemaVersion":1,"shops":{"priceRules":{"Prefabs/Items/Drinks/":{"buyPrice":17}}}} Start the server. The log sho...
  - Join with a client. At the general store counter, search Water. The price list shows $17 (shipped: $6). Put one on the order slip: it reads Pay $17 cash. Pay; the server takes 17.
  - As an administrator, change the file to 23 and type /configreload in chat. Reopen the price list on the connected client: it shows $23, and the slip shows $23.
  - Join with a second client (late joiner): its price list shows $23.
  - (+3 more steps: PR #856, PR #866, or `--steps`)
- [ ] #1076 M3-Packs: Switch the service systems (Comms phones, Chat, Medic, Police, Crime, Investigation, Vehicles, Ve... (~6 min) [RESTART]   fail [ ]
  - same steps as #1073
  - Check the console for one [LFDebug:<Id>] <Id> is off (owner config) line per system.
  - Walk to the car lot and garage, the quest giver, the police desk and a payphone; try a parked, locked car.
  - Remove the switches, run /configreload as an admin, and repeat.
  - Restart with Vehicles on again.
  - (+1 more steps: PR #1102, or `--steps`)
- [ ] #749 M3-Spec: Skills on job levels: one XP track per job, unlock gates per level (~6 min) [RESTART]   fail [ ]
  - Host DebugWorld (dedicated server if you can) and join. Do 5 parcel runs at the post office counter. The clerk now offers fuel delivery, the same as before this change. The server log shows [LFDebug:JobLevels] +100 DE...
  - Play the fruit catcher once. The log shows +N FARMER xp ... from fruit_pick. Gather at a node: +2 FARMER xp ... from gather.
  - Restart the server (tools\cli host stop, then host again with the same save). The clerk still offers fuel delivery. tools\cli logs "job xp row" shows applied N of N job xp row(s) at boot.
  - Load an old save: copy a playthrough written by a build before this PR (version 27 or older) and continue it. The log shows save version N has no job xp rows: job levels migrated .... A courier who had fuel or taxi ac...
  - (+2 more steps: PR #880, or `--steps`)
- [ ] #282 M3-Config: Jobs, crime, and licences sections read through the config service (~4 min) [RESTART] [chapter #224 #1004]   fail [ ]
  - Write a $profile:LifeFramework/config.json with {"schemaVersion":1,"police":{"arrestReach":8},"organizations":{"foundingFee":900},"zones":{"ruleSets":{"SAFE":{"fineMultiplier":2}}}} on a dedicated server, join with a...
  - Set persistence.autosaveInterval to 60, restart; expect autosave scheduled every 60 seconds.
- [ ] #962 M3-Packs: One override model for all content: add, replace, patch and remove by id (~4 min) [DEDICATED] [chapter #1005]   fail [ ]
  - Dedicated server, no restart of the client needed:
  - Put this in $profile:LifeFramework/config.json on the server: json { "schemaVersion": 1, "crime": { "crimes": { "#LF-Offence_Robbery": { "op": "patch", "fine": 400 }, "test-missing": { "op": "patch", "fine": 1 } } },...
  - Start the server. In console.log expect [LFDebug:Config] crime.crimes.test-missing: patch names an id no lower layer has (...config.json); record skipped and no other config error.
  - Join as a client and open the clothing rack: the green raincoat is not listed (the server removed it and the client's list follows the stream).
  - (+1 more steps: PR #1114, or `--steps`)
- [ ] #958 M3-Addons: Terms layer: packs and owners rename the framework's terms, in every language (~9 min) [2P] [RESTART] [JIP] [chapter #1005]   fail [ ]
  - Add "terms": {"police": "Sheriff", "town": {"en_us": "village", "de_de": "Dorf"}, "currency": {"sym": "£"}} to $profile:LifeFramework/config.json, restart (or /configreload).
  - In English: pay a fine ticket and read the toasts, open the wanted board (title says "Sheriff notices"), the dispatch service card, an ID request, and a robbery toast (£ before the amount); shop prices read £.
  - Switch the game to German: town reads "Dorf", police "Sheriff", other text unchanged.
  - Join as a second client (or late join): it shows the same words (they ride the config stream).
  - (+3 more steps: PR #1142, or `--steps`)
- [ ] #1108 M3-Packs: Police, organizations, zones, licences and systems lists on the override model (~3 min) [DEDICATED]   fail [ ]
  - Dedicated server:
  - Start the server. In console.log expect [LFDebug:Config] lines refusing police.ranks patrol (shipped rank), police.ranks ghost (no lower layer has it) and zones.ruleSets SAFE (cannot be removed), and no other config e...
  - Join as a client and confirm the police menu still lists the patrol and senior ranks.
- [ ] #692 M3-Patterns: Playtime per account and per job (~6 min) [RESTART]   fail [ ]
  - Dedicated server, one client, a restart is needed:
  - tools\cli host start, join, play about 2 min as a civilian, then clock in at the police duty desk and stay on duty about 2 min.
  - tools\cli logs Playtime: expect <uid> starts counting time as CIVILIAN, then ... as POLICE.
  - tools\cli host stop (shutdown save), tools\cli host start again (it loads the save) and look for [LFDebug:Persistence] applied N of N playtime row(s) with N >= 2.
  - (+1 more steps: PR #829, or `--steps`)
- [ ] #750 M3-Spec: Organisation ranks with rights bits and a persisted conflict record (~9 min) [2P] [RESTART] [JIP]   fail [ ]
  - Host a listen or dedicated server and join with two clients (A and B). No restart is needed except in step 6.
  - As A, found an organisation at the registry desk. The ledger shows rank "Founder".
  - A faces B and uses "Invite to <org>". B faces A and uses "Join <org>". B's ledger shows "Member".
  - Try a right you lack: as B, face A. B gets no Remove, Promote or Demote entry. At the desk, B's ledger has no Withdraw button.
  - (+4 more steps: PR #857, or `--steps`)
- [ ] #1046 M3-Packs: validate-map-independence: no coordinates, entity or place names in core script (~4 min) [RESTART]   fail [ ]
  - Optional: in the DebugWorld town, open the hideout (door, machine, stash) and confirm it still belongs to the same organization after a restart; the layer values are unchanged so nothing should differ.
- [ ] #279 #302 M3-Config: A bad config field falls back to its default; the server stays up (~5 min) [RESTART] [chapter #224 #1004]   fail [ ]
  - also proves #302: M3-Upgrade: schemaVersion in the owner file, with too-new and too-old handling
  - On a dedicated server on this build, write <profile>/profile/LifeFramework/config.json: {"schemaVersion":1,"shop":{},"shops":{"defaultBuyPrice":5000000,"defaultSellPrise":9,"priceRules":{"Prefabs/Items/Food/":{"buyPri...
  - The log shows unknown top-level key shop; ignored, shops.defaultSellPrise is not a known key; ignored and shops.defaultBuyPrice = 5000000 is above the maximum 1000000; keeping 50. The server is joinable and an apple c...
  - Change schemaVersion to 2 and restart: one LF_Debug.Error says the file is written for schemaVersion 2 and the apple still costs 13.
- [ ] #289 M3-Generator: Generator presets: flagship 1989, small server, hardcore economy (~2 min) [DEDICATED] [chapter #225 #1004]   fail [ ]
  - Open /config, click each preset: the form and the JSON pane fill. Set the admin password, download both files.
  - Boot a dedicated server on the downloaded server.json + config.json with the Workshop items published (needs the #370 ids live).
- [ ] #295 M3-Example: Example server template under server/ boots core and pack (~5 min) [RESTART] [chapter #227 #1004]   fail [ ]
  - Copy server/example/ to a clean folder, set game.passwordAdmin, set LF_REFORGER_SERVER, run start.cmd (needs the mods on the Workshop).
  - Join from a client; #login <password>; check profile/logs/*/console.log for [LFDebug:Config] owner config loaded.
  - Edit a price in config.json, restart, see shops: owner config read change.
- [ ] #965 M3-Packs: Saves keep content by stable id, with an alias table for renames (~5 min) [RESTART] [chapter #1005]   fail [ ]
  - Dedicated server, real restart. Nothing here can be seen without a pack rename, so this checks only that normal saves are unchanged:
  - tools\cli host start on a profile with an existing save. Join, put a few items in your backpack, then tools\cli host stop.
  - tools\cli host start again and join. Every item should be back where you left it. The console should have no save content missing or save alias line, and should end the load with save loaded: version 32, ....
  - Optional, for a pack author: add an LF_SaveAliasesConfig file mapping an item's GUID to another item, list it in the mission header's config overlays, and restart. The item should come back as the new one, with one sa...
- [ ] #1106 M3-Packs: Jobs lists on the override model (tracks, xpAwards, licenceGates) (~5 min) [RESTART]   fail [ ]
  - Dedicated server, no client needed:
  - In $profile:LifeFramework/config.json: json { "schemaVersion": 1, "jobs": { "tracks": { "LUMBERJACK": { "op": "remove" }, "GHOST": { "op": "patch", "dailyXpCap": 1 } }, "xpAwards": { "gather": { "op": "patch", "xp": 9...
  - Start the server. In console.log expect [LFDebug:Config] naming jobs.tracks GHOST: patch names an id no lower layer has, and no other config error.
  - Join and gather a resource (a gather action): the FARMER XP it awards is 9, where the shipped file gives 2 ([LFDebug:Level] lines). config.json is read at boot, so restart after editing.

## Desk (no game needed) (~14 min)

- [ ] #685 M3-Patterns: Crime catalog: typed crime codes behind wanted, fines and jail (~2 min)   fail [ ]
  - Rob the fuel till, steal a weapon, take a car by force, and kill someone: wanted rises 1, 2, 3, 3 as before (docs/playtest.md, "Crime penalties are one table").
  - Optional: add the crime owner snippet from docs/playtest.md, /configreload as an admin, rob three times and check the third adds 3.
- [ ] #1004 M3 chapter: Your town in five minutes (~3 min) [LOOK] [chapter #1004]   fail [ ]
  - Follow docs/owner/install.md on a machine with Workshop access once both items are published (#370): unzip the example server, run start.cmd, join, pass the quiz, buy at a shop.
  - On life-framework.org/config check the intro and server.json text read right.
  - No screenshots: docs and one website sentence, no in-game visual change.
- [ ] #286 M3-Generator: Generator page renders a form from config.schema.json (~2 min) [chapter #225 #1004]   fail [ ]
  - Open /config on the deployed preview, change a default, add a price rule, and download. Check the file only holds your changes and schemaVersion.
- [ ] #296 #967 M3-Example: Release download built from the template (~3 min) [chapter #227 #1004]   fail [ ]
  - also proves #967: M3-Packs: Licence note for pack authors
  - In a worktree run tools\cli example-zip. Expect tmp/release/LifeFramework-ExampleServer-DRAFT.zip and the note that the pack id is a placeholder.
  - Run tools\cli example-zip --final. Expect exit 1 naming mods.pack.modId.
  - After #370: put the real pack id in tools/release/example-zip.json, run --final, unzip, run tools\cli config-check config.json, and start a server from the folder with start.cmd. A restart is not needed.
  - Read docs/architecture/pack-licence.md once for tone and accuracy against the maintainer's intent. It is not legal advice and states only the project's licences and the obvious obligations.
- [ ] #370 #371 M3-Release: First Workshop upload of core and pack (maintainer) (~2 min)   fail [ ]
  - also proves #371: M3-Release: A clean dedicated server boots from the Workshop items alone
  - After the maintainer's first upload (#370): from a worktree run tools\cli workshop boot-check 79636E668EA37AC9 B1E84D5687E11724. Expect SUMMARY workshop-boot-check ok stage=pass. The client-join stage (a client starte...
  - Follow docs/release-first-upload.md steps 1 to 5 and record the page URLs and passing line in docs/setup.md.
- [ ] #372 M3-Release: Release process doc for updates and rollback (~2 min)   fail [ ]
  - No manual steps. Docs and one website build-list edit. Confirm the two pages render at /docs/release-process and /docs/upgrades after the next site build.
