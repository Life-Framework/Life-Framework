# Persistence review, 2026-11-17

An adversarial review of the whole save system, not only today's diffs. It covered:
- the store serializer and its payload registry (`LF_PersistenceComponentSerializer`, versions 1 to 48), the positional write and read order, the version gates and old-save handling;
- partial-read failure handling (#980), idempotent re-apply on a live session, and the static resets at world end;
- the save orchestration (`LF_PersistenceManagerComponent`), the persistence switches (`LF_PersistenceKeep`), and the wipe path;
- disconnect captures, logout bodies (`LF_LogoutBody`, `LF_LogoutBodyRejoin`), hospital rejoins, and placed storage.

Today's payloads got the closest read: armorykits (v45), houses (v46), itemkeys (v47) and graceclock (v48). jobdeposits (v49, #2480) was merged and then reverted by the main guard, so it is not on `main` (dismissed item 9).

Two passes ran (the `interrogate` skill). The first read the serializer, the registry and today's payload owners. The second, run independently, covered disconnects, logout bodies, the keep switches, static resets and placed storage.

**Result:** 5 confirmed (4 fixed here, 1 filed as #2488), 4 plausible (2 filed as #2489), 19 dismissed.

The format itself holds up:
- write and read walk one registry;
- `validate-persistence-version` ties the written version literal to the list length and requires a write and a read branch for every name;
- every version gate checked here is off by neither one way nor the other;
- every payload added today reads the same shape it writes, and an older save that lacks it keeps a stated default.

The holes are at the edges: a save from a newer build, and a world that ends inside one process.

## Confirmed

### 1. A save from a newer build loads silently, then the next autosave drops its newer payloads (fixed)

**Where:** `LF_PersistenceComponentSerializer.DeserializeStore`. The loop stops at the list length, so a save whose version is above it reads the known payloads, logs `save loaded`, and is not marked damaged.

**Scenario:**
1. A build that wrote version 49 saves (#2480 did, between its merge and the main guard's revert), or an operator rolls a server back to an older build.
2. The older build loads that save. Whatever sits after payload 48 is never read.
3. The next autosave writes version 48 over it, and that data is gone for good.

The same gap would let a later, different payload appended at index 48 read #2480's rows as its own type.

**Fix:** a version above the list length counts as a read failure, with the error `save version N is newer than this build's M`. `ReportLoad` then marks the load damaged, so autosave pauses, and on the boot load saving is turned off. The known payloads still load. This is the #980 fail-safe applied to this case.

**Test:** `LF_Test_SaveNewerVersion` (`persist/save-newer-version`, PERSISTENCE). It writes the current store, raises its version by one, loads it, and asserts that the bank balance in it loads, that the read counts as a failure, and that the load is damaged.
- Red (`--red`, version left at the current one): `the save claims the newer version (expected 49, got 48)`, `the unread newer payloads count as a read failure` and `a newer save pauses autosave` fail.
- Green with the fix.

### 2. `WasStoreLoaded` survives the end of a world (fixed)

**Where:** `LF_PersistenceComponentSerializer.s_bStoreLoaded`. Nothing reset it. `LF_SaveStamp`'s hook cleared only the damage flag.

**Scenario:**
1. In one process (a scenario restart, a Workbench play), world 1 reads its store.
2. World 2 continues a save that the engine drops (a game update did that in #980).
3. At `ACTIVE`, `WasStoreLoaded()` is still true from world 1, so `MarkStoreMissing` never runs.
4. World 2 runs fresh with autosave on and overwrites the save on disk.

**Fix:** `LF_PersistenceComponentSerializer.ResetStatics()` clears the flag and the damage state, through `LF_PersistenceComponentSerializer_StaticResetHook`.

**Test:** `LF_Test_PersistStoreStaticReset` (`persist/store-static-reset`, WORLD). Red with the reset skipped; green with the hook.

### 3. A logout-body rejoin launders unpaid store goods (fixed)

**Where:** `Feature/Character/LF_LogoutBodyRejoin.c` `TakeBack`.

**Scenario:**
1. On disconnect, `LF_Stores.ParkCarrier` moves the tags of the leaver's unpaid goods off the live list and parks them under the character key.
2. The only place that puts them back on the body is `LF_SpawnLogic.OnCharacterCreated`, which a take-back never reaches (`TakeBackBody` returns before it).
3. A shoplifter logs out inside a store and rejoins within the 5-minute window. They get the same body back with the goods untagged, walk out, and owe nothing.

**Fix:** `TakeBack` calls `LF_Stores.ReattachUnpaid(uid, body)` on the body it hands back. The restored-tag grace and sweep then apply as on a fresh spawn.

**Test:** `LF_Test_LogoutBodyUnpaidTakeBack` (`rejoin/logout-body-unpaid-tags`, WORLD). A body with a parked tag for a used-up apple is kept and taken back, and it must still owe the store.
- Red on the unfixed code: `the taken-back body still owes the store (expected 7, got 0)`.
- Green with the fix, alongside `rejoin/logout-body-takeback` and `persist/store-unpaid`.

### 4. Bank accounts, wanted reports and pending farm rows have no world-end reset (fixed)

**Where:** `LF_ATMManager` (`Reset`), `LF_WantedReports` and `LF_FarmPlots` (`ResetStatics`). No hook and no teardown call reached them; the game mode destructor resets only the account manager and runs `LF_StaticReset.ResetAll`.

**Scenario:** in one process, the next world starts with the last world's bank balances, posters and their sequence, and pending crops. When that world loads no save over them (a fresh world), its next save writes them out as its own: money carried across worlds. A dedicated server's process restart is not affected.

**Fix:** an `LF_StaticResetHook` per owner, in its own file (`LF_ATMManager_StaticResetHook`, `LF_WantedReports_StaticResetHook`, `LF_FarmPlots_StaticResetHook`).

**Test:** `LF_Test_PersistOwnersStaticReset` (`persist/owners-static-reset`, LOGIC). The bank and wanted hooks clear their stores, and all three are registered. The farm hook is only checked as registered, because it also forgets the live plot components.
- Red (`--red`, hooks skipped): `a world end drops the bank accounts`.

While adding the farm hook: `LF_FarmPlots.c` held a raw carriage return in a doc comment (the `\r` of `\return`). It is now `\return`.

### 5. `keepCarriedGear` and `keepCash` leave the duty-locker snapshots (filed #2488)

**Where:** `Persistence/LF_PersistenceKeep.c:33-45`. `ClearSlot` and `StripPrefab` touch only `SLOT_CARRIED`. The `civilian_stash`, `police_duty` and `medic_duty` snapshots ride the same payload and survive.

**Scenario:** the owner turns `keepCarriedGear` off. An officer on duty at the save clocks out after the restart and gets back their civilian gear and cash from before the wipe.

**Why filed:** clearing `police_duty` also has to clear that officer's outstanding serials in the armory book (#2416), or the officer can never draw a kit again. That is a design call.

## Plausible

1. **A failed payload read does not stop the load (#2489).** After a failed `context.Read`, `DeserializeStore` keeps reading and applying every later payload. In a positional binary context those reads can be misaligned, and one that happens to succeed replaces live state. Saving is off after a damaged boot load, but the session runs on that state until the operator restores.
2. **Damage pauses only the autosave timer (#2489).** `IsLoadDamaged()` gates `OnAutosaveTimer` and the boot `ACTIVE` transition. A damaged re-apply later in the session (`ReapplyLatestSaveData`) leaves `SaveGame()` and the engine's shutdown save free to overwrite the save. Only tests call `SaveGame()` today.
3. **A logout body's snapshot can go stale on an in-place quantity change.** Recapture follows the item added and removed events only. `LF_InventoryUtils.RemoveAmount` lowers a stack without either, and `LF_LogoutBody.RecaptureAll` has no caller outside tests. If anything lowered a cash stack on a body, a restart inside the window would restore the old amount. No caller does that to a body today, so this is latent; no issue.
4. **A placed storage deleted in the same frame as an item change drops its pending capture** (`LF_PersistentStorageComponent.c:114-118`). Low impact; no issue.

## Dismissed

1. **Today's payloads read in a different order from the one they are written in.** The write and read branches both follow `RegisterPersistencePayloads`. armorykits, houses, itemkeys and graceclock each write one array and read one array of the same class, and `validate-persistence-version` requires both branches.
2. **Version gates off by one.** The `wanted` prune, the job-levels migration, the grace seed, the character-slot note and the legacy orgranks write all compare the version against `order.Find(name)` (zero-based) with the right operator. For example, the graceclock seed runs for `version <= 47`, and version 48 is the first save with the payload.
3. **Old saves before v45–v48.** The armory book starts empty and houses keep their editor state. Keys come back unbound. The grace clocks are seeded from account playtime (`SeedFromAccounts`) when the payload is missing and also when its read fails, so neither case gives a veteran a fresh grace.
4. **Idempotent re-apply of today's payloads.** `LF_ArmoryKits.ApplyAll`, `LF_Houses.ApplyAll` and `LF_GraceCharacterClock.ApplyAll` and `SeedFromAccounts` rebuild their stores from the rows. A live house the rows do not name keeps its state, by design.
5. **Missing static resets for today's owners.** `LF_ArmoryKits`, `LF_Houses`, `LF_GraceCharacterClock`, `LF_SaveIds`, `LF_SaveStamp`, `LF_HospitalRejoin`, `LF_LogoutBody` and `LF_PersistentStorageComponent` all register a hook.
6. **Two parallel PRs claim the same version.** The serializer has no merge driver, so both appends conflict in git, and `validate-persistence-version` fails a written literal that differs from the list length.
7. **Houses applied before their house exists.** `LF_Houses` keeps a row until its house registers, and the other way round. A row whose house is gone is saved again.
8. **Character-keyed payloads skip `LF_SaveIds`.** That resolver aliases content ids (prefabs, places, jobs); character record keys are never aliased.
9. **jobdeposits v49 (#2480).** Not on `main`: the main guard reverted it (#2483) after `e2e/pass2-verify-load` failed twice. The failing assertion is no longer in the guard machine's logs. Sign-ups do not restore `m_bActive`, so the refund is not skipped for that reason. The re-land must pass `tools\cli wt e2e`. A v49 save written in that window is now caught by confirmed 1.
10. **A disconnect captures twice, or after the items left.** The chain is `LF_DisconnectRules`, `LF_BleedOut`, `ReleaseDisconnectedPlayer`, then `LF_LogoutBody.Keep`, and every step is idempotent. `LF_Contraband.Fail` removes no items.
11. **A logout body plus its snapshot duplicates items on rejoin.** The per-body listener keeps the snapshot in step. `Release` recaptures and deletes the body; `Detach` hands it over without restoring the snapshot.
12. **A restart with a body pending duplicates items.** A body is never saved, so the restart restores only the snapshot.
13. **Downed logout.** `BleedOut.Finish` kills the character before the capture, so the snapshot drops and no body is kept. The hospital row survives through `LF_HospitalRejoin`.
14. **`LF_HospitalRejoin.ExportAll` changes rows while exporting.** It drops only consumed rows, which is intended.
15. **Keep switches and today's payloads.** armorykits, houses, itemkeys and graceclock have no switch, and the header says so. Clearing the carried snapshots also drops their `itemkeys` rows, which run parallel to the snapshot items.
16. **Wipe.** `WipeSave` only purges save points and has no caller outside tests. `LF_SaveSelection` is pure.
17. **`ApplyItemKeys` / `ExportItemKeys`.** The field is `[NonSerialized()]`, so the carried shape is unchanged. Apply checks the item count, skips a mismatch with a warning, and `SetHouseKey` is idempotent.
18. **Houses re-apply leaves an unnamed live house alone.** This matches the "keeps editor state" contract of an old save.
19. **Playtime and the grace clock on disconnect.** Both are credited through the invoker with the clock's stored uid and character, and both reset at world end.

## Verification

- `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_PersistStoreStaticReset`: RED PROOF OK.
- `tools\cli test --no-build --red LF_Test_PersistOwnersStaticReset,LF_Test_LogoutBodyUnpaidTakeBack`: RED PROOF OK. The take-back test ran against the unfixed `TakeBack`.
- `tools\cli test --no-build --only` with the four new tests plus `LF_Test_LogoutBodyRejoin`, `LF_Test_StoreUnpaidPersist`, `LF_Test_SaveStampPersist`, `LF_Test_StaticResetHooks` and `LF_Test_HeldGroundLegacy`: 9 passed.
- The full suite and the restart e2e were not run here, to keep game runs down. The main guard runs both on the merge.
