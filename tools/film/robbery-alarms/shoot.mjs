// Robbery alarms video (#740): the shoot driver. Runs on a 2-client `--profile full --sound` session (README.md).
//   node tools/film/robbery-alarms/shoot.mjs setup                 register both clients, dress the robber
//   node tools/film/robbery-alarms/shoot.mjs take <ID[,ID..]> [--as=_2]   record takes -> <takes>/<ID>.mp4 (+ game audio)
//   node tools/film/robbery-alarms/shoot.mjs still <name> <x,y,z> <x,y,z> [fov]  one framed still (client 2)
//   node tools/film/robbery-alarms/shoot.mjs cmd <n> <verb> [args..]           one raw harness command
// Client 1 is the robber. Client 2 is the camera: its body waits out of frame at PARK, the shot is a scripted
// camera move with the HUD off, and the take's audio is client 2's own process (it hears from the camera).
import { copyFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { COSTUME, STRIP } from "./costume.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const imp = (p) => import(pathToFileURL(join(ROOT, p)).href);
const H = await imp("agent-box/harness.mjs");
const S = await imp("agent-box/session.mjs");
const A = await imp("tools/film/audio.mjs");
const E = await imp("tools/film/engine.mjs");

export const WORK = process.env.RV_WORK || join(ROOT, "tmp", "robbery-video");
const TAKES = join(WORK, "takes");
const STILLS = join(WORK, "stills");
mkdirSync(TAKES, { recursive: true });
mkdirSync(STILLS, { recursive: true });
const LOG = join(WORK, "shoot.log");
const dir = S.sessionDir();
const log = (s) => {
  console.log(s);
  writeFileSync(LOG, `${new Date().toISOString().slice(11, 19)} ${s}\n`, { flag: "a" });
};
const box = (n) => S.clientBox(dir, n);
export function on(n, spec, t = 10000) {
  H.setBoxes([box(n)]);
  const r = H.sendCommand(spec, t);
  log(`  c${n} ${r.line.replace(/^\d+ (s:\S+ )?/, "")} -> ${r.status} ${String(r.detail).slice(0, 220)}`);
  return r;
}
export const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const v = (a) => a.map((x) => (+x).toFixed(2)).join(" ");
const stand = (n, e, side) => {
  const r = on(n, side ? { verb: "stand", entityName: e, side } : { verb: "stand", entityName: e });
  wait(1800);
  return r;
};
const use = (n, cls, e) => on(n, { verb: "use", className: cls, entityName: e });
const dlg = (n, arg, value) => on(n, { verb: "dialogue", arg, value }, 10000).detail;

// Golden hour on this world's date: 17.75 is low warm sun; 18.5 is already night (stills in README.md).
export const HOUR = process.env.RV_HOUR || "17.75";
function scene(n, name) {
  on(n, { verb: "scene", args: "stop" });
  const r = on(n, { verb: "scene", args: `start ${name} ${HOUR} Clear` }, 12000);
  wait(2500);
  return r;
}
// A camera move: drone from pose A to pose B over the take ([position, look-at] each), or a static pose.
function camArgs(c, seconds) {
  const fov = c.fov ?? 50;
  if (c.static) return `static ${v(c.static[0])} ${v(c.static[1])} ${seconds} ${fov}`;
  return `drone ${v(c.from[0])} ${v(c.from[1])} ${v(c.to[0])} ${v(c.to[1])} ${seconds} ${fov}`;
}
export function still(name, p, l, fov = 50) {
  on(2, { verb: "camera", args: "hud off" });
  on(2, { verb: "camera", args: `static ${v(p)} ${v(l)} 6 ${fov}` });
  wait(1500);
  const r = on(2, { verb: "capture", mode: "ui", name: `rv_${name}` }, 25000);
  H.setBoxes([box(2)]);
  const f = H.resolveCapture(r.detail);
  if (f) copyFileSync(f, join(STILLS, `${name}${f.slice(f.lastIndexOf("."))}`));
  on(2, { verb: "camera", args: "stop" });
  wait(300);
}

// ------------------------------------------------------------------ the story
const CLERK = "Roadmap_Fuel_Clerk";
const TILL = "Roadmap_Fuel_Till";
const PARK = "Roadmap_Fuel_Payphone"; // the camera body's spot for shots with no dialogue
// A bystander hears the clerk only within the dialogue's hear range (15 m, LF_Dialogue.Overhear), so for the robbery
// the camera body stands behind the kiosk at the FUEL STATION sign, 5.6 m from the clerk and hidden from every angle.
const EARSHOT = "Sign_FuelStation";

// The robber at the service window, pistol in hand, nothing running.
function robberAtWindow() {
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "pistol" }, 10000);
  stand(1, TILL, "s");
  on(1, { verb: "hold", item: "weapon" }, 10000);
  wait(1500);
}
// One robbery on a fixed clock (seconds from the camera start): talk, "This is a robbery" (the clerk's panic line),
// then the demand at DEMAND (the hold starts, the fuel intruder alarm and its beacon go off, the clerk complies).
export const TALK = 0.6;
export const DEMAND = 12.8; // the panic line is 8.2 s; a reply picked while it plays is refused (picked=false)
async function robbery(at) {
  await at(TALK); use(1, "LF_DialogueTalkAction", CLERK);
  await at(TALK + 1.3); dlg(1, "skip");
  await at(TALK + 2.0); dlg(1, "pick", "2");
  await at(DEMAND); dlg(1, "pick", "0");
}

// Camera poses, measured on the world (stills in README.md). Every robbery angle is the same robbery, shot again.
export const CAMS = {
  est: { from: [[262.5, 2.6, 225.5], [281, 1.5, 238]], to: [[263.9, 2.35, 226.9], [281, 1.5, 238]], fov: 50 },
  window: { from: [[286.9, 1.35, 232.9], [283.7, 1.25, 236.5]], to: [[286.2, 1.3, 233.7], [283.7, 1.25, 236.5]], fov: 38 },
  inside: { from: [[284.65, 1.66, 239.85], [283.7, 1.45, 236.3]], to: [[284.45, 1.63, 239.6], [283.7, 1.45, 236.3]], fov: 32 },
  side: { from: [[285.8, 1.7, 234.7], [283.2, 1.65, 236.7]], to: [[285.35, 1.62, 235.15], [283.2, 1.65, 236.7]], fov: 42 },
  beacon: { from: [[279.7, 1.6, 234.3], [282.3, 2.25, 236.8]], to: [[280.05, 1.55, 234.6], [282.3, 2.25, 236.8]], fov: 30 },
  bank: { from: [[198.3, 1.55, 255.3], [193.9, 1.3, 258.5]], to: [[197.5, 1.5, 255.9], [193.9, 1.3, 258.5]], fov: 46 },
  police: { from: [[263.6, 1.0, 252.75], [246, 1.2, 249.6]], to: [[263.0, 1.0, 252.9], [246, 1.2, 249.6]], fov: 28 },
};

const ROB = (cam, note) => ({ seconds: 24, cam, note, park: EARSHOT, prep: robberAtWindow, body: robbery });
export const SHOTS = {
  EST: { seconds: 8, cam: CAMS.est, note: "establishing, quiet: the station in the last sun",
    prep: () => { on(1, { verb: "resetcrime" }, 10000); stand(1, PARK, "e"); } },
  WINDOW: ROB(CAMS.window, "the robber at the service window, pistol in hand; push in"),
  INSIDE: ROB(CAMS.inside, "from inside past the clerk's shoulder: the panic line, then the alarm light"),
  SIDE: ROB(CAMS.side, "side close on the robber, the clerk behind the mesh, the alarm box top left"),
  BEACON: ROB(CAMS.beacon, "the alarm box on the kiosk wall: the beacon flashing, the intruder alarm"),
  BANK: { seconds: 10, scene: "trailer-bank", park: "Roadmap_Garage", cam: CAMS.bank, note: "the vault door swings open, the bank bell",
    prep: () => { on(1, { verb: "resetcrime" }, 10000); stand(1, "Roadmap_Bank_RobKit", "w"); on(1, { verb: "charge" }); },
    body: async (at) => { await at(1.5); use(1, "LF_BankRobAction", "Roadmap_Bank_Vault"); } },
  POLICE: { seconds: 14, scene: false, cam: CAMS.police, note: "the patrol car comes down the main street, lights and siren",
    prep: () => { on(1, { verb: "resetcrime" }, 10000); stand(1, PARK, "s"); },
    body: async (at) => { await at(0); on(2, { verb: "scene", args: `start robbery-police ${HOUR} Clear` }, 12000); } },
};

// ------------------------------------------------------------------ runners
function prep(s) {
  if (s.scene === false) {
    on(2, { verb: "scene", args: "stop" });
    on(2, { verb: "time", hour: HOUR });
  } else scene(2, s.scene ?? "trailer-fuel");
  stand(2, s.park ?? PARK, "n");
  if (s.prep) s.prep();
}

async function take(id, suffix = "") {
  const s = SHOTS[id];
  if (!s) throw new Error(`no shot ${id}`);
  log(`take ${id} (${s.note})`);
  prep(s);
  on(2, { verb: "camera", args: "hud off" });
  const pid = A.clientPid(join(dir, "session.json"), 2);
  const send = (spec, t) => on(2, spec, t);
  const resolveAlias = (alias) => {
    H.setBoxes([box(2)]);
    return H.resolveCapture(alias);
  };
  const out = join(TAKES, `${id}${suffix}.mp4`);
  let camStart = 0;
  const at = async (sec) => {
    const left = camStart + sec * 1000 - Date.now();
    if (left > 0) wait(left);
  };
  const r = await A.shootWithAudio({
    send, resolveAlias, name: `rv_${id}`, fps: 30, seconds: Math.ceil(s.seconds + 12), pid, out, label: `rv_${id}`, log,
    body: async () => {
      camStart = Date.now();
      on(2, { verb: "camera", args: camArgs(s.cam, s.seconds) });
      if (s.body) await s.body(at);
      await at(s.seconds + 0.3);
      on(2, { verb: "camera", args: "stop" });
    },
  });
  if (r.ok) log(`  ${id}: ${out} mean ${r.mean} dB drift ${r.driftMs?.toFixed?.(0)} ms offset ${r.offsetSec?.toFixed?.(3)}`);
  else {
    log(`  ${id}: audio failed (${r.error})`);
    if (r.clip?.ok) copyFileSync(r.clip.out, out);
  }
  if (r.clip) log(`  ${E.describe(r.clip)}`);
  // camStartSec: where the camera move starts in the clip (the start slate and the 1.2 s after it come first).
  writeFileSync(join(TAKES, `${id}${suffix}.json`), JSON.stringify({ id, seconds: s.seconds, audio: !!r.ok, summary: r.clip?.summary }, null, 2));
  on(1, { verb: "resetcrime" }, 10000);
  return r.ok;
}

function setup() {
  for (const n of [1, 2]) S.arrive(on, n, wait);
  for (const p of COSTUME) {
    on(1, { verb: "itemwear", prefab: p });
    wait(800);
  }
  for (const p of STRIP) on(1, { verb: "itemstrip", prefab: p });
  const worn = on(1, { verb: "itemwear", prefab: "list" }).detail;
  if (/Uniform_|Helmet|Vest|Jacket_M88|Pants_M88|Backpack/i.test(worn)) log(`WARNING the robber still wears military kit: ${worn}`);
}

const [cmd, arg, ...rest] = process.argv.slice(2);
if (cmd === "setup") setup();
else if (cmd === "still") {
  const [p, l, fov] = rest;
  still(arg, p.split(",").map(Number), l.split(",").map(Number), Number(fov || 50));
} else if (cmd === "take") {
  const suffix = rest.find((x) => x.startsWith("--as="))?.slice(5) ?? "";
  const ids = arg === "all" ? Object.keys(SHOTS) : arg.split(",");
  for (const id of ids) {
    try {
      await take(id, suffix);
    } catch (e) {
      log(`  ${id}: FAILED ${e.message}`);
    }
  }
} else if (cmd === "cmd") {
  const [verb, ...a] = rest;
  const spec = { verb };
  if (verb === "camera" || verb === "scene") spec.args = a.join(" ");
  else if (verb === "use") Object.assign(spec, { className: a[0], entityName: a[1] });
  else if (verb === "stand") Object.assign(spec, { entityName: a[0], side: a[1] });
  else if (verb === "dialogue") Object.assign(spec, { arg: a[0], value: a[1] });
  else if (verb === "hold") spec.item = a[0];
  else if (verb === "itemwear" || verb === "itemstrip") spec.prefab = a[0];
  on(Number(arg), spec, 25000);
} else if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) {
  console.log("usage: shoot.mjs setup | take <IDs|all> [--as=_2] | still <name> <p> <l> [fov] | cmd <n> <verb> ..");
}
