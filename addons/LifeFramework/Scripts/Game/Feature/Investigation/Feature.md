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
