// tools/validation/validate-player-props.mjs
// inputs: addons/**, docs/**
//
// No new debug-looking thing in a place players walk through. A test fixture or a debug
// tool placed in the village (a `*_Debug` entity, a prefab from a `/Debug/` folder) is what a
// player then meets in the street: a blank white sign that jails you, a crate named
// CrimeTrader_Debug standing in for a trader. The annex layers are where fixtures live
// (docs/architecture/core-vs-pack.md); the village layers hold only what a player should see.
//
// Fails when a village layer declares such an entity that is not in
// tools/validation/player-props-baseline.json. The baseline is today's debt
// (docs/architecture/agent-quality.md "What remains"); it never grows.
//   node tools/validation/validate-player-props.mjs --write-baseline
// rewrites it from the tree and refuses to add a name.
//
// What to do instead: put the fixture in an annex layer, or give the thing a real name
// and a prop a player would recognise (docs/style-guide.md).
// red-proof: tools/validation/agent-quality-selftest.mjs plants a village `Till_Debug` and a
// `/Debug/` prefab and asserts both are reported while an annex one is not.

import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ANNEX_LAYERS, LAYERS } from "./validate-annex-fixtures.mjs";

export const BASELINE = "tools/validation/player-props-baseline.json";

// Top-level entity declarations of a layer that look like debug things: [{ name, why }].
export function debugProps(text) {
  const out = [];
  for (const m of String(text).matchAll(/^[ 	]*[A-Za-z_][A-Za-z0-9_]* ([A-Za-z_][A-Za-z0-9_]*) : "\{[0-9A-F]{16}\}([^"]+)"/gm)) {
    const [, name, prefab] = m;
    if (/debug/i.test(name)) out.push({ name, why: "named *Debug*" });
    else if (/\/Debug\//i.test(`/${prefab}`)) out.push({ name, why: `prefab from a Debug folder (${prefab})` });
  }
  return out;
}

export function scan(root) {
  const dir = join(root, LAYERS);
  const found = [];
  if (!existsSync(dir)) return found;
  for (const f of readdirSync(dir).filter((n) => n.endsWith(".layer")).sort()) {
    const layer = f.replace(/\.layer$/, "");
    if (ANNEX_LAYERS.has(layer)) continue;
    for (const hit of debugProps(readFileSync(join(dir, f), "utf8"))) found.push({ layer, ...hit });
  }
  return found;
}

export function check(found, baseline) {
  const known = new Set(baseline);
  const present = new Set(found.map((f) => `${f.layer}/${f.name}`));
  return {
    fresh: found.filter((f) => !known.has(`${f.layer}/${f.name}`)),
    stale: baseline.filter((b) => !present.has(b)),
  };
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const file = join(root, BASELINE);
  const baseline = existsSync(file) ? JSON.parse(readFileSync(file, "utf8")).entities : [];
  const found = scan(root);
  const { fresh, stale } = check(found, baseline);
  if (process.argv.includes("--write-baseline")) {
    if (fresh.length && existsSync(file)) {
      console.log(`validate-player-props: refusing to add ${fresh.length} name(s) to the baseline: ${fresh.map((f) => `${f.layer}/${f.name}`).join(", ")}`);
      process.exit(1);
    }
    const entities = found.map((f) => `${f.layer}/${f.name}`).sort();
    writeFileSync(file, `${JSON.stringify({ note: "Debug-looking entities in village layers on the day the check landed. Burn down, never add (validate-player-props.mjs).", entities }, null, 2)}\n`);
    console.log(`validate-player-props: baseline written (${entities.length})`);
    process.exit(0);
  }
  if (fresh.length) {
    console.log("validate-player-props: FAIL");
    for (const f of fresh) console.log(`  ${f.layer}.layer: ${f.name} is ${f.why}. Players walk through this layer: move the fixture to an annex layer, or give it a real name and prop (docs/style-guide.md)`);
    process.exit(1);
  }
  console.log(`validate-player-props: OK (${baseline.length - stale.length} baselined debug-looking village entities${stale.length ? `, ${stale.length} stale baseline row(s): run --write-baseline` : ""})`);
}
