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
