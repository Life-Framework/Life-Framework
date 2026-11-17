// tools\cli npc dress: the World Editor's LF NPC Dresser, head-less (docs/tooling/npc-dresser.md).
//
//   tools\cli npc dress --entity <name> [--entity <name>...] [--preset <conf>] [--slot <key>=<prefab>...]
//                       [--world <path>] [--save] [--proof] [--look x,z] [--remote]
//
// Writes $profile:lf-npc-dresser/plan.txt and starts a hidden Workbench with -plugin=LF_NpcDresserPlugin, which
// runs the same Apply the dialog runs (one undoable edit per dress). --proof adds the round trip: read the slots
// back, Edit > Undo, read the originals back, Edit > Redo, read again. --save saves the world's layers.
// --look x,z takes `look` pictures of that spot before and after (implies --save).
// On a PC without the workbench cap (this one) the whole run goes to a pool machine with it (tools/lib/wb-route.mjs);
// its layers are that machine's synced copy, so nothing in this checkout changes. The status, the layer diff and the
// pictures come back to tmp/npc-dress/<id>/.
import { cpSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join, resolve } from "node:path";

export const help = "npc dress --entity <name> [--preset <conf>] [--slot head=<prefab>] [--world w] [--save] [--proof] [--look x,z]   dress NPCs through the World Editor plugin, head-less (docs/tooling/npc-dresser.md)";

const DEBUG_WORLD = "Worlds/DebugWorld/DebugWorld.ent";
const SLOTS = ["head", "torso", "legs", "feet", "vest", "backpack"];

export function parse(args) {
  const o = { entities: [], slots: [], world: DEBUG_WORLD, save: false, proof: false, look: null, id: null, preset: null };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    const v = () => args[++i];
    if (a === "--entity") o.entities.push(v());
    else if (a === "--preset") o.preset = v();
    else if (a === "--slot") {
      const [k, p] = String(v()).split("=");
      if (!SLOTS.includes(k) || !p) throw new Error(`--slot wants <${SLOTS.join("|")}>=<prefab>`);
      o.slots.push([k, p]);
    } else if (a === "--world") o.world = v();
    else if (a === "--save") o.save = true;
    else if (a === "--proof") o.proof = true;
    else if (a === "--look") o.look = v();
    else if (a === "--id") o.id = v();
    else if (a === "--remote") o.remote = true;
    else throw new Error(`unknown argument ${a}`);
  }
  if (!o.entities.length) throw new Error("name at least one --entity");
  if (!o.preset && !o.slots.length) throw new Error("give --preset or --slot");
  if (o.look) o.save = true;
  return o;
}

// The plugin's plan: what to dress, and with --proof the read-back, undo and redo around it.
export function planText(o, expected) {
  const lines = [`world ${o.world}`, ...o.entities.map((e) => `select ${e}`)];
  if (o.preset) lines.push(`preset ${o.preset}`);
  for (const [k, p] of o.slots) lines.push(`slot ${k} ${p}`);
  if (o.proof) for (const e of o.entities) lines.push(`snapshot ${e}`);
  const heads = () => { for (const e of o.entities) lines.push(`expect-heads ${e} 1`); };
  if (o.proof) heads(); // the NPC starts with its one head
  lines.push("dress");
  if (o.proof) {
    for (const e of o.entities) for (const [k, p] of expected) lines.push(`expect ${e} ${k} ${p}`);
    heads(); // a dress must not grow another head (the editor re-initialises the entity on a write)
    lines.push("undo");
    for (const e of o.entities) lines.push(`expect-snapshot ${e}`);
    heads();
    lines.push("redo");
    for (const e of o.entities) for (const [k, p] of expected) lines.push(`expect ${e} ${k} ${p}`);
    heads();
  }
  if (o.save) lines.push("save");
  return lines.join("\n") + "\n";
}

// The slots a run should end with: the preset's, then the --slot picks over them (path part only).
export function expectedSlots(root, o) {
  const want = new Map();
  if (o.preset) {
    const rel = o.preset.replace(/^\{[0-9A-F]{16}\}/, "");
    const file = ["LifeFramework", "LF_Everon1989"].map((a) => join(root, "addons", a, rel)).find(existsSync);
    if (!file) throw new Error(`preset not found in the core or the pack: ${rel}`);
    const field = { m_Head: "head", m_Torso: "torso", m_Legs: "legs", m_Feet: "feet", m_Vest: "vest", m_Backpack: "backpack" };
    for (const m of readFileSync(file, "utf8").matchAll(/^\s*(m_\w+)\s+"(?:\{[0-9A-F]{16}\})?([^"]+)"/gm)) if (field[m[1]]) want.set(field[m[1]], m[2]);
  }
  for (const [k, p] of o.slots) want.set(k, p.replace(/^\{[0-9A-F]{16}\}/, ""));
  return [...want];
}

async function runHere(root, o, outDir) {
  const look = await import("../visual/look.mjs");
  const exe = look.workbenchExe();
  if (!exe) {
    console.log("FAIL  npc dress: Workbench not found (ENFUSION_WORKBENCH_PATH)");
    return 2;
  }
  mkdirSync(outDir, { recursive: true });
  const cli = (args) => spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), ...args], { cwd: root, stdio: "inherit", windowsHide: true }).status;
  if (o.look) cli(["look", o.look, "--fresh", "--radius", "4", "--close", "4", "--wait", "--out", join(outDir, "before")]);
  const profile = join(root, "tmp", "npc-dress-profile");
  const dir = join(profile, "profile", "lf-npc-dresser"); // $profile: is <-profile>/profile/
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "plan.txt"), planText(o, expectedSlots(root, o)));
  writeFileSync(join(dir, "status.txt"), "");
  console.log(`npc dress: hidden Workbench, plugin LF_NpcDresserPlugin, ${o.entities.length} NPC(s)`);
  const r = await look.runWorkbench(exe, profile, 240, false, { plugin: "LF_NpcDresserPlugin" });
  const status = readFileSync(join(dir, "status.txt"), "utf8");
  writeFileSync(join(outDir, "status.txt"), status);
  const logFile = look.latestLog(profile);
  if (logFile) cpSync(logFile, join(outDir, "workbench.log"));
  const git = (a) => spawnSync("git", a, { cwd: root, encoding: "utf8" }).stdout || "";
  const diff = `${git(["status", "--porcelain", "--", "addons"])}\n${git(["diff", "--", "addons"])}`;
  writeFileSync(join(outDir, "layer.diff"), diff);
  if (o.look) cli(["look", o.look, "--fresh", "--radius", "4", "--close", "4", "--wait", "--out", join(outDir, "after")]);
  const ok = /^done 0$/m.test(status) && !r.timedOut;
  for (const l of status.split("\n")) if (/^error/.test(l)) console.log(`  ${l}`);
  if (logFile) for (const l of look.compileErrors(logFile)) console.log(`  ${l}`);
  console.log(`SUMMARY npc-dress ${ok ? "ok" : "fail"} steps=${status.split("\n").filter(Boolean).length} out=${outDir}`);
  return ok ? 0 : 1;
}

export async function main(args, { root }) {
  if (args[0] !== "dress") {
    console.log(`usage: tools\\cli ${help}`);
    return 2;
  }
  let o;
  try {
    o = parse(args.slice(1));
  } catch (e) {
    console.log(`npc dress: ${e.message}`);
    return 2;
  }
  const wt = await import("../wt.mjs");
  if (wt.isMainCheckout(root)) {
    console.log("REFUSE  'npc dress' edits a world: run it in a worktree (tools\\cli wt new <slug>), never the world-editor checkout.");
    return 2;
  }
  o.id = o.id || `${Date.now().toString(36)}`;
  const rel = `tmp/npc-dress/${o.id}`;
  const outDir = resolve(root, rel);
  const route = await import("../lib/wb-route.mjs");
  const fwd = ["npc", ...args.filter((a) => a !== "--remote" && a !== "--id" && a !== o.id), "--id", o.id];
  let r;
  if (o.remote) {
    // A pool machine even when this one may run Workbench: the proof belongs on a box nobody edits on.
    const p = await route.pickWorkbenchMachine(root, { purpose: "npc dress" });
    if (p.error || !p.machine) {
      console.log(`FAIL  ${p.error ?? "npc dress: no pool machine with a Workbench"}`);
      return 3;
    }
    console.log(`npc dress: running on ${p.name}: ${p.why}`);
    const pool = await import("../remote/pool.mjs");
    const out = await pool.syncAndRun(root, p.machine, { script: "tools\\cli.mjs", args: fwd, suffix: "-npcdress", purpose: "npc dress" });
    r = { routed: true, code: out.code, machine: p.machine, rDir: out.rDir };
  } else {
    r = await route.routeWorkbench(root, { purpose: "npc dress", suffix: "-npcdress", args: fwd, runLocal: () => runHere(root, o, outDir) });
  }
  if (!r.routed) return r.code;
  if (r.rDir) {
    const tb = await import("../remote/testbox.mjs");
    mkdirSync(join(root, "tmp", "npc-dress"), { recursive: true });
    const down = tb.scp(["-r", `${r.machine.host}:${r.rDir.replace(/\\/g, "/")}/${rel}`, join(root, "tmp", "npc-dress")]);
    console.log(down.status === 0 ? `npc dress: results in ${rel}/` : `npc dress: copy back failed (${(down.stderr || "").trim().split("\n")[0]})`);
  }
  return r.code;
}
