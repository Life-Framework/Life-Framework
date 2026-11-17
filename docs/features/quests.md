# Quests

### `LF_QuestTrackerComponent` — `Scripts/Game/Feature/Quests/`
- ✅ Server-authoritative quest definitions, acceptance, objective progress,
  timed expiry, collect recounts across managed nested storage, rewards, and
  client quest-log replication.
- ✅ `QuestGiver_Start` and `DebugWorld` provide a world-facing onboarding
  example with localized menu layouts and registered logic/world tests.
- ✅ **Turn-in loop closed** (verified 2026-11-07): turn-in removes each
  `COLLECT_REPORT` objective's target amount through `LF_InventoryUtils` before
  the reward, all or nothing (a short removal puts the items back and refuses).
  Client open/accept/turn-in (`RequestOpenGiver`/`RequestAccept`/`RequestTurnIn`)
  require the registered giver that offers the quest within
  `LF_QuestManager.GIVER_RADIUS` (5 m); an unknown giver id is refused. Server
  scripts may still call `AcceptQuest`/`TurnInQuest` directly. Pin test
  `security/quest-turnin-takes-items`.
- ✅ **Guided arrival chain** (M1-18, 2026-11-08): `QuestGiver_Clerk.et`
  (`Quest_Clerk` behind the registry counter on the square, 137.06 0 240.91, under a parasol beside the REGISTRY OFFICE sign) runs five steps: `arrival_register`
  (talk; hands over the ID card), `arrival_board` (`SIGNAL sign.read:town_map`),
  `arrival_work` (`SIGNAL job.run_complete:parcel`), `arrival_bank`
  (`SIGNAL atm.deposit`), `arrival_done` (talk; a welcome gift, not cash (#1781), and "the town is yours" with
  pointers to the rest of town). Steps 2-4 start and finish on their own
  (`m_bAutoAccept`, `m_bAutoTurnIn`), so the player never walks back between
  errands; each new step's errand shows as a 12 s hint. A fresh spawn without the
  card is pointed at the clerk. A giver's menu lists one chain step at a time and
  never re-lists a one-off quest already turned in (`LF_QuestDefinition.IsListed`),
  so a returning player is not offered the chain; a player who left mid-chain
  gets the next step back on spawn (`SettleChain` after `RestoreTurnedIn`).
  Accepting at a giver counts that giver's talk objective at once. No waypoints.
  The clerk is a converse giver (`m_bConverse`): "Talk to the clerk" opens no menu; one talk
  turns in what is done, takes on the next quest, and the clerk's lines show as slips
  (`RequestConverse`). Since 2026-11-13 the talk is a conversation scene ("NPC dialogue scenes"
  below): the reply "I am new in town" / "What should I do now?" runs the same `RequestConverse`
  through `LF_DialogueHook_QuestConverse` (`ConverseSpoken`), and the story lines are spoken, not
  shown as slips; mid-errand he repeats the errand, after the chain his idle line
  (`m_sIdleKey`). A second NPC with a placed giver's id no longer takes the giver over.
  Harness: `agent-box/flows/arrival-chain.txt`; `state` adds `idcard=` and `quests=`;
  `stand`/`use` reach a replicated NPC (no layer name on a client).
- ✅ `SIGNAL` event type with a string key (`LF_QuestManager.ReportSignal`).
  Hooks in `LF_QuestHooks.c` (modded classes; the job, ATM, and sign files are
  untouched): `LF_JobDropoffAction` reports `job.run_complete:<job>` for a paid
  run, `LF_CharacterATMComponent.ReplyResult` reports `atm.deposit` for a
  successful deposit, and `LF_SignReadAction` asks the server (`RpcAsk_SignRead`)
  to report `sign.read:<id>`, counted only with that sign within 6 m.
  `QuestGiver_Start` (test quests, "Test quest board") moved to the test annex
  (`Quest_TestGiver`, 104 0 70). Tests: `quests/signal-event` (LOGIC),
  `quests/arrival-chain` (WORLD).

- ✅ **The arrival chain ends with a gift, not cash** (#1781, #795). `arrival_done` pays no money. What the
  clerk hands over is pack data: `LF_WelcomeGiftTable` (`Feature/Quests/LF_WelcomeGift.c`) reads
  `Configs/Quests/LF_WelcomeGift.conf`, rows keyed by quest id with a list of item prefabs, and a pack overlays
  rows with the override verbs (`m_sOp`). The core file ships no rows; `LF_QuestTrackerComponent.GrantReward`
  spawns a quest's own `m_aRewardItems` and then its table row into the inventory. The 1989 Everon pack
  (`LF_WelcomeGift_Everon1989.conf`, in its mission header) gives a folded paper map, a packet of crackers and a
  bottle of apple juice. A row naming an item that does not load is refused (`[LFDebug:Quests]`). Pack-only: no
  owner `config.json` section. Tests: `quests/onboarding-chain` (WORLD: links, no money, items resolve),
  `quests/arrival-chain` (gift lands, no cash), `everon/welcome-gift` (pack leg).

### Layouts (loaded by `LF_Test_Data`)
- ✅ `LF_SplitQuantityDialog`, `LF_CharacterCreationMenu`, `LF_ATMMenu`,
  `EL_SurvivalHUD`, `ShopMenu`, `PoliceMenu`,
  `DeathScreen` load (WORLD tier). The `SurvivalHUD` menu preset and its
  `EL_SurvivalHUD` class were never opened and are removed (2026-11-07); `LF_BetaHud`
  draws the gauges. The layout stays only as this load fixture. The unused
  falling-fruit widget (`LF_FallingFruit`, which loaded a layout that never existed) is
  removed; `EHarvestJobType` moved to `Feature/Jobs/LF_HarvestJobType.c`.
- ✅ `LF_DebugMenu` is a development menu opened with F10 (or its rebind),
  with server-routed controls for cash, wanted state, faction, jobs, survival,
  and CIVILIAN job XP (#749; skill points no longer exist). Actions affect only the requesting player's state
  (verified 2026-08-30). `LF_DevGate` (`Core/LF_DevGate.c`) checks every
  action on the server. It logs `[LFDebug:DebugMenu] debug action refused: ...`
  and refuses unless the game mode sets `m_bDevToolsEnabled` or the sender
  holds `EPlayerRole.ADMINISTRATOR`. Only `GameMode_Roleplay_Debug.et`
  (DebugWorld) sets the flag. The production `GameMode_Roleplay.et` leaves it
  off. The open key runs the same check on the client and logs
  `open refused` instead of opening. `LF_DevGate.IsAllowed` has the LOGIC test
  `debug-menu/gate` (2026-11-07).
- ✅ The playtest mission `Missions/LF_PlaytestTown.conf` (`LF_MissionHeader`,
  `Core/LF_MissionHeader.c`) runs DebugWorld with `m_bLF_DevToolsOff 1`: the
  header overrides the DebugWorld game mode opt-in, so only administrators
  pass `LF_DevGate` there. A bare world (Workbench play, `toolscli test`) has
  no LF header and keeps the opt-in. LOGIC test `host/playtest-mission`
  (2026-11-08).
- ✅ **Workshop scenario (2026-11-10).** `LF_PlaytestTown.conf` is the Workshop item's scenario
  "Life Framework: Playtest Town": plain-text name and description (the Workshop backend copies
  them as text), the town picture `Missions/LF_PlaytestTown_Preview.edds` as icon, preview and
  loading screen, 16 players, details from `#LF-Mission_PlaytestTown_Details`. `EL_DebugTest.conf`
  and `EveronLifeGameMode.conf` set `m_bShowInScenarioMenu 0`. `tools\cli pack` packs the addon
  as Workbench Publish bundles it. Proven from the packed addon in a clean profile: Scenarios
  lists it, Play loads the town, form R-89 signs, top-up and test cars apply. Asserted in
  `host/playtest-mission`. Publish steps: `docs/playtest/workshop-publish.md`.
- ✅ **Batch 1 of the pack move (#323, 2026-11-14).** `LF_PlaytestTown.conf`, its preview and
  `EveronLifeGameMode.conf` live in `addons/LF_Everon1989/Missions/` with their GUIDs; both playtest
  headers load the pack world `Worlds/Everon1989Town/Everon1989Town.ent`, a SubScene of the core
  DebugWorld that holds `Village_Buildings`, `Village_Dressing`, `Village_Roads` and
  `Village_Signs` (the mining sign). `MainWorld/`, the title poster mesh and `EL_MiningSign.et` moved
  too. `host/playtest-mission` and `harness/dev-gate-dedicated` read the core fixture
  `Missions/LF_HostedTest.conf`; `validate-pack-missions` holds the Workshop header checks. A bare
  `session up`, `look`, `place` and `audit placement` use the pack world when the pack is in the
  checkout; `tools\cli test` still boots the core DebugWorld. `e2e --pre-move <ref> --mission playtest`
  reloads a save made before the move.
  The `ci` step `pre-move save` (`tools/lib/pre-move-gate.mjs`, #327) runs it against the pinned `502ebf39` whenever persistence code, a mission header or the pack boundary changes (`docs/persistence.md` "Pre-move save gate").
- ⚠️ Localization keys `#EL-*` are frequently passed through `string.Format`
  (corrupting keys with `%1`) and/or handed to
  `SCR_HintManagerComponent.ShowCustomHint`, which does not localize `#Key`
  — the player sees raw keys. Spanish literals in notifications remain. The job,
  level and licence texts, the duty status words, the robbery, gather and process action
  texts, and the DebugWorld shop names moved to `#LF-*` keys on 2026-11-07, worded as
  employment rather than XP. Violates the AGENTS.md localization contract until the
  notification prefixes move too.


### Town hall building kit (`tools/buildings/town-hall/`, 2026-11-16, epic #2201)
- 🧪 **The town hall as a whole-building kit.** `TownHall.et` (pack, `LF_Everon1989/Prefabs/Buildings/TownHall/`) is the vanilla `MunicipalOffice_E_01` with a pruned copy of its furniture (242 props cut to 100: no infirmary, switches or duplicate radiators) and a decor block, written by `node tools/buildings/town-hall/shell.mjs --pass 0|1|2` (`decor.mjs` holds the rows; `spec.json`, `proof.json` and `looks.mjs` re-shoot it). It stands in the debug grid, claim `town-hall` (cells `c11r2`, 3 x 2), with the registry clerk behind the post counter, the licence desk as the counter's second window (`LF_LicenceDesk` in place of the vanilla window module), the town board on the entrance corridor wall, the registry sign outside and an `ArrivalSpawn` marker at the entrance (the real spawn point joins when `town fit` places the kit: a second spawn in GridWorld would move the test spawn). Performance budget: at most 6 dynamic lights (four ceiling lights, one first-floor light, one lit facade lamp), no shadow casters, static props only, no scripts. The vanilla ceiling lamps glow through an emissive cover instead of a light each. Merged, in-dev: the maintainer reads the pictures. No test: layer and prefab data only (`audit placement --grid --strict` is clean).
- 🧪 **Town hall A pass, part 1: the first floor** (#2309). The kit now stands in a pack grid cell (`Grid_town_hall.layer` in `LF_Everon1989`'s Everon1989Grid, `grid place town-hall --pack`; the old `stage/` copy is gone). `shell.mjs --pass 3` (the default) adds the first floor: emissive ceiling tubes and one short no-shadow light each for the hall's west end and the two offices, and the desks' paperwork (typewriter, files, phone, lamp, radio), every prop snapped onto its desk top and `building check town-hall` clean. Budget: 9 of 10 lights, 0 shadow casters, 191 of 500 entities (`perf building TownHall --size large`). Merged, in-dev.
