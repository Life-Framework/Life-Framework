# PR Integration Plan — EveronLife PRs & Forks into Life-Framework

> Source of truth for the port of unmerged EveronLife work into this repo.
> Companion to [test-plan.md](./test-plan.md) and [features.md](./features.md).
> Goal: land usable, tested, DebugWorld-verifiable features from the 9 open
> EveronLife PRs, ordered by ease and readiness. Parallel sub-agents do the
> porting; the main thread does the integration, the DebugWorld asset setup,
> and the final manual test walkthrough.

## End state

A reviewer can, in one DebugWorld session near the spawn point:

1. Get in a vehicle, drive, and watch its license plate follow the car.
2. Pick up an ID card item and inspect it in the inventory.
3. Pick up a cash stack and see the reworked single-bill model.
4. Lock a vehicle with its key, watch the storage deny access, unlock it.
5. Enter a police vehicle, cycle the siren modes, and see lights sync.

Each feature ships with automated tests (LOGIC where pure, WORLD where it
touches a prefab/world, PERSISTENCE reserved for the save/reload path), each
test carries a `// red-proof:` comment proven by an actual red run, and the
full suite passes `tools\cli test`. The DebugWorld fixtures are placed by the
main thread via Workbench MCP so the human only walks and observes.

## Principles applied (from life-mode)

- **sequence-verifiable-units.** Every workstream ends in a check: red run,
  then green run, then `cli validate`, then in-game pass. No stream merges
  without its checks.
- **laziness-protocol.** Port the smallest slice that makes the feature real.
  Do not "modernize" upstream code beyond what the port forces.
- **guard-the-context-window.** Sub-agents do the bulk reads and the porting;
  they return summaries, not raw file dumps.
- **prove-it-works.** The DebugWorld pass and the observed red runs are the
  proof, not a compile.
- **make-operations-idempotent.** A re-run of any workstream converges to the
  same end state; nothing depends on a prior run's side effects.
- **boundary-discipline.** Replication state, RPCs, and persistence values are
  validated at the network/config boundary and trusted inside, per AGENTS.md.

## Hard rules for every workstream

- Follow `AGENTS.md` golden rules and the hard EnforceScript lessons. No
  ternary, no null-coalescing, `ref` on Managed collections and elements,
  `EntityID` stored not `IEntity`, server-side validation of client RPCs.
- Every new `.c` file under `Scripts/Game/` must carry the `// red-proof:`
  comment with a real observed red run, and the `EL_` class prefix.
- Every new resource needs a `.meta` with a unique 16-hex GUID. Use the MCP
  `game_duplicate` / Workbench registration paths, never hand-duplicate GUIDs.
- Prefabs reference resources by `{GUID}path`, folder casing included.
- No commits and no pushes from sub-agents. Leave changes in the working tree
  (or a branch) for the main thread to review and stage.
- Every stream runs `tools\cli validate` after its edit, and `tools\cli test
  --tier fast` for any LOGIC-tier change.
- The final manual steps for a stream ship with it, per AGENTS.md.

---

## Phase 0 — Baseline gate (main thread, before any parallel work)

Confirm the repo is green before porting anything.

1. `tools\cli status` — toolchain, MCP, environment paths present.
2. `tools\cli validate` — repo consistency checks pass.
3. `tools\cli test --tier fast` — LOGIC suite boots the DebugWorld server and
   passes. Record the pass count.

Note: `test-plan.md` (2026-08-29) marks several WORLD-tier red runs as
"pending the first build baseline after EPF dependency removal." The code has
no `EPF_` references left, but the `.gproj` still lists dependency
`58D0FB3206B6F859`. If `cli test --tier fast` fails to boot, resolve the
dependency chain FIRST and get the fast tier green before starting Phase 1.
Phase 1 streams can still author and register tests during the wait, but
their red/green proof is only meaningful on a bootable baseline.

Gate: fast tier green. Record the starting pass count in this file (below).

> Baseline: (fill in after Phase 0)

---

## Phase 1 — Parallel workstreams (sub-agents, `subagent_type: life-agent`)

| WS | Source | What it ports | Effort |
| --- | --- | --- | --- |
| WS-1 | PR #232 zalexki `patch-1` | License plate parenting fix | trivial |
| WS-2 | PR #115 12gabriel3 `id-card` | ID card model + prefab + materials | small |
| WS-3 | PR #201 Pyth3rEx `main` | Cash single-bill model/texture rework | small |
| WS-4 | PR #238 dan-schwerner `feature/revive-vehicle-locking` | Vehicle lock + key + inventory lock | medium |
| WS-5 | PR #142 12gabriel3 `siren` | Emergency siren + lights system | large |

WS-1, WS-2, WS-3 are fully independent and can run in parallel. WS-4 owns
`Prefabs/Vehicles/Core/Vehicle_Base.et` (adds the lock component). WS-5 must
NOT touch `Vehicle_Base.et`; it builds a new police vehicle prefab that
inherits the base M151A2. This keeps WS-4 and WS-5 parallel. All five touch
`Scripts/Game/Tests/EL_TestManager.c` (one registration line each) — that is
the single merge point, resolved in the integration pass (Phase 1.5), not by
the streams fighting over it. Each stream adds its test file and appends its
registration after the last existing `Register(...)` line; the main thread
merges them.

Shared-file rule: a stream that finds another stream's change in a file it
must edit stops and reports to the main thread instead of overwriting.

### WS-1 — License plate parenting fix (PR #232)

**Source.** zalexki/EveronLife `patch-1` branch, one commit: remove the
`veh.AddChild(...)` call. The local tree still has the buggy line:
`addons/LifeFramework/Scripts/Game/Components/Vehicle/EL_LicensePlateManagerComponent.c:57`
(`veh.AddChild(plate.m_Object, -1, EAddChildFlags.RECALC_LOCAL_TRANSFORM)`).
Spawning with `params.Parent = owner` (line 37) is enough; the double-parent
breaks the plate following the car.

**Scope.** Delete line 57. Nothing else. Do not touch the plate entity or the
generator.

**Test.** `EL_Test_LicensePlateParenting` (WORLD tier) in
`EL_Test_LicensePlate.c` or a new `EL_Test_LicensePlateParenting.c`:
1. Load `{E95486C43308F36B}Prefabs/Vehicles/LicensePlate/LicensePlate.et` and
   assert it resolves (mirror the pattern in `EL_Test_Prefabs`).
2. If a vehicle prefab carrying `EL_LicensePlateManagerComponent` can be
   spawned headless, spawn it and assert the spawned plate's parent equals the
   vehicle, and that exactly one plate exists per configured point.
3. If headless vehicle spawn proves infeasible, document that in the test's
   header and make the in-game pass the proof (per el-tdd, prefab-wiring fixes
   legitimately land on the in-game pass). The red-proof comment records the
   perturbation either way: re-adding the `AddChild` line makes the plate not
   follow the car.

**In-game proof.** Spawn a vehicle, drive, plate tracks the body.

### WS-2 — ID card model (PR #115)

**Source.** 12gabriel3/EveronLife `id-card` branch. The PR is asset-only but
its branch carries stray license-plate commits Arkensor flagged — port ONLY
the ID card model, materials, and prefab.

**Scope.**
1. `game_duplicate` the IDCard prefab (or copy model + `.emat` + `.et`) into
   `Prefabs/Items/Roleplay/IDCard.et` under a NEW GUID via Workbench
   registration. Base it on the local `RoleplayItem_Base.et` item convention
   so it fits an inventory slot.
2. Place model/materials under `Assets/Items/Roleplay/IDCard/` with unique
   GUIDs. Match the material naming the review asked for (no generic
   `material.emat`).
3. Verify the model is pick-up-able (the branch contains the "fix not being
   able to pick up item" commit — preserve it).

**Test.** Extend `EL_Test_Prefabs`' prefab list with the new IDCard prefab
(WORLD tier). Red-proof: a nonexistent path in the list fails the load
assertion.

**In-game proof.** Item on a crate, pick up, appears in inventory, equips.

### WS-3 — Money rework (PR #201)

**Source.** Pyth3rEx/EveronLife `main` branch. The PR target tree has
diverged; port the final asset state onto the LOCAL `MoneyStack.et`
(`{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`) and its material.

**Scope.**
1. Replace the money stack model/texture with the reworked single $1 bill.
2. Apply the outstanding review feedback, which blocks merge upstream:
   - Texture file named `CashTexture_BCR.tif` (suffix drives the import
     profile).
   - Single NMO texture instead of displacement + normal.
   - Material named `CashMaterial` (not `material`).
   - No green tint in the inventory render (remove the translucent layer the
     inventory dislikes).
   - Texture small (review target: ~under 100 KB for a small item).
3. Re-register all new assets with unique GUIDs.

**Test.** Extend the WORLD-tier money prefab test (or `EL_Test_Prefabs`) to
assert the reworked prefab's material resolves and the texture loads. A
LOGIC-tier test is not meaningful here; the visual contract is the in-game
pass. Red-proof: reference a wrong texture/material path.

**In-game proof.** Give cash, check first-person view AND the inventory icon
(not green), both directions.

### WS-4 — Vehicle locking & inventory locking (PR #238)

**Source.** dan-schwerner/EveronLifeLocking `feature/revive-vehicle-locking`.
This is the revival of njbrown09's #71 and is marked ready for review (Apr
2026). Read the PR conversation first: the author's known gaps are persistence
("wonky when leaving and rejoining") and key inventory presentation.

**Scope.**
1. Port the scripts: `EL_VehicleLockComponent`, `EL_VehicleKeyComponent`,
   `EL_VehicleLockAction`, and the storage access control. Per Arkensor's #71
   review, the open-storage restriction should be a `modded` override of
   `SCR_OpenVehicleStorageAction`, not a replaced action class.
2. Add the lock component to `Prefabs/Vehicles/Core/Vehicle_Base.et` so every
   vehicle is lockable (per Arkensor).
3. Add a key item prefab under `Prefabs/Items/Vehicles/` on the local item
   base, wired with `EL_VehicleKeyComponent`.
4. Fix the persistence gap the author flagged if the port exposes it: the key
   should store the vehicle's persistent id, not a per-session id, and the
   lock state should survive a restart. If the local persistence API makes
   this non-trivial, ship the in-session behavior and record the restart gap
   explicitly in the manual test plan (AGENTS.md: in-session round trips prove
   nothing about restarts).

**Tests.**
- LOGIC: key↔lock identifier matching (pure string compare extracted as a
  helper), lock state toggle on the plain-logic surface if one exists.
- WORLD: spawn a locked vehicle + key, assert storage access control denies a
  non-key holder and allows with the matching key. Mirror the headless spawn
  pattern in `EL_Test_MoneyCash` (it already spawns a character).
- PERSISTENCE: if the local save/reload API can be driven through public
  accessors, add a lock-state round trip. Otherwise document why it is
  deferred and cover it in the manual restart pass.

**In-game proof.** Lock a vehicle, try opening its storage (denied), unlock
with the key, re-enter and open storage (allowed). Then the restart proof:
lock, restart the server, verify still locked.

### WS-5 — Siren system (PR #142)

**Source.** 12gabriel3/EveronLife `siren` branch. 108 commits, four years
stale. Port the FINAL state only, adapted to current APIs. Read the PR
conversation: Arkensor's review asked for scripts under
`Scripts/Vehicle/SirenLights` (the `Scripts/Game/Components` folder should not
exist) and the `Attachments` asset folder structure. The branch already
contains the "Fix siren not working on 9.6" commit — keep it.

**Scope.**
1. Port the siren scripts to `Scripts/Game/Feature/SirenLights/` (or the
   established feature-folder convention): `EL_SirenManagerComponent`,
   siren mode enum, the knob/action wiring, light manager.
2. Port the assets: police M151A2 variant prefab (inherits the base M151A2,
   does NOT touch `Vehicle_Base.et`), light bar/rotary models, materials,
   audio. Register all with new GUIDs.
3. Rebase against current APIs; do not modernize beyond what the port forces.
4. Keep the known horn-override UX note (vehicles without siren lose the
   quick-tap horn) as a documented limitation in the PR description; do not
   fix it in this pass unless trivial.

**Tests.**
- LOGIC: `EL_SirenMode` mode array integrity and any pure mode-cycling
  helper.
- WORLD: spawn the police vehicle prefab, assert `EL_SirenManagerComponent`
  resolves and a mode toggle changes the light state on the server.
- Red-proof on both.

**In-game proof.** Enter the police vehicle, cycle siren modes, lights + audio
change; second client sees the same lights (MP sync).

### Explicitly out of scope (do not port)

| PR | Why skipped |
| --- | --- |
| #186 Item shop (thebonbon) | Local `Scripts/Game/Feature/Shop` already implements item shops with persistence; DebugWorld uses the ADM shop system. Mine the branch's `docs/feature/itemshop/stores.md` for config ideas only. |
| #195 Bank system (thebonbon) | Local Banking/ATM (`EL_ATMManager`, `EL_BankAccountComponent`, `EL_BankAccount`) already exist with persistence and tests. |
| #188 Garage + vehicle shop (thebonbon) | Shop is covered locally; the garage half is an incomplete draft (missing UAZ parts, planned rework). Possible future harvest for vehicle-ownership/garage logic, not this pass. |
| #71 Vehicle locking (njbrown09) | Superseded by #238. Do not port. |

---

## Phase 1.5 — Integration pass (main thread)

After the streams report done:

1. Review each diff against AGENTS.md (GUID uniqueness, `{GUID}path` casing,
   ternary/ref/EntityID lessons, layout GUIDs if any UI).
2. Merge the `EL_TestManager.c` registrations (the one shared-file conflict
   point).
3. Run `tools\cli validate`, then `tools\cli test --tier fast` (LOGIC) and
   `tools\cli test` (full WORLD suite). Every new test must be green.
4. For each new test, spot-check its `// red-proof:` comment claims a real
   observed red run, not a hypothetical one.
5. Run an adversarial pass via the **interrogate** skill on the WS-4 diff
   specifically (replication, boundary guards, key persistence) before it is
   considered done.

Gate: `tools\cli ci` green.

---

## Phase 2 — DebugWorld asset setup (main thread, Workbench MCP)

Placed by the main thread so the human only walks. Player spawn is at
`(110, 0, 137.5)`; the test runner sits at `(109.995, 0.001, 136.617)`. New
fixtures go in a dedicated layer `DebugWorld_Layers/PRFeatures.layer`, within
easy walking distance of the spawn, following the coordinate cluster already
used by ATM/Shops/Police (120-126, 0, 130-136).

| Fixture | Layer | Approx coords | What to place |
| --- | --- | --- | --- |
| License plate proof | existing `Shops.layer` / new | 128, 0, 136 | A drivable vehicle prefab (base game duplicate with plate component). |
| ID card proof | `PRFeatures.layer` | 115, 0, 134 | One `IDCard.et` on a crate. |
| Money proof | `PRFeatures.layer` | 116, 0, 134 | One `MoneyStack.et` on a crate. |
| Vehicle lock proof | `PRFeatures.layer` | 130, 0, 132 | A lockable vehicle (base vehicle with lock component) + a matching key item on a nearby crate + a mismatched key to prove denial. |
| Siren proof | `PRFeatures.layer` | 132, 0, 132 | The police M151A2 variant with the siren system. |

Setup procedure (main thread):
1. Launch Workbench (`wb_launch` with the DebugWorld open), ensure edit mode.
2. Place each fixture with `wb_entity_create` using the ported prefabs'
   `{GUID}path`.
3. Position crates/vehicles, then `wb_save`.
4. Boot the DebugWorld server once (`tools\cli serve` or `cli test
   --no-build`) and confirm the layer loads with no `(E)` errors and the new
   prefabs resolve.
5. `wb_cleanup` the injected MCP handler scripts before the user publishes.

Note: `Worlds/DebugWorld/DebugWorld.ent` is currently 0 bytes while the
`DebugWorld_Layers/*.layer` files exist. Confirm during setup that saving via
Workbench writes the sub-scene references the ent needs, and that a boot loads
the layers. If the ent needs rebuilding, do it in Workbench before placing
fixtures.

---

## Phase 3 — Final manual test plan (human in DebugWorld)

Run as the host (Workbench play or hosted server). Start at the spawn point,
`(110, 0, 137.5)`. All fixtures are within ~30 m.

### 3.1 License plate follows the car

1. Walk to the proof vehicle at `128, 0, 136`. Enter it.
2. Drive a straight line at speed for 20+ m.
3. **Observe:** the rear license plate stays glued to the bumper at all
   times, matching the car's roll/pitch.
4. **Fail if:** the plate lags behind, floats in place, or drifts when the car
   turns. A restart is not required for this check.

### 3.2 ID card item

1. Walk to the crate at `115, 0, 134`. Pick up the ID card.
2. Open the inventory. Inspect the card.
3. **Observe:** the card occupies an inventory slot, shows a sane icon, and
   equips into a roleplay/hand slot.
4. **Fail if:** the card cannot be picked up, is invisible, or has no
   inventory representation.

### 3.3 Money rework

1. Walk to the crate at `116, 0, 134`. Pick up the cash stack.
2. Hold it in first person and open the inventory.
3. **Observe:** a single bill (not a green-shaded stack) in hand; the
   inventory icon is the same bill with no green tint.
4. **Fail if:** the bill looks green in the inventory or the model is
   oversized/undersized. No restart needed.

### 3.4 Vehicle locking

1. Walk to the lockable vehicle at `130, 0, 132`. Take the matching key from
   the crate next to it.
2. Use the lock/unlock action on the vehicle. Lock it.
3. **Observe:** the vehicle shows locked state; the inventory/storage action is
   greyed or denied for you when not holding the key (hand the key to a second
   player or drop it to test denial).
4. With the key, unlock; open the storage.
5. **Restart proof (required):** lock the vehicle, restart the server, rejoin.
   Verify the vehicle is still locked and the key still matches. In-session
   round trips prove nothing about restarts — this is the ground-truth check.
6. **Fail if:** a wrong key unlocks, a non-holder opens the storage, or the
   lock resets on restart.

### 3.5 Siren system

1. Enter the police vehicle at `132, 0, 132`.
2. Cycle the siren modes (knob/action) through off → mode 1 → mode 2.
3. **Observe:** roof lights and any rotary lights change pattern per mode and
   the audio matches. A second client sees the same lights (MP sync).
4. **Fail if:** lights stay dark, modes don't change, or audio doesn't follow.
5. Known limitation (documented, not a fail): in a vehicle with no siren, the
   horn may require a longer press.

---

## Phase 4 — Final gate & release

1. `tools\cli ci` fully green (validate + build + test).
2. Manual test plan above executed by the human, results recorded.
3. Restart + late-joiner proofs done for WS-4 (lock persistence).
4. `wb_cleanup` run; no MCP handler scripts left in the addon.
5. Commits staged by the main thread, one per workstream, referencing the PR
   source and the port decision. No generated/artifact files committed
   (`.gproj.user`, `resourceDatabase.rdb`, `*.rdb.lock`, `log`, MCP clones).

## Success criteria

- All five features reach the DebugWorld walkthrough.
- Every new test is registered, green, and carries a proven red-proof.
- `tools\cli ci` green on the merged tree.
- The manual walkthrough (Phase 3) passes end to end, including the WS-4
  restart proof.