// tools/validation/validate-island-zones.mjs
// inputs: always
//
// The island zone: `MainWorld_Layers/Place_*.layer`, the layers `tools\cli town fit` writes onto the real Everon map
// (docs/architecture/everon-world.md 3.4, card #2206). A separate validator from validate-world-zones, which it does not touch.
//
// 1. Branch guard. Only a `ws/town-draft-<place>-<slot>` branch may change an island layer, and only its own
//    `Place_<place>_<slot>.layer`. The branch is GITHUB_HEAD_REF in CI, else the checked-out branch; main and a detached HEAD
//    outside CI are not checked. The change set is the diff against the merge base with origin/main, untracked files included.
// 2. Claims. Every island layer has its claim `tools/town/claims/<place>-<slot>.json` and every claim its layer; the claim agrees with
//    the place's approved mapping; no two claims name one building (or plot), except a street slot marked `shared`.
// 3. Footprints. Each layer's top-level entities stand inside the claimed building's footprint (or plot) plus the slot's margin,
//    read from `docs/everon/places/<place>/dossier.json`.
// A claim or layer whose place has no dossier or mapping fails: the footprint is unknown. With no island layer and no claim the
// check passes at once. The per-place completeness (required slots, kinds, perf budget) is `tools\cli town check <place>`.
// red-proof: tools/validation/island-zones-selftest.mjs runs the fixture places in tools/town/fixtures/check-place/ (a complete one, a
// missing slot, an entity outside its footprint, a double claim, a non-town-draft branch) and asserts each result.

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CLAIMS_DIR, LAYER_DIR, checkClaimsAndFootprints, guardIslandLayers, loadPlace } from "../town/check.mjs";
import { changedFiles } from "./validate-world-zones.mjs";

const git = (root, args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8", windowsHide: true });
  return r.status === 0 ? r.stdout : null;
};

// The places an island has layers or claims for: from the layer names (Place_<place>_<slot>.layer) and the claims' own `place`.
export function islandPlaces(root) {
  const places = new Set();
  const lDir = join(root, LAYER_DIR);
  if (existsSync(lDir)) for (const f of readdirSync(lDir)) { const m = /^Place_([a-z0-9-]+)_[a-z0-9-]+\.layer$/.exec(f); if (m) places.add(m[1]); }
  const cDir = join(root, CLAIMS_DIR);
  const bad = [];
  if (existsSync(cDir)) {
    for (const f of readdirSync(cDir).filter((n) => n.endsWith(".json"))) {
      try {
        const c = JSON.parse(readFileSync(join(cDir, f), "utf8"));
        if (c && c.place) places.add(c.place);
        else bad.push(`${CLAIMS_DIR}/${f}: the claim names no place`);
      } catch (e) {
        bad.push(`${CLAIMS_DIR}/${f}: not valid JSON (${e.message})`);
      }
    }
  }
  return { places: [...places].sort(), bad };
}

// Pure but for the reads of the repo: claims, layers and footprints of every place.
export function checkIslandClaims(root) {
  const { places, bad } = islandPlaces(root);
  const errors = [...bad];
  const warnings = [];
  for (const place of places) {
    let data;
    try {
      data = loadPlace(root, place);
    } catch (e) {
      errors.push(`place ${place}: ${e.message} (its island layers and claims cannot be checked)`);
      continue;
    }
    const r = checkClaimsAndFootprints(data);
    errors.push(...r.errors);
    warnings.push(...r.warnings);
  }
  return { errors, warnings, places: places.length };
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const branch = process.env.GITHUB_HEAD_REF || (git(root, ["rev-parse", "--abbrev-ref", "HEAD"]) || "").trim();
  const files = changedFiles(root);
  const guard = files ? guardIslandLayers({ branch, files }) : { errors: [], touched: [], skipped: "no merge base with origin/main" };
  const claims = checkIslandClaims(root);
  for (const w of claims.warnings) console.log(`validate-island-zones: warning: ${w}`);
  const errors = [...guard.errors, ...claims.errors];
  if (errors.length) {
    console.log("validate-island-zones: FAIL");
    for (const e of errors) console.log(`  ${e}`);
    process.exit(1);
  }
  console.log(`validate-island-zones: OK (${claims.places} place(s) with island layers or claims; guard ${guard.skipped ? `skipped: ${guard.skipped}` : `${branch}: ${guard.touched.length} island layer(s) changed`})`);
}
