// Proves `wt ship` regenerates docs/config/ for a branch that touches the config surface (#3717): the decision
// (tools/lib/config-schema-regen.mjs), its place in the ship flow before validate (tools/lib/ship-direct.mjs), and the
// cli wiring. Fakes only: no git, no GitHub.
// inputs: tools/lib/config-schema-regen.mjs, tools/lib/ship-direct.mjs, tools/cli.mjs, tools/feature/new.mjs
// red-proof 2026-11-19: with the regen() call removed from ship-direct.mjs this printed "FAIL  ship: the schema is
// regenerated before validate"; with touchesConfigSurface returning false it printed "FAIL  a config class change
// triggers the regeneration".

import { readFileSync } from "node:fs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

const regen = await import("../lib/config-schema-regen.mjs");
const direct = await import("../lib/ship-direct.mjs");

// 1. What counts as the config surface.
const t = regen.touchesConfigSurface;
check(t(["addons/LifeFramework/Scripts/Game/Feature/Market/LF_MarketConfig.c"]), "a config class change triggers the regeneration");
check(t(["addons/LF_Everon1989/Configs/LF_Pack.conf"]), "a pack conf under Configs/ triggers it");
check(t(["tools/config/schema-overrides.json"]), "the overrides file triggers it");
check(t(["addons\\LifeFramework\\Scripts\\Game\\X.c"]), "backslash paths are understood");
check(!t(["docs/features/market.md", "website/src/App.tsx", "addons/LifeFramework/Prefabs/X.et"]), "docs, website and prefabs do not");
check(!t([]) && !t(undefined), "an empty change list does not");

// 2. regenConfigSchema: quiet when untouched, commits only when the output changed, never throws on a failed write.
function deps(over = {}) {
  const calls = [];
  return {
    calls,
    log: () => {},
    changed: () => (calls.push("changed"), ["addons/LifeFramework/Scripts/Game/Core/Config/LF_X.c"]),
    write: () => (calls.push("write"), { status: 0 }),
    dirty: () => (calls.push("dirty"), ["M docs/config/config.schema.json"]),
    commit: () => (calls.push("commit"), { status: 0 }),
    ...over,
  };
}
{
  const d = deps({ changed: () => (d.calls.push("changed"), ["docs/features/x.md"]) });
  const r = regen.regenConfigSchema(d);
  check(!r.ran && d.calls.join(",") === "changed", "an unrelated branch runs nothing");
}
{
  const d = deps();
  const r = regen.regenConfigSchema(d);
  check(r.ran && r.committed && d.calls.join(",") === "changed,write,dirty,commit", `a stale schema is written then committed (${d.calls.join(",")})`);
}
{
  const d = deps({ dirty: () => [] });
  const r = regen.regenConfigSchema(d);
  check(r.ran && !r.committed && !d.calls.includes("commit"), "an up-to-date schema makes no commit");
}
{
  const logs = [];
  const d = deps({ write: () => ({ status: 1, stderr: "boom\nmore" }), log: (l) => logs.push(l) });
  const r = regen.regenConfigSchema(d);
  check(r.ran && !r.ok && !d.calls.includes("commit") && /validate will report it/.test(logs.join(" ")), "a failed generation is reported, not committed, not fatal");
}

// 3. In the ship flow: before validate in the preflight, after the behind-merge, and a throwing regen never blocks.
{
  const calls = [];
  const d = {
    log: () => {},
    mergeTree: () => ({ status: 0, stdout: "tree\n" }),
    mergeBase: () => calls.push("mergeBase"),
    validate: () => (calls.push("validate"), 0),
    push: () => (calls.push("push"), { status: 0 }),
    findPr: () => null,
    createPr: () => ({ number: 7, url: "https://x/pull/7" }),
    autoMerge: () => ({ status: 0 }),
    regenSchema: () => calls.push("regen"),
  };
  const r = await direct.shipDirect(d, { slug: "x" });
  check(r.code === 0 && calls.join(",") === "regen,validate,push", `ship: the schema is regenerated before validate (${calls.join(",")})`);
}
{
  const calls = [];
  const d = {
    log: () => {},
    mergeTree: () => ({ status: 0, stdout: "tree\n" }),
    mergeBase: () => calls.push("mergeBase"),
    validate: () => (calls.push("validate"), 0),
    push: () => (calls.push("push"), { status: 0 }),
    findPr: () => null,
    createPr: () => ({ number: 7, url: "https://x/pull/7" }),
    autoMerge: () => (calls.push("autoMerge"), calls.filter((c) => c === "autoMerge").length === 1 ? { status: 1, stderr: "Base branch was modified" } : { status: 0 }),
    regenSchema: () => { calls.push("regen"); throw new Error("kaboom"); },
  };
  const r = await direct.shipDirect(d, { slug: "x" });
  check(r.code === 0, "ship: a throwing regeneration does not stop the ship");
  check(calls.join(",") === "regen,validate,push,autoMerge,mergeBase,regen,push,autoMerge", `ship: regenerated again after main is merged in (${calls.join(",")})`);
}
{
  const calls = [];
  const d = {
    log: () => {},
    mergeTree: () => ({ status: 0 }),
    validate: () => (calls.push("validate"), 0),
    push: () => (calls.push("push"), { status: 0 }),
    findPr: () => null,
    createPr: () => ({ number: 7, url: "u" }),
    autoMerge: () => ({ status: 0 }),
    regenSchema: () => calls.push("regen"),
  };
  await direct.shipDirect(d, { slug: "x", preflight: false });
  check(calls.join(",") === "regen,push", `ship --no-preflight: still regenerates before the push (${calls.join(",")})`);
}

// 4. Wiring.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const body = cli.slice(cli.indexOf("async function shipDirectCmd"), cli.indexOf("async function wtShipCmd"));
  check(/regenSchema:/.test(body) && body.includes("regenConfigSchema") && body.includes("schema.mjs"), "cli: wt ship passes regenSchema to shipDirect");
  const feat = readFileSync(new URL("../feature/new.mjs", import.meta.url), "utf8");
  check(/schema\.mjs --write/.test(feat), "feature new: the checklist names the schema regeneration");
}

if (failed) process.exit(1);
console.log("PASS  config-schema-regen selftest");
