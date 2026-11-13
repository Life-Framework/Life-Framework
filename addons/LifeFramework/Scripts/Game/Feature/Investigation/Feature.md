# Investigation: scene evidence

Shots and dropped items at a crime become evidence an officer can bag (#268).

- A crime report (`LF_WantedReports.Report`) opens a scene: the report number, offence, place and
  position, open for `m_iSceneSeconds`.
- A shot by a player inside a scene's radius leaves one casing (`LF_EvidenceCasing.et`) at the
  shooter, up to `m_iMaxCasingsPerScene`. The hook is the character's `OnProjectileShot` event
  (`LF_Evidence.WatchShooter`, registered when the body spawns).
- An item that leaves a player inventory (`SCR_InventoryStorageManagerComponent.OnItemRemoved`)
  and lies inside a scene gets an `LF_EvidenceMarker.et` beside it.
- An on-duty officer picks "Bag as evidence" (`LF_BagEvidenceAction`). The server spawns an
  `LF_EvidenceBag.et` stamped with report, offence, place and minutes lying, seals the piece
  inside it as a hidden child, and hands the bag over. "Read evidence bag" shows the line.
- Untouched evidence expires after `m_iExpirySeconds`. Total evidence is capped at
  `m_iMaxEvidence`; the oldest goes first. Nothing runs per frame: one 30 s timer sweeps while
  anything lies about.
- Config: `Configs/Investigation/LF_Evidence.conf` (`LF_EvidenceConfig`).

Prefab rule learned here: a prefab derived from `RoleplayItem_Base.et` must reuse the inherited
`ActionsManagerComponent` GUID (`{508AB5952B584B2E}`). A fresh GUID makes a second component and
the spawned entity comes out with no components at all.

Not done: evidence is not saved (a restart clears scenes and what lies about); a bag in an inventory
loses its stamp over a restart until the locker (#269) logs it.

## Evidence locker (#269)

- `Prefabs/Investigation/LF_EvidenceLocker.et` stands at the police station (`Roadmap_Police_EvidenceLocker`
  in `Street_Police.layer`, beside the armory). An on-duty officer picks "Log evidence bags": every
  stamped evidence bag they carry is opened, each piece goes through `LF_FenceSerials.IdentifyItem`
  (a serial that names its buyer identifies that account's open wanted reports), a row records it
  against the crime report (`LF_EvidenceLocker.CountFor(report)`), and the bag is consumed. The locker
  takes nothing else. "Read case file" lists the newest rows (police only).
- The case file is saved as the `evidencelocker` payload (save version 18). Rows are plain values: report,
  offence, place, kind, minutes, prefab, serial, identified. The piece itself is not kept.
- Forced entry is `LF_Breach` (the locker is a breach target `LF_EvidenceLocker`): arming a charge
  already files the break-in report. Once forced open, anyone but on-duty police can "Burn the case
  file", which destroys the logged rows.
- Not done: the wanted board does not yet print "evidence on file" beside a report; reading is by the locker.

## Witnesses (#266)

- `LF_WantedReports.Report` calls `LF_Witness.RecordCrime`: every player within `m_fSightRange` of the suspect is a witness, with the distance stored.
- The payphone has three `LF_WitnessPayphoneAction` entries (clothing, vehicle, plate). The server gives only what that distance allows (`LF_Witness.PresetsFor`); the client never sends text.
- Statements are shown by `LF_WantedBoard` under the dispatch log as `S;` entries (`LF_Witness.BoardLine`).
- Config: `Configs/Investigation/LF_Witness.conf` (`LF_WitnessConfig`). Not saved across a restart.

## Questioning witnesses (#267)

- `LF_WitnessAskAction` is on `Npc_Base.et` (context `Witness`), shown to on-duty police within 3 m. The server rolls the NPC accuracy (`LF_WitnessConfig.NpcAccuracy`) per thing it could see; an NPC with no recent scene says it saw nothing.
- The `witness` role slot (`LF_WitnessRole`, entry on `Character_Roleplay.et`) asks a player; it says everything its distance allows.

- NPC witnesses (#610, #615): `LF_Witness.CollectNpc` skips only an NPC that is dying (`LF_NpcDamage.IsDying`); it used to skip all of them. Answers come from `LF_WitnessDialogue` (names, four noncommittal lines). Ambient civilians can be shot (`Core/Npcs/LF_NpcDamage.c`); NPCs without an `LF_NpcHitZone` cannot.

## Scene pieces (#148)

- **Player sees:** a robbery on foot leaves a footprint card on the ground a stride behind where the robber stood; a shot at a scene leaves a casing on the floor around the shooter (a burst spreads over a ring, not a pile); a witness who tells an officer what they saw leaves a written statement at their feet. Only an on-duty officer can take any of them, with "Bag as evidence".
- **Owner and side:** `LF_ScenePieces` (server) decides what a crime leaves and where it lies: `GroundSpot` asks the placement probe (`LF_PlacementAudit.SupportAt`) for the surface under the point, skipping bodies. `LF_Evidence` keeps every piece as one row, so `m_iMaxEvidence`, `m_iMaxCasingsPerScene` and the expiry hold for the new kinds too. Hooks: `LF_WantedReports.Report` (footprint), `LF_WitnessAskAction` and `LF_WitnessRole` (statement).
- **Police only:** the pieces (`LF_EvidencePiece_Base.et` and its children: casing, marker, footprint card, statement) are not inventory items. The vanilla pickup and the inventory's vicinity list never offer them; only `LF_BagEvidenceAction` takes one. Before #148 the casing and the marker were items anyone could pick up.
- **Data:** `Configs/Investigation/LF_Evidence.conf` `m_aFootprintOffences` (robbery, hospital robbery, break-in, armory heist, cash van). A piece carries a note for its card (footwear key, "clothing~vehicle~plate", item name key), stamped on the bag (`LF_EvidenceBagComponent.GetNote`). Not saved: what lies about is cleared by a restart, as before.
- **Proof:** `investigation/scene-pieces` (LOGIC), `investigation/scene-pieces-world` (WORLD: the gap to the ground, flat cards, police-only), `node agent-box/session.mjs case-board-proof`.
- **Stand-ins:** the footprint card and the statement are the personal papers mesh (the ID card mesh read as a dropped ID booklet) (`docs/asset-inventory.md`). A real footprint decal under the card is not done.

## Case board (#148)

- **Player sees:** a cork board in the police station porch (`Roadmap_Police_CaseBoard`, Street_Police.layer, between the brick pillar and the door). An on-duty officer sees up to three open cases on it: a sheet per case (CASE n, the suspect's portrait once identified or an empty photo frame and UNKNOWN PERSON, the offence, the place and the time of the crime) with up to four evidence cards under it and a red string from each card's pin to the suspect's. Anyone else sees plain cork. "Pin evidence" logs the bags the officer carries at the locker and pins them; with none it pins the next logged card of an open case. "Unpin evidence" takes the last card down; it stays in the locker. Read gives the board's sign card.
- **Owner and side:** `LF_CaseBoard` (server) keeps only what the existing systems do not: the time of each report, each card's note and its pin. A case is a crime report (`LF_WantedReports`) and closes with it; a card is an evidence locker row (`LF_EvidenceLocker.LogBag` hands it over); the portrait is the wanted board's look. `LF_CaseBoardFace` (client) is the wanted board's render target (`LF_WantedPosterFace`) with case columns; a string is `LF_CaseBoardString.layout`, two pins and a chain of beads (a turned image does not draw full length in the render target). A new list builds the face again.
- **Police only on the wire:** the list goes to on-duty officers only, by an owner RPC on each officer's player controller (`LF_SendCaseBoard`): on a change (events, coalesced 1.5 s) and when a client comes on duty (`LF_AskCaseBoard`, also a late joiner's first tick). The prop replicates nothing of it. `docs/decisions.md` 2026-11-13 "#148".
- **Data:** the `caseboard` payload (save version 31): `LF_CaseBoardRow` per report (time) and per card (locker number, kind, note, pin order); a closed case's rows are not saved. Strings `LF-CaseBoard_*`, the sign `police_cases`.
- **Proof:** `investigation/case-board` (LOGIC), `investigation/case-board-world` (WORLD), `investigation/case-board-persist` (PERSISTENCE), `e2e/pass2-verify-load` (a real restart), `node agent-box/session.mjs case-board-proof`.
