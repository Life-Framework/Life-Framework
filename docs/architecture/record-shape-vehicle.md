# Record shape: vehicle state (#752a)

Design spec for the M3 reshape of the owned-vehicle record (epic #746, part a of
#752). It fixes the saved shape that the M5 behaviours fill: garages and towing
(#774, #775), inspection and hidden compartments (#778, #779), and the vehicle
cache sidecar (#859). No behaviour ships with it. Status: design, not built.

Rules this follows: `docs/persistence.md` (positional payloads, append only),
D5 and D6 in `docs/roadmap/m3-plan.md` section 6 (one PR per reshape, its own
version bump), and #761 (append-only versions, every older save loads).

## Today

- `LF_VehicleRecord` (`Feature/Vehicles/LF_VehicleOwnership.c`), class
  `VERSION = 1`, no per-instance version field: vehicle id, owner uid, key id,
  prefab, spawn prefab, shop id, plate, name key, locked, stored.
- Payload `vehicles`, save version 4, written with `context.Write(array)` by
  reflection. The live map is `LF_VehicleOwnership.Records()`, keyed by vehicle
  id (the lock identifier).
- Impound is derived (`IsImpounded`: owned, not stored, no live entity). Every
  restart puts each unstored vehicle at the impound lot
  (`ImpoundStreetVehicles`). The save has no impound field.
- The save version on `main` is 32 (`buildstamp` last).

## Shape: a sidecar payload, not new fields on the record

A field added to `LF_VehicleRecord` changes the positional layout of the
`vehicles` payload and breaks every older save (`docs/persistence.md`, "Rules").
The precedent on `main` is #750a (`orgranks`) and #749 (`joblevels`): the old
record keeps its layout and the new data is a row payload appended at the end.
This spec does the same, with the `LF_StorageItemRecord.m_iAmmo` pattern for the
live side.

- The new fields live on `LF_VehicleRecord` as `[NonSerialized()]` members, so
  every reader and writer uses the one live record and the `vehicles` payload
  layout does not move. `Copy` and `CopyOnto` copy them.
- A new payload `vehiclestate` is appended to `RegisterPersistencePayloads`. It
  writes one `LF_VehicleStateRow` per record whose state differs from the
  defaults (a stock, never-towed, never-inspected car writes no row).

```
class LF_VehicleStateRow : Managed
{
	static const int VERSION = 1;

	int m_iVersion;            // VERSION at write
	string m_sVehicleId;       // key: LF_VehicleRecord.m_sVehicleId
	int m_iTowState;           // LF_EVehicleTowState
	int m_iTowedAt;            // unix seconds, 0 = never
	int m_iInspectedAt;        // unix seconds, 0 = never
	int m_iCompartment;        // fitted compartment level, 0 = none
	int m_iCargoTune;          // bought cargo tuning step, 0 = stock
}
```

`m_iVersion` is per row, as on `LF_OrganizationRankRow`, so a later row change
can be recognised. A later field is still a new payload, never a new member
here.

## Fields

| Field | Type | Default | Owner | Written by | Read by |
|---|---|---|---|---|---|
| `m_iTowState` | int, enum `LF_EVehicleTowState`: 0 `NONE`, 1 `TOWED` (abandonment sweep, tow yard), 2 `POLICE_IMPOUND` (seized by police) | 0 | `LF_VehicleOwnership` | #774 tow sweep (sets 1); police seizure (sets 2, #859); `CollectFromImpound` and the tow-yard release (set 0) | `IsImpounded`, `LF_ImpoundDeskComponent` (fee and which desk), garage ledger card (#774), `LF_StolenVehicles` (a hot car is not towed) |
| `m_iTowedAt` | int, unix seconds | 0 | `LF_VehicleOwnership` | the same writers as `m_iTowState`; 0 when it returns to `NONE` | tow notice and ledger line (#774), any storage-fee-per-day rule (#775 pack) |
| `m_iInspectedAt` | int, unix seconds | 0 (never inspected) | `LF_VehicleOwnership` | #778 inspection action (`LF_InspectionAction`, police or mechanic) | #778 sticker state on spawn, `LF_VehicleSearchAction` (expired sticker line), validity window from `m_iInspectionValidDays` in config |
| `m_iCompartment` | int, level | 0 (none fitted) | `LF_VehicleOwnership` | #778/#779 fitting at a mechanic bay | #859 cache stow and take-out (refused at 0), `LF_ChainSearch.Collect` (hidden split), capacity from config by level |
| `m_iCargoTune` | int, step | 0 (stock) | `LF_VehicleOwnership` | the cargo-tuning action at a mechanic bay (no issue yet, see question 4) | the storage capacity resolve for the vehicle: base capacity times the config multiplier for this step |

Not on the row, on purpose:

- **Garage slot.** The cap is config (`LF_GarageConfig.m_iSlotCap`, set high
  enough not to bind), and slots used is the count of the owner's records with
  `m_bStored`. No per-vehicle slot index. Bought extra slots are per owner, not
  per vehicle, so they are a per-character row in #774's own payload
  (`garageslots`), which can be appended in a minor release (D5).
- **Hidden compartment contents.** The `vehiclecache` sidecar of #859 (M5),
  keyed by vehicle id. Only the fitted level is here.
- **Cargo capacity.** Never saved: it is resolved from the prefab, the config
  and `m_iCargoTune` at use, so a retune reaches old saves (the Overthrow
  `OVT_StorageRules.ResolveAutoCapacity` lesson). That is why the tuning is a
  step, not the float multiplier the issue first named.
- **In-game minutes.** The issue body named in-game minutes;
  #774 and #778 named unix seconds. Unix seconds survive a restart and a
  time-of-day change, so both stamps use them (see question 2).

### Tow state and the derived impound

`m_iTowState` is explicit; the restart impound stays derived. After a load, a
car that is not stored, has no entity and has `m_iTowState == NONE` is at the
impound lot as today. `IsImpounded` becomes "not stored, no entity" (unchanged)
and the desk reads `m_iTowState` only to choose the fee and the copy: `NONE` is
the restart impound (`m_iImpoundFee`), `TOWED` the tow fee, `POLICE_IMPOUND` the
police release rule. Nothing writes `TOWED` on a restart (see question 3).

## Save-version plan

- Append `vehiclestate` at the end of `RegisterPersistencePayloads`. The save
  version becomes that list's length at merge time: 33 if this lands first, one
  more for each persistence PR that merges before it (#689 is in flight; check
  `RegisterPersistencePayloads` when the PR is opened, never hard-code the
  number in prose other than the version table).
- One row in the version table in `docs/persistence.md` and one header line in
  `LF_PersistenceComponentSerializer.c`: "A version N-1 save includes ... and has
  no vehicle state rows (#752a): every vehicle loads untowed, uninspected, with
  no compartment and stock cargo."
- `validate-persistence-version` sees the append and the bump.
- The `vehicles` payload, `LF_VehicleRecord.VERSION` and its layout do not
  change.

## Load, migration and old saves

- **Order.** `vehiclestate` loads after `vehicles` (it is appended, so it is
  later in the list). `vehicles` `ApplyAll` rebuilds the records, and the
  `[NonSerialized()]` members start at their defaults; `vehiclestate`
  `ApplyState(rows)` then writes onto the live records.
- **Ids.** The row key is the vehicle id that `vehicles` used after
  `LF_SaveIds.ResolveVehicles`. Vehicle ids are lock identifiers, not content
  ids, so the row needs no alias pass. A row whose vehicle id has no record
  (the vehicle was dropped on load) is skipped with one
  `[LFDebug:Vehicle] state row for unknown vehicle <id> skipped` line.
- **Old saves.** A save at version below N has no rows: every record keeps the
  defaults. That matches today's behaviour exactly (no tow, no inspection, no
  compartment, stock cargo), so there is nothing to convert and no log beyond
  the header comment.
- **Bad values.** A tow state outside the enum loads as `NONE`, a negative
  stamp as 0, a compartment or tune step above the config maximum is clamped,
  each with an `LF_Debug.Error` naming the vehicle. One bad row never stops the
  other rows.
- **Idempotent.** `ApplyState` overwrites; running it twice gives the same
  records. A live re-apply runs `vehicles` (defaults) then `vehiclestate`.
- **#689 persistence switches.** Independent: the switches cover fuel, damage
  and trunk contents, which this row does not carry. If an owner switch for
  vehicle state is wanted later, the row is skipped on write and the defaults
  load, as #689 describes.

## Replication and JIP

Nothing in the row replicates. The record stays server-only, as today (only the
plate and the lock state reach clients, through their existing components).

- The garage ledger card (#774) reads slots used and tow lines on the server
  and sends them through the existing ledger read path.
- The inspection sticker (#778) is entity state, not record state: on spawn the
  server sets one replicated int on the vehicle (or swaps a prop) from
  `m_iInspectedAt`. That `RplProp` and its JIP path belong to #778, which must
  prove a late joiner sees the sticker.
- Compartment level and cargo step are read only by server actions and the
  server search roll.

## Test plan

Each test is one file, `// tier:` and `// red-proof:` as `AGENTS.md` asks.

| Test | Tier | Proves | Red-proof |
|---|---|---|---|
| `LF_Test_VehicleStateRoundTrip` | PERSISTENCE | rows written through `SerializeStore`, read back through `DeserializeStore`, all five fields restored on the live record; a default car writes no row | drop `m_iInspectedAt` from the row copy |
| `LF_Test_VehicleStateLegacy` | PERSISTENCE | a save written at version N-1 (and one at 25, the e2e legacy save) loads every vehicle with defaults and the `vehicles` records unchanged | apply a non-default tow state when no row exists |
| `LF_Test_VehicleStateBadRow` | LOGIC | unknown vehicle id skipped with the log line; out-of-range tow state and steps clamped; other rows still applied | let an unknown tow state through |
| `LF_Test_VehicleImpound` (extend) | WORLD | the restart impound still reads `NONE` and charges `m_iImpoundFee`; a `TOWED` record shows the tow fee | |

Restart proof: `tools\cli wt e2e <slug>` with a pass-1 car given
`TOWED`, an inspection stamp and compartment level 1; pass 2 finds the three
values (`e2e/pass*`). `restart/vehicle-street-impounded` keeps passing
unchanged.

Manual testing for the PR: none player-visible (no behaviour). Host, buy a car,
save, restart, collect it at the impound desk: the fee and copy are as before.

## Open questions for the maintainer

1. **Is #752a still M3 work?** With the sidecar shape, the `vehicles` payload
   never changes, so `vehiclestate` could be appended in a minor release like
   any M5 payload (the D5 rule). Landing it now fixes one shared row and the
   units before #774, #778 and #859 each invent their own. Recommend keeping it
   in M3; the alternative is to close #752a and let #774 add the row.
2. **Clock for tow and inspection.** Unix seconds (wall clock: a tow delay and
   an inspection validity keep running while the server is down) or the online
   clock the tickets and jail use (`TickOnline`: they pause)? Recommend unix
   seconds for both; an owner who wants paused validity can set a longer period.
3. **Does a restart write `TOWED`?** Today a restart impounds every unstored
   car (derived, `m_iImpoundFee`). Should it stay a separate state with its own
   fee, or should the load write `TOWED` and charge the tow fee? Recommend
   keeping it derived and separate.
4. **Is cargo tuning in scope?** #778 says "we do not ship tuning". If no
   feature will write `m_iCargoTune`, drop it from the row now; a later field
   costs a new payload. Recommend keeping it (one int, default 0) only if a
   mechanic-bay tuning feature is on the roadmap.
5. **Compartment as a level or a bool?** #778 names one bool; this spec uses an
   int level so a pack can sell sizes. Confirm.
