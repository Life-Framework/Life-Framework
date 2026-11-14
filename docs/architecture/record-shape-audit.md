# Record-shape audit (#761)

Every record the game-mode store saves, its save version, its serialized
fields, and whether the shape is final for the persistent launch (#556).
Audited 2026-11-14 at save version 32 (`buildstamp` last). The rule it serves
is in `docs/persistence.md` "Record shapes: append only";
`tools/validation/validate-record-shapes.mjs` enforces it against the lock in
`tools/validation/record-shapes.json`.

## How a record is saved

`LF_PersistenceComponentSerializer` writes `version`, the prefix (accounts and
bank), then each payload of `RegisterPersistencePayloads` as
`context.Write(array<ref LF_X>)`. The engine writes each object by reflection:
its non-static fields in declaration order, base class first, nested `LF_`
records inline. A binary save is positional, so that order is the save format.
`[NonSerialized()]`, `static` and `const` members are not in it.

## Verdict

Every shipped record is final as it stands. No record needs a field removed,
retyped or reordered before launch, and none may have one: every one is in
saves that must keep loading (D7, and the `wt e2e --legacy-save` proof loads a
version 23 save). New M3 data goes in sidecar payloads, the
pattern #750a (`orgranks`, version 27) and #749 (`joblevels`, version 28) set.

Two sidecar reshapes are still to land before launch (#1216, #1217), each its own PR with its
own version bump (D6). They change no existing record:

| Reshape | Spec | Payload | Live side |
|---|---|---|---|
| Vehicle state (#752a) | `docs/architecture/record-shape-vehicle.md` | `vehiclestate`, `LF_VehicleStateRow` | `[NonSerialized()]` fields on `LF_VehicleRecord` |
| Held ground (#751) | `docs/architecture/record-shape-held-ground.md` | `heldground`, `LF_HeldGroundRow` | `LF_HideoutState` |

The other #746 children need no record change: #685 keeps
`LF_CrimeRecordRow` and uses the offence key as the crime id; #754a keeps the
police rank an int index into the config ladder (qualifications moved to #860,
M5); #755 is config on placed zones; #753, #748 and the other new payloads
moved to M5 (D5) and append there.

At #556 (#1218) the lock's `"launched"` flips to `true`: from then on a shipped record
takes no new serialized field at all, only sidecar payloads.

## Notes per record

- **Retired fields stay.** `LF_SurvivalStatsRecord.m_fHealth` is written as
  `RETIRED_HEALTH`. `LF_ProgressionRecord.m_bHasLevel`, `m_iPlayerLevel` and
  `m_fPlayerExperience` are the pre-#749 single level, read once to seed job
  XP. `LF_OrganizationRankRecord`'s four bools are read for record version 1
  organizations; the rights mask lives in `orgranks`. Removing a retired field
  moves every later field, so they stay as padding.
- **Per-row version.** `LF_OrganizationRecord`, `LF_HideoutRecord`,
  `LF_StorageSnapshotRecord`, `LF_ProgressionRecord`, `LF_OrganizationRankRow`
  and `LF_SaveStampRecord` carry `m_iVersion` first, so a change of meaning can
  be recognised on read. The rest carry none; a change of meaning in them is a
  new payload. New row classes start with `int m_iVersion` (the vehicle and
  held-ground specs do).
- **Not in the store.** `LF_QuantityComponentSerializer` (stack amount) and
  `LF_SerialComponent` (item serial) are entity component serializers that
  write their own `version` 1 and one value. The whitelist is a file outside
  the save. Neither is covered by the lock.
- **Stable ids.** Prefab strings in records (`m_sPrefab`, `m_rPrefab`) resolve
  through the alias table on load ("Stable ids and renamed content (#965)" in
  `docs/persistence.md`); the shape does not change for a rename.

## Every saved record

`Since` is the save version its payload first appeared in (1 for the prefix
and survival). Regenerate with
`node tools/validation/validate-record-shapes.mjs --report`.

| Payload | Record | Since | Serialized fields |
|---|---|---|---|
| prefix | `LF_PlayerAccountRecord` | 1 | `string m_sPersistentId`, `array<ref LF_PlayerCharacterRecord> m_aCharacters`, `int m_iActiveCharacterIdx`, `LF_Faction m_eFaction`, `bool m_bOnDuty`, `int m_iWantedLevel` |
| (in LF_PlayerAccountRecord) | `LF_PlayerCharacterRecord` | 1 | `string m_sId`, `ResourceName m_rPrefab`, `string m_sFirstName`, `string m_sLastName`, `int m_iAge` |
| prefix | `LF_BankAccountRecord` | 1 | `string m_sPersistentId`, `int m_iBalance` |
| survival | `LF_SurvivalStatsRecord` | 1 | `string m_sPersistentId`, `float m_fHunger`, `float m_fThirst`, `float m_fHealth` |
| organizations | `LF_OrganizationRecord` | 2 | `int m_iVersion`, `string m_sId`, `string m_sName`, `int m_iFund`, `bool m_bExtraStorage`, `bool m_bProcessingSlot`, `bool m_bParkedVehicle`, `array<ref LF_OrganizationRankRecord> m_aRanks`, `array<ref LF_OrganizationMemberRecord> m_aMembers` |
| (in LF_OrganizationRecord) | `LF_OrganizationRankRecord` | 2 | `string m_sId`, `string m_sName`, `bool m_bCanWithdraw`, `bool m_bCanInvite`, `bool m_bCanKick`, `bool m_bCanBuyUpgrade` |
| (in LF_OrganizationRecord) | `LF_OrganizationMemberRecord` | 2 | `string m_sCharacterId`, `string m_sRankId` |
| hideout | `LF_HideoutRecord` | 3 | `int m_iVersion`, `string m_sHideoutId`, `string m_sOrganizationId`, `bool m_bLocked` |
| vehicles | `LF_VehicleRecord` | 4 | `string m_sVehicleId`, `string m_sOwnerUid`, `string m_sKeyId`, `string m_sPrefab`, `string m_sSpawnPrefab`, `string m_sShopId`, `string m_sPlate`, `string m_sNameKey`, `bool m_bLocked`, `bool m_bStored` |
| jail | `LF_JailRecord` | 5 | `string m_sPlayerUid`, `float m_fRemaining`, `bool m_bServing` |
| carried | `LF_StorageSnapshotRecord` | 6 | `int m_iVersion`, `string m_sOwnerKey`, `string m_sSlot`, `array<ref LF_StorageItemRecord> m_aItems` |
| (in LF_StorageSnapshotRecord) | `LF_StorageItemRecord` | 6 | `string m_sPrefab`, `int m_iQuantity`, `int m_iParent`, `int m_iStorage`, `int m_iSlot` |
| progression | `LF_ProgressionRecord` | 7 | `int m_iVersion`, `string m_sPlayerUid`, `int m_iCareerLevel`, `float m_fCareerExperience`, `int m_iCareerPaidTotal`, `int m_iPoliceRank`, `int m_iOnDutySeconds`, `int m_iUnpaidDutySeconds`, `float m_fBenefitElapsedSeconds`, `bool m_bHasLevel`, `int m_iPlayerLevel`, `float m_fPlayerExperience`, `int m_iSkillPoints`, `int m_iTotalSkillPoints`, `array<string> m_aLicenses`, `array<string> m_aTurnedInQuests` |
| introductions | `LF_IntroductionRecord` | 8 | `string m_sKnowerUid`, `string m_sKnownUid` |
| itemstate | `LF_StorageItemStateRecord` | 9 | `string m_sOwnerKey`, `string m_sSlot`, `array<int> m_aAmmo` |
| wanted | `LF_WantedReportRecord` | 10 | `string m_sPlayerUid`, `string m_sOffence`, `string m_sClothing`, `string m_sVehicle`, `string m_sPlace`, `bool m_bIdentified`, `int m_iSequence` |
| jobsignups | `LF_JobSignUpRecord` | 11 | `string m_sPlayerUid`, `string m_sJob` |
| tickets | `LF_FineTicketRecord` | 12 | `int m_iNumber`, `string m_sHolderUid`, `string m_sHolderName`, `string m_sOfficerUid`, `string m_sOfficerName`, `int m_iOfficerRank`, `string m_sOffence`, `int m_iBaseAmount`, `int m_iAmount`, `float m_fOnlineSeconds`, `int m_iStage`, `bool m_bPaid`, `string m_sIssued`, `string m_sPaidAt` |
| incidents | `LF_IncidentRecord` | 13 | `int m_iNumber`, `string m_sKind`, `int m_iTime`, `string m_sSubjectUid`, `string m_sOtherUid`, `float m_fX`, `float m_fY`, `float m_fZ`, `int m_iAmount`, `string m_sHow`, `bool m_bMurder` |
| ledger | `LF_LedgerEntry` | 14 | `int m_iId`, `int m_iTime`, `string m_sPlayerUid`, `string m_sAccount`, `int m_iDelta`, `string m_sReason`, `string m_sCounterpartyUid`, `int m_iBalanceAfter` |
| crimerecord | `LF_CrimeRecordRow` | 15 | `string m_sPlayerUid`, `string m_sOffence`, `int m_iCount` |
| fenceserials | `LF_FenceSerialRow` | 16 | `int m_iNumber`, `string m_sSerial`, `string m_sBuyerUid`, `string m_sPrefab` |
| incidentflags | `LF_IncidentFlagRow` | 17 | `int m_iNumber`, `string m_sReporterUid`, `int m_iTime` |
| evidencelocker | `LF_EvidenceLockerRow` | 18 | `int m_iNumber`, `int m_iReport`, `string m_sOffence`, `string m_sPlace`, `string m_sKind`, `int m_iMinutes`, `string m_sPrefab`, `string m_sSerial`, `string m_sOfficerUid`, `bool m_bIdentified` |
| bills | `LF_TreatmentBillRecord` | 19 | `int m_iNumber`, `string m_sPatientUid`, `string m_sPatientName`, `string m_sMedicUid`, `string m_sMedicName`, `int m_iAmount`, `int m_iShare`, `bool m_bPaid`, `bool m_bShareTaken`, `string m_sIssued`, `string m_sPaidAt` |
| chainstations | `LF_ChainStationRow` | 20 | `string m_sStationKey`, `string m_sChainId`, `string m_sStepId`, `int m_iWork`, `int m_iNeeded` |
| storestock | `LF_StoreStockRow` | 21 | `string m_sStoreKey`, `string m_sPrefab`, `int m_iStock` |
| itemserials | `LF_StorageItemSerialRecord` | 22 | `string m_sOwnerKey`, `string m_sSlot`, `array<string> m_aSerials` |
| farmplots | `LF_FarmPlotRow` | 23 | `string m_sPlotKey`, `string m_sCropId`, `int m_iGrownSeconds` |
| characters | `LF_CharacterSlotRecord` | 24 | `string m_sAccountUid`, `string m_sCharacterId`, `string m_sRecordKey`, `int m_iWantedLevel` |
| storeunpaid | `LF_StoreUnpaidRow` | 25 | `string m_sStoreKey`, `string m_sPrefab`, `int m_iPrice`, `string m_sCarrierKey`, `bool m_bConsumed` |
| playtime | `LF_PlaytimeRecord` | 26 | `string m_sAccountUid`, `string m_sJob`, `int m_iSeconds` |
| orgranks | `LF_OrganizationRankRow` | 27 | `int m_iVersion`, `string m_sOrgId`, `string m_sRankId`, `int m_iOrder`, `int m_iRights` |
| joblevels | `LF_JobXpRow` | 28 | `string m_sPlayerUid`, `string m_sJob`, `int m_iXp` |
| wantedlooks | `LF_WantedLookRow` | 29 | `int m_iSequence`, `string m_sAppearance` |
| idcards | `LF_PhotoIdRow` | 30 | `string m_sNumber`, `string m_sHolderId`, `string m_sFirstName`, `string m_sLastName`, `int m_iAge`, `string m_sIssued`, `string m_sLook` |
| caseboard | `LF_CaseBoardRow` | 31 | `int m_iReport`, `int m_iNumber`, `string m_sKind`, `string m_sNote`, `int m_iPinned` |
| buildstamp | `LF_SaveStampRecord` | 32 | `int m_iVersion`, `string m_sGameBuild`, `int m_iSaveVersion` |
