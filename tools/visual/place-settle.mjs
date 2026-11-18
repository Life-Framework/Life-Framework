// tools/visual/place-settle.mjs - physics settle for loose dynamic props (#2957).
//
//   node tools/visual/place-settle.mjs settle <entity>... [--world W] [--dry] [--lift M]
//     simulates each named layer entity (a dynamic rigid body) to rest in Workbench play mode and writes
//     the rest pose (coords and angleX/Y/Z) to its layer; `--dry` only prints it.
//   node tools/visual/place-settle.mjs proof
//     the acceptance check: drops a log, a tyre and a saline bag tilted over the slice-2957 grid cell,
//     simulates each to rest, simulates the rest pose again and requires it to stay put (1 cm, 2 deg).
//
// The engine side is tools/visual/wb/LF_PlaceSettlePlugin.c (its header documents plan.txt and status.txt).
// Static props settle by trace snap instead (`tools\cli place settle`, #2954).
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PLUGIN = join(ROOT, "tools", "visual", "wb", "LF_PlaceSettlePlugin.c");
const GRID = "Worlds/GridWorld/GridWorld.ent";
// The proof's bodies: dropped 1 m over the ground of the slice-2957 cell (x 262..274, z 46..58), tilted.
const PROOF_DROPS = [
  // Clear of the cell's sign and posts on its west side (a log dropped at x 264 came to rest on the sign at 2.85 m).
  // The tyre (Tire_M151A2) is a static prop: the plugin refuses it (`not_dynamic`), trace snap is its tool.
  ["log", 265.5, 1, 50, 30, 20, 25, "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et"],
  ["log_steep", 265.5, 1.2, 53, 70, 110, 10, "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et"],
  ["bag", 265.5, 1, 56, 20, 45, 70, "Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et"],
];

export function parseStatus(text) {
  const rows = { rest: {}, stable: {}, wrote: {}, errors: [], done: null };
  for (const line of String(text || "").split(/\r?\n/)) {
    const m = /^\d+ (\w+) (.*)$/.exec(line.trim());
    if (!m) continue;
    const [, kind, rest] = m;
    if (kind === "error") rows.errors.push(rest);
    else if (kind === "done") rows.done = rest.split(" ").map(Number);
    else if (kind === "rest" || kind === "stable" || kind === "wrote") {
      const [name, ...kv] = rest.split(" ");
      rows[kind][name] = Object.fromEntries(kv.filter((s) => s.includes("=")).map((s) => s.split("=")));
      rows[kind][name].line = rest;
    }
  }
  return rows;
}

export function planFor({ world = GRID, park = "120 -100", drops = [], settles = [], dry = false, lift }) {
  const lines = [`world ${world}`, `park ${park}`, "rest 0.02 0.05 800 15000"];
  if (lift !== undefined) lines.push(`lift ${lift}`);
  if (dry) lines.push("dry");
  for (const d of drops) lines.push(`drop ${d.join(" ")}`);
  for (const s of settles) lines.push(`settle ${s}`);
  return lines.join("\n") + "\n";
}

async function runPlugin(plan, budgetS) {
  const look = await import("./look.mjs");
  const exe = look.workbenchExe();
  if (!exe) throw new Error("Workbench not found (set ENFUSION_WORKBENCH_PATH)");
  const profile = join(ROOT, "server", "profile", "settle");
  const dir = join(profile, "profile", "lf-settle"); // $profile is <profile dir>/profile
  mkdirSync(dir, { recursive: true });
  for (const f of readdirSync(dir)) rmSync(join(dir, f), { force: true });
  writeFileSync(join(dir, "plan.txt"), plan);
  const injected = join(ROOT, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "LFVisual");
  const wbRoute = await import("../lib/wb-route.mjs");
  wbRoute.requireLocalWorkbench(ROOT, "place settle physics"); // never on a PC without the workbench cap
  const slot = await wbRoute.acquireWorkbench(ROOT, { purpose: "place settle physics", waitMs: 1800000 });
  try {
    mkdirSync(injected, { recursive: true });
    cpSync(PLUGIN, join(injected, "LF_PlaceSettlePlugin.c"));
    const run = await look.runWorkbench(exe, profile, budgetS, false, { plugin: "LF_PlaceSettlePlugin" });
    const status = existsSync(join(dir, "status.txt")) ? readFileSync(join(dir, "status.txt"), "utf8") : "";
    // Compile failures only: compileErrors also matches the game's own LFDebug error lines in play mode.
    const compile = look.compileErrors(look.latestLog(profile)).filter((l) => /Can't compile|SCRIPT\s+\(E\): @"/.test(l));
    return { run, status, compile };
  } finally {
    rmSync(join(injected, "LF_PlaceSettlePlugin.c"), { force: true });
    try {
      if (existsSync(injected) && readdirSync(injected).length === 0) rmSync(injected, { recursive: false });
    } catch {}
    slot.release();
  }
}

function report(rows, { run, compile }) {
  for (const line of compile) console.log(`  ${line}`);
  for (const [n, r] of Object.entries(rows.rest)) console.log(`  rest   ${r.line}`);
  for (const [n, r] of Object.entries(rows.stable)) console.log(`  stable ${r.line}`);
  for (const [n, r] of Object.entries(rows.wrote)) console.log(`  wrote  ${r.line}`);
  for (const e of rows.errors) console.log(`  error  ${e}`);
  if (run.timedOut) console.log("  Workbench ran out of its budget");
}

export async function main(argv) {
  const [verb, ...rest] = argv;
  const flags = {};
  const pos = [];
  for (let i = 0; i < rest.length; i++) {
    if (rest[i].startsWith("--")) flags[rest[i].slice(2)] = rest[i + 1] && !rest[i + 1].startsWith("--") ? rest[++i] : true;
    else pos.push(rest[i]);
  }
  if (verb === "proof") {
    const { run, status, compile } = await runPlugin(planFor({ drops: PROOF_DROPS }), 300);
    const rows = parseStatus(status);
    report(rows, { run, compile });
    const fails = [];
    for (const [name] of PROOF_DROPS) {
      const r = rows.rest[name];
      const s = rows.stable[name];
      if (!r) fails.push(`${name}: never came to rest`);
      else if (Number(r.moved) < 0.05) fails.push(`${name}: moved ${r.moved} m from the drop (physics did not run)`);
      if (!s) fails.push(`${name}: rest pose not simulated again`);
      else if (Number(s.drift) > 0.01 || Number(s.turn) > 2) fails.push(`${name}: rest pose not stable (drift ${s.drift} m, turn ${s.turn} deg)`);
    }
    if (compile.length) fails.push("compile errors");
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(`SUMMARY physics-settle ${fails.length ? "fail" : "ok"} bodies=${PROOF_DROPS.length} rested=${Object.keys(rows.rest).length} stable=${Object.keys(rows.stable).length}`);
    return fails.length ? 1 : 0;
  }
  if (verb === "settle" && pos.length) {
    const { run, status, compile } = await runPlugin(planFor({ world: flags.world || GRID, settles: pos, dry: flags.dry === true, lift: flags.lift }), 240 + 20 * pos.length);
    const rows = parseStatus(status);
    report(rows, { run, compile });
    const ok = !rows.errors.length && !compile.length && rows.done && rows.done[1] === 0;
    console.log(`SUMMARY place-settle-physics ${ok ? "ok" : "fail"} rested=${Object.keys(rows.rest).length}/${pos.length} wrote=${Object.keys(rows.wrote).length}${flags.dry ? " (dry)" : ""}`);
    return ok ? 0 : 1;
  }
  console.log("usage: node tools/visual/place-settle.mjs settle <entity>... [--world W] [--dry] [--lift M]\n       node tools/visual/place-settle.mjs proof");
  return 2;
}

if (import.meta.url === pathToFileURL(process.argv[1] || "").href) process.exitCode = await main(process.argv.slice(2));
