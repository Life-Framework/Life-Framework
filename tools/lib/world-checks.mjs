// Which validate checks judge a maintainer world edit, and which of them only warn.
//
// A hand edit of the town (`tools\cli world ship`, the T1w gate, any validate on a
// town-maintainer-* branch) is judged on two kinds of check:
//   HARD    the layer is not broken: a parse error (world ship's diff), a prefab GUID that is
//           missing or unknown, a duplicate GUID, an edit outside the town layers. These block.
//   LAYOUT  where things stand: the town manifest's sign distances, the village-name and fixture
//           checks, the player-props and road-id checks. The maintainer placed it by hand and looked
//           at it in Workbench, so a failure here is a warning listed in the PR body, not a blocker.
// A failed test is hard too (it is its own step, not a validate check).
//
// Every name below reads .layer files. `validate --world` runs only these (not all ~270 checks), so
// world ship reaches the gate in well under a minute. tools/validation/world-checks-selftest.mjs
// asserts each named check exists and the two lists do not overlap.

export const HARD_CHECKS = [
  "validate-repo.ps1", // duplicate resource GUIDs, orphan .meta files
  "validate-guid-paths.mjs", // a {GUID}path that does not resolve
  "validate-world-zones.mjs", // an edit outside the town layers (needs a town- branch)
  "validate-island-zones.mjs", // an island Place_* layer changed off a town-draft branch, or outside its claim or footprint (#2206)
  "validate-debugworld.ps1", // required layers exist and parse
  "validate-debugworld-layout.ps1",
  "validate-debugworld-layer-order.ps1",
];

export const LAYOUT_CHECKS = [
  "validate-town-manifest.mjs",
  "validate-annex-fixtures.mjs",
  "validate-core-village-names.mjs",
  "validate-core-place-names.mjs",
  "validate-player-props.mjs",
  "validate-road-ids.mjs",
  "validate-prop-on-surface.mjs",
];

// The LAYOUT checks that read only town layers. The town is hand-edited and must never block anyone
// (maintainer rule, 2026-11-15), so these warn on EVERY branch, not just a town-maintainer-* one. A normal PR
// cannot change a town layer (validate-world-zones stays HARD), so a finding here is never the PR's doing.
// validate-prop-on-surface and the placement audit judge per entity instead (tools/lib/town-findings.mjs):
// town entities warn, grid and prefab findings still block. The rest of LAYOUT_CHECKS read tests, core scripts
// or the annex, so they stay hard off a maintainer branch.
export const TOWN_LAYOUT_CHECKS = [
  "validate-town-manifest.mjs",
  "validate-player-props.mjs",
  "validate-road-ids.mjs",
];

// Checks that read .layer files and stay hard on a maintainer edit, each with the reason. Audited 2026-11-15: none
// of these judges where a thing stands; a failure is a broken or unsafe layer, or says nothing about the world.
export const LAYER_READERS_KEPT_HARD = {
  "validate-addon-boundary.mjs": "core layers never name the pack: a build-breaking reference, not a placement",
  "validate-en-text.mjs": "non-English literal text in a layer: a content bug, not a position",
  "validate-npc-layer-overrides.mjs": "a layer override on a replicated NPC kicks clients on join (RplCreationError): never a layout call",
  "validate-grid-world.mjs": "GridWorld only (the debug grid): a legacy station outside every claimed cell; the town layers are never read",
  "validate-sound-slots.mjs":"a sound slot id a layer asks for must exist: a broken reference",
  "validate-no-town-layout-asserts.mjs": "judges new test files, and already only warns when the town moved under an old literal",
};

export const MAINTAINER_BRANCH = /^(ws\/)?town-maintainer-[\w-]+$/;

// The scripts a world validate runs, in directory order, from what is on disk.
export function worldScripts(all) {
  const want = new Set([...HARD_CHECKS, ...LAYOUT_CHECKS]);
  return all.filter((s) => want.has(s));
}

// { failed, warned } from [{ s, status }]: a failing LAYOUT check warns only while `demote` is on.
export function judgeChecks(results, { demote = false } = {}) {
  const layout = new Set(LAYOUT_CHECKS);
  const town = new Set(TOWN_LAYOUT_CHECKS);
  const failed = [];
  const warned = [];
  for (const r of results) {
    if (r.status === 0) continue;
    if (town.has(r.s) || (demote && layout.has(r.s))) warned.push(r.s);
    else failed.push(r.s);
  }
  return { failed, warned };
}

// The text of each demoted check from a validate transcript: the lines between its
// `WARN  validate/<name>` header and the next header or summary line.
export function layoutWarnings(text) {
  const out = [];
  let cur = null;
  for (const line of String(text).split(/\r?\n/)) {
    const h = /^(PASS|FAIL|WARN|SKIP)\s+validate\/(\S+)/.exec(line);
    if (h) {
      cur = h[1] === "WARN" ? { check: h[2], lines: [] } : null;
      if (cur) out.push(cur);
      continue;
    }
    if (/^(slowest:|incremental:|validate: \d+ check)/.test(line)) {
      cur = null;
      continue;
    }
    if (cur && line.trim()) cur.lines.push(line.trim());
  }
  return out.map((w) => ({ check: w.check, text: w.lines.filter((l) => !/^SUMMARY /.test(l)).slice(0, 6).join(" | ") }));
}
