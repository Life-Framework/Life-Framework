# Revert audit, 2026-11-27

The main guard has reverted 22 merges since it started on 2026-11-15 (`tmp/logs/main-guard.log`, `revert PR for #N`;
`git log --grep='^Revert #'` lists the 16 revert commits still on `main`, the rest were re-landed over). This page groups
them by cause, says which a cheap check could have caught before the merge, and records what was added. No game code
changed.

Sources: the guard log (which step failed, which test, the bisect), the reverted merge's file list
(`git diff M^1 M`), and the re-land or fix commit that followed (its message names the cause). The guard's own failure
logs for the early reverts were pruned, so a cause marked *inferred* comes from the fix commit alone.

## The causes

| Class | Reverts | Why GitHub's `validate` and `wt ship` missed it | Added |
|---|---|---|---|
| **A. Pack or core data pinned by a test that nobody ran** | 8 | `validate` runs no game. The pack leg (`everon/*`, 175 tests) and the sign tests boot only in the guard's `--tier all`. A pack test reads the merged config, so it rarely names the file it pins | `wt ship` prints the tests; `validate-mannequin-outfits` for #5096 |
| **B. Persistence or save-version change** | 3 | Needs a restart: the guard's pre-move e2e, or a `PERSISTENCE` test. Two PRs also took the same save version | `wt ship` prints `--tier persistence` and `wt e2e` |
| **C. Test or tool change that passes alone and fails in the full run, or a test's own bug** | 6 | A fixture left by another test, a test that wiped every feature's statics, a registration shape, the world pin list. Visible only in `--tier all` or in the guard's world | `wt ship` prints `--tier all` for pin lists; two classes already have validators |
| **D. Infra, not the PR** | 4 | A held build lock, a bisect that blamed the oldest of six red merges, a test-only fix reverted because the data it matched had just been reverted | Nothing a PR check can do (below) |
| **E. Placement audit** | 1 | The audit boots a world | `wt ship` already runs the island gate (#3888); it now also prints the audit for any layer |

## Every revert

| PR | Guard step and failing test | Cause | Class | Cheap catch |
|---|---|---|---|---|
| #1867 town-maintainer-edits | placement audit | Maintainer layer edits (DebugWorld and island layers) tripped `audit placement --strict` | E | Island gate since #3888; ship prints `audit placement --strict` for a layer |
| #1899 clothing-pipeline | suite, `store/folded-stack` | The test measured the e2e's leftover trousers pile on the counter; fixed in the test (`76e7a921b`) | C | `--tier all` only; no cheap check |
| #1910 slice-1893 | suite | The test-world pin list was emptied: the plan became one whole-registry leg, and the town tests ran in it (`eded6b5a9`, `c82ca8234`) | C | Ship prints `--tier all` when `world-pins.json` or `town-tests.json` changes |
| #2011 slice-1920 (ImpairmentView) | suite | The statics reset hook was registered the old way; it became an `LF_StaticResetHook` subclass (#2042, #2755) | C | `validate-registries` now fails the old way |
| #2073 slice-1953 (EventScenePlan) | suite | The test called `ResetAll()` and wiped every feature's statics (fix `f0a7d6f56`) | C | `validate-no-test-resetall` now fails it |
| #2117 slice-2100 (roadblock) | suite | The test laid a roadblock set onto the annex fence (fix `8a9bf9c54`) | C | A test fixture bug; `--only` plus `--tier all` |
| #2376 town-tool-fixes | suite, `grid/frame-labels` | The lattice change skipped markers that main already placed on a claim edge (#2278); the test reads `Grid_frame.layer` | C | `--only LF_Test_GridFrameLabels` names it through `Grid_frame` |
| #2480 fix-review-followups | pre-move e2e | `VerifyJobDeposit` asserted a courier account the guard's reference build (`502ebf39`) never wrote | B | Ship prints `--tier persistence` and `wt e2e` |
| #2798 slice-2659 (lockers) | pre-move e2e | Lockers and plants both claimed the next save version; the serializer read branch was left open after the merge (`8ce2e570f`, `e37d82a33`) | B | Same line; `record-shapes.json` is a persistence file |
| #2369 town-draft-saint-philippe-clothing | build | The build was red for six merges in a row; the bisect reverted the oldest, a layer-only PR | D | None. Not this PR |
| #2881 slice-2855 (chains) | build | Held build lock, reported as a red build ("INFRA, not a red": the worker ran code from before #2763's lock classifier) | D | None; fixed in the guard (`278950af5`) |
| #2887 slice-2847 (price board) | build | Same | D | None |
| #3008 slice-2998 (café owner) | suite, `everon/cafe-owner` | Pack dialogue now has four mood greetings; the test counted the old ones (`a08961505`) | A | Pack leg |
| #3017 fix-cafe-owner-test | suite, `everon/cafe-owner` | *Inferred:* a test-only fix for the data #3008's revert had just removed, so it failed against the reverted data | D | None; the lead re-lands both in order |
| #3653 slice-3297 (harvest) | suite, `harvest/persist-owned-implements` | The test read the payload section key unquoted; the quoted key broke it (`f04182ed0`) | B | `--tier persistence` (the test is `PERSISTENCE`) |
| #3767 slice-3440 (shipment quest) | suite, `everon/reputation` | A new pack reputation conf row the reputation test rejected (fix #3777) | A | Pack leg |
| #3805 slice-3464 (anniversary memorial) | suite, `everon/reputation` | The `anniversary_memorial` award row had no core code raising it; the test's published-action rule rejected it (`446f7ddf7`) | A | Pack leg |
| #3835 slice-3334 (repair kit stock) | suite, `everon/village-stock`, `everon/market` | New pack shop stock rows the two stock tests did not expect (`309ae3d02`) | A | Pack leg |
| #4096 map-clean-howto | suite, `everon/map-places`, `everon/mining-loop` | New place pictograms; both tests pinned the old (`71d393154`) | A | Pack leg |
| #4899 town-draft-coalmans-brow-mine-board | suite, `signs/next` | The new shift board sign card had no Next line; core `LF_Signs.conf` plus a new prefab (`1d6bf940e`) | A | `--only` names `LF_Test_SignNext` through `LF_Signs` |
| #5074 fix-copper-permit | suite, `everon/job-tables` | Miner level 3 lost its copper licence gate; the test expected one gate per level (`c5caf9ece`) | A | Pack leg |
| #5096 shop-ui-polish-1127 | suite, `everon/mannequins` | The pack dropped its `jacket_raincoat_green` clothing offer; the raincoat mannequin still wore it (`b21eb4264`) | A | Pack leg; `validate-mannequin-outfits` fails it in `validate` |

Counts: A 8 (#3008, #3767, #3805, #3835, #4096, #4899, #5074, #5096), B 3 (#2480, #2798, #3653), C 6 (#1899, #1910,
#2011, #2073, #2117, #2376), D 4 (#2369, #2881, #2887, #3017), E 1 (#1867). That is 22. Every revert has a cause from
a fix commit or a re-land message; #3017 and #2369 are inferred.

## What was added

**1. `wt ship` prints the tests a diff can break** (`tools/lib/touched-tests.mjs`, wired in `ship-direct.mjs` and
`tools/cli.mjs`, proved by `touched-tests-selftest.mjs` on the real file lists of #5074, #5096, #4899, #3653 and
#1910, and by `ship-direct-selftest.mjs`). It prints before the preflight, never fails a ship, and works from the
changed file list and the real test sources:

- a changed test file runs its own class;
- a changed file runs every test whose source names its stem, a class it declares, or a string Id the diff adds;
- **any pack data change prints the whole pack leg** as one `tools\cli test --no-build --only <175 classes>`: one boot,
  about 40 s. Naming only the tests that mention the file would not have caught #5074 (`everon/job-tables` names no
  conf) or #5096 (`everon/mannequins` reads the catalogue through the prefab);
- persistence files (`Persistence/`, `*SavePayload*`, `record-shapes`) add `--tier persistence` and `wt e2e`;
- a world layer adds `audit placement --strict` (and `--grid --strict` under GridWorld);
- `tools/test/world-pins.json` or `town-tests.json` adds `--tier all`;
- a core script or data change that no test names adds `--tier fast`.

Replayed against the real merges, the pack leg line contains `LF_Test_EveronJobTables` for #5074 and
`LF_Test_EveronMannequins` for #5096, the named list contains `LF_Test_SignNext` for #4899, and #3653 prints the
persistence line.

**2. `validate-mannequin-outfits.mjs`** (`mannequin-outfits-selftest.mjs`): the `everon/mannequins` rule read from the
files, so `validate` fails in a second instead of the guard failing after the merge. Every prefab with an
`LF_MannequinOutfitComponent` must wear only live, priced catalogue items its outlet offers (merged core then overlays,
`m_bRemoved` and licence rows honoured), and at least two. Red observed on the real tree with the raincoat mannequin
set back to `jacket_raincoat_green`: `LF_Mannequin_Raincoat.et wears jacket_raincoat_green, which clothing_rack does
not sell`.

`parseCatalogue` (`validate-item-catalogue.mjs`) now also returns `removed` for items and offers.

## What no cheap check catches

- **Pack tests that pin values, not files** (#5074's "one gate per level", #3008's greeting count): the test is the
  check and the data moved. The fix is to run it. Ship now says so.
- **Class D.** A held build lock, a bisect that stops at the oldest red merge of several, and a fix reverted while the
  change it fixes is reverted are guard behaviour. The guard already got a stale-code fix (`278950af5`). Left for the
  lead: when the bisect range holds more than one red build, revert nothing and alert, and bisect a test-only PR
  together with the PR it follows.
- **C: fixture state** (#1899, #2117): a test that passes alone and fails after another. Only `--tier all` shows it.
