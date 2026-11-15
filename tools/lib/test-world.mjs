// Which world a test run boots (tools\cli test).
//
// A run whose every selected test is LOGIC boots the minimal world Worlds/TestLogic/TestLogic.ent:
// a world entity, the time manager, the debug game mode, the destruction manager and a test runner
// flagged m_bLogicWorld. No terrain, village, annex, zones or spawn points, so the engine skips
// about 2.4 s of entity load and preload on lf-cloud (1.5 s on the laptop) that a LOGIC test never
// reads. Everything else boots DebugWorld as before.
//
//   --tier fast (no --only)        LOGIC only by definition (LF_TestManager FAST mode)  -> TestLogic
//   --only / --red, all LOGIC      every name resolves to a LOGIC test                   -> TestLogic
//   --only / --red, any other name WORLD, PERSISTENCE or unknown                         -> DebugWorld
//   --tier all | persistence       -> DebugWorld
//   --world <path>                 always wins; LF_TEST_LOGIC_WORLD=0 turns the routing off
//
// In TestLogic the runner refuses a WORLD or PERSISTENCE test by name and adds a "tier it WORLD"
// hint under a failing LOGIC test (LF_TestManager.LogicWorldRefusal / LogicWorldHint).
// The chosen world goes into the test pass cache key (tools/lib/test-cache.mjs keys on --world).

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const LOGIC_WORLD = "Worlds/TestLogic/TestLogic.ent";
export const LOGIC_WORLD_LAYER = "addons/LifeFramework/Worlds/TestLogic/TestLogic_Layers/Tests.layer";

// { "<LF_Test_Class>": "LOGIC", "<GetName() string>": "LOGIC", ... } from every test file's
// `// tier:` line above each `class LF_Test_X : LF_Test`, the same source regen-tests reads.
export function parseTiers(text) {
  const tiers = {};
  let pending = null;
  let cls = null;
  let inGetName = false;
  for (const line of text.split(/\r?\n/)) {
    const t = /^\s*\/\/\s*tier:\s*(LOGIC|WORLD|PERSISTENCE)\b/.exec(line);
    if (t) {
      pending = t[1];
      continue;
    }
    const c = /^\s*class\s+(\w+)\s*:\s*LF_Test\b/.exec(line);
    if (c) {
      cls = c[1];
      if (pending) tiers[cls] = pending;
      pending = null;
      inGetName = false;
      continue;
    }
    if (!cls || !tiers[cls]) continue;
    if (/\bstring\s+GetName\s*\(\s*\)/.test(line)) {
      inGetName = true;
      continue;
    }
    if (inGetName) {
      const r = /return\s+"([^"]+)"\s*;/.exec(line);
      if (r) {
        tiers[r[1]] = tiers[cls];
        inGetName = false;
      } else if (/^\s*}\s*$/.test(line)) inGetName = false;
    }
  }
  return tiers;
}

export function testTiers(root) {
  const dir = join(root, "addons", "LifeFramework", "Scripts", "Game", "Tests");
  const tiers = {};
  if (!existsSync(dir)) return tiers;
  for (const f of readdirSync(dir)) {
    if (f.endsWith(".c")) Object.assign(tiers, parseTiers(readFileSync(join(dir, f), "utf8")));
  }
  return tiers;
}

// { world, reason }: world is undefined for DebugWorld (the harness default), so a run that is not
// routed keeps the exact cache key and arguments it had before the routing existed.
export function pickWorld({ tier, only = [], world, env = process.env, tiers }) {
  if (world) return { world, reason: "--world" };
  if (env.LF_TEST_LOGIC_WORLD === "0") return { world: undefined, reason: "LF_TEST_LOGIC_WORLD=0" };
  if (only.length) {
    const notLogic = only.filter((n) => tiers[n] !== "LOGIC");
    if (notLogic.length) return { world: undefined, reason: `not LOGIC: ${notLogic.slice(0, 3).join(",")}${notLogic.length > 3 ? ",..." : ""}` };
    return { world: LOGIC_WORLD, reason: "every selected test is LOGIC" };
  }
  if (tier === "fast") return { world: LOGIC_WORLD, reason: "the fast tier is LOGIC only" };
  return { world: undefined, reason: `tier ${tier}` };
}
