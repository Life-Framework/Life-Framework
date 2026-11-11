// Trailer v1 shoot driver (#571 / #398). Session: node agent-box/session.mjs roster 2, then
// node agent-box/session.mjs up --clients 2 --mission playtest --profile full --res 1920x1080 --sound --wait --hold 45. Drives the two session clients through the harness and records each take
// with the engine recorder plus process-loopback game audio (tools/film/audio.mjs).
//   node video/trailer-v1/shoot.mjs arrive              both clients through the arrival form (client 1 skipped with --keep1)
//   node video/trailer-v1/shoot.mjs scout <ids|all>     one still per camera pose (start, middle, end of a move)
//   node video/trailer-v1/shoot.mjs take <ids>          record the takes into tmp/trailer-v1/takes/<id>.mp4
//   node video/trailer-v1/shoot.mjs cmd <n> <verb> ...  one raw harness command (debugging)
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
const imp = (p) => import(pathToFileURL(join(ROOT, p)).href);
const H = await imp("agent-box/harness.mjs");
const S = await imp("agent-box/session.mjs");
const A = await imp("tools/film/audio.mjs");
const E = await imp("tools/film/engine.mjs");

const dir = S.sessionDir();
const WORK = join(ROOT, "tmp", "trailer-v1"); // git-ignored: takes, stills and the log
const TAKES = join(WORK, "takes");
const SCOUT = join(WORK, "scout");
mkdirSync(TAKES, { recursive: true });
mkdirSync(SCOUT, { recursive: true });
const LOG = join(WORK, "shoot.log");
const log = (s) => {
  console.log(s);
  writeFileSync(LOG, `${new Date().toISOString().slice(11, 19)} ${s}\n`, { flag: "a" });
};

const box = (n) => S.clientBox(dir, n);
function on(n, spec, t = 10000) {
  H.setBoxes([box(n)]);
  const r = H.sendCommand(spec, t);
  log(`  c${n} ${r.line.replace(/^\d+ (s:\S+ )?/, "")} -> ${r.status} ${String(r.detail).slice(0, 220)}`);
  return r;
}
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const state = (n) => S.parseState(on(n, { verb: "state" }).detail);
const posOf = (n) => String(state(n).pos ?? "0,0,0").split(",").map(Number);
const cam = (n, args) => on(n, { verb: "camera", args });
const stand = (n, e, side) => {
  const r = on(n, side ? { verb: "stand", entityName: e, side } : { verb: "stand", entityName: e });
  wait(1800);
  return r;
};
const use = (n, cls, e) => on(n, e ? { verb: "use", className: cls, entityName: e } : { verb: "use", className: cls });
const v = (a) => a.map((x) => (+x).toFixed(2)).join(" ");

// Scene light per take (LF_FilmSceneEnv presets: trailer-* hour 17.9 Clear unless an override is given).
function scene(n, name, hour = "-") {
  on(n, { verb: "scene", args: "stop" });
  const r = on(n, { verb: "scene", args: `start ${name} ${hour} Clear` }, 12000);
  wait(2500);
  return r;
}

// Camera poses for scouting: static [pos, look], drone [from, lookFrom, to, lookTo], orbit {point, r, az, arc, h}.
function orbitPose(o, k) {
  const a = ((o.az - o.arc / 2 + k * o.arc) * Math.PI) / 180;
  const p = [o.point[0] + Math.sin(a) * o.r, o.point[1] + o.h, o.point[2] + Math.cos(a) * o.r];
  return [p, [o.point[0], o.point[1] + 1.2, o.point[2]]];
}
function poses(shot) {
  const c = shot.cam;
  if (!c) return [];
  if (c.static) return [c.static];
  if (c.drone) return [[c.drone[0], c.drone[1]], [c.drone[2], c.drone[3]]];
  if (c.orbit) return [orbitPose(c.orbit, 0), orbitPose(c.orbit, 0.5), orbitPose(c.orbit, 1)];
  return [];
}
function camArgs(shot, seconds) {
  const c = shot.cam;
  const fov = c.fov ?? 55;
  if (c.static) return `static ${v(c.static[0])} ${v(c.static[1])} ${seconds} ${fov}`;
  if (c.drone) return `drone ${v(c.drone[0])} ${v(c.drone[1])} ${v(c.drone[2])} ${v(c.drone[3])} ${seconds} ${fov}`;
  if (c.orbit) {
    const o = c.orbit;
    return `orbit ${v(o.point)} ${o.r} ${seconds} ${o.az} ${o.arc} ${o.h}`;
  }
  return null;
}

// ------------------------------------------------------------------ shots
// film: the client whose screen is recorded. park: where the filming client's own body waits (out of frame).
// prep(): before recording. body(at): during recording, at(sec) waits until that many seconds after the camera starts.
const P2 = () => `Player${state(2).player}`;
const SHOTS = {
  // ---- spawn square
  S06: { film: 1, scene: "trailer-square", seconds: 9, note: "first person: arrival form R-89 (fresh character only)",
    body: async (at) => {
      await at(1.5); on(1, { verb: "type", widget: "RegFirstName", text: "Ada" });
      await at(2.4); on(1, { verb: "type", widget: "RegLastName", text: "Novak" });
      await at(3.2); on(1, { verb: "type", widget: "RegAge", text: "29" });
      await at(4.6); on(1, { verb: "click", widget: "SignButton" });
    } },
  S05: { film: 2, scene: "trailer-square", seconds: 7, park2: "SpawnHub_Globe",
    prep: () => stand(1, "Quest_Clerk", "w"),
    cam: { drone: [[110, 3, 252], [130, 1, 250], [118, 14, 250], [170, 0, 250]], fov: 60 } },
  S22: { film: 1, scene: "trailer-square", seconds: 8, note: "first person: travel globe list, pick Farm",
    prep: () => { stand(1, "SpawnHub_Globe"); on(1, { verb: "look" }); },
    body: async (at) => {
      await at(1.0); use(1, "LF_SpawnHubAction", "SpawnHub_Globe");
      await at(4.0); on(1, { verb: "click", widget: "HubGo_farm_Action" });
    } },
  S23: { film: 2, scene: "trailer-square", seconds: 12, park2: "SpawnHub_Globe",
    prep: () => { stand(1, "Village_Payphone"); },
    body: async (at) => { await at(2); use(1, "LF_PayphoneAction", "Village_Payphone"); },
    cam: { orbit: { point: [126, 0, 250], r: 15, az: 270, arc: 80, h: 6 } } },
  R2: { film: 2, scene: "trailer-square", seconds: 7, park2: "SpawnHub_Globe", prep: () => stand(1, "Quest_Clerk", "w"),
    cam: { drone: [[108.5, 7.5, 244.5], [127, 0, 250], [110, 8, 246.5], [128, 0, 250]], fov: 58 } },
  S28: { film: 2, scene: "trailer-square-dusk", hour: 18.15, seconds: 8, park2: "SpawnHub_Globe", prep: () => stand(1, "SpawnHub_Globe", "w"),
    cam: { static: [[132, 2.2, 250], [200, 1.5, 248]], fov: 55 } },
  // ---- store
  S07: { film: 2, scene: "trailer-store", seconds: 8, park2: "SpawnHub_Globe", prep: () => stand(1, "SpawnHub_Globe", "w"),
    cam: { orbit: { point: [180, 0.6, 245], r: 9, az: 0, arc: 90, h: 3 } } },
  R1: { film: 2, scene: "trailer-store", seconds: 7, park2: "SpawnHub_Globe",
    cam: { drone: [[180.6, 1.8, 253.5], [180, 1.7, 244], [180.4, 1.7, 251], [180, 1.6, 244]], fov: 55 } },
  // ---- bank and cash van (client 2 films off duty, so no dispatch toast lands on the recorded screen)
  S14: { film: 1, scene: "trailer-bank", seconds: 9, note: "first person: the teller",
    prep: () => { stand(1, "Bank_Teller"); on(1, { verb: "look" }); },
    body: async (at) => {
      await at(1.2); use(1, "LF_ATMAction", "Bank_Teller");
      await at(3.4); on(1, { verb: "click", widget: "Key1Button" });
      await at(3.9); on(1, { verb: "click", widget: "Key0Button" });
      await at(4.4); on(1, { verb: "click", widget: "Key0Button" });
      await at(5.6); on(1, { verb: "click", widget: "WithdrawButton" });
    } },
  S01: { film: 2, scene: "trailer-bank", seconds: 15, park2: "Roadmap_Garage",
    prep: () => { stand(1, "Roadmap_Cash_Van_Box"); on(1, { verb: "look" }); on(1, { verb: "drill" }); },
    body: async (at) => { await at(1.0); use(1, "LF_HeistRobAction", "Roadmap_Cash_Van_Box"); },
    cam: { drone: [[201.8, 1.7, 258.4], [197.4, 0.7, 253.7], [201.0, 1.6, 258.9], [197.4, 0.7, 253.7]], fov: 50 } },
  S19: { film: 2, scene: "trailer-bank", seconds: 13, park2: "Roadmap_Garage",
    prep: () => { stand(1, "Roadmap_Bank_Vault"); on(1, { verb: "look" }); on(1, { verb: "drill" }); },
    body: async (at) => { await at(1.0); use(1, "LF_BankRobAction", "Roadmap_Bank_Vault"); },
    cam: { static: [[198.4, 1.6, 267.4], [194.4, 1.1, 265.0]], fov: 50 } },
  // ---- police (client 2 on duty from here)
  S15: { film: 1, scene: "trailer-police", seconds: 7, note: "client 2 clocks in at the duty desk",
    prep: () => { stand(2, "Roadmap_Police_Desk"); on(2, { verb: "look" }); stand(1, "Roadmap_Police_Payphone"); },
    body: async (at) => { await at(1.5); use(2, "LF_DutyAction", "Roadmap_Police_Desk"); },
    cam: { static: [[236.5, 1.7, 245.0], [233.0, 1.0, 239.8]], fov: 50 } },
  S04: { film: 1, scene: "trailer-police", seconds: 9, note: "client 2 at the wheel, lights on",
    prep: () => { on(2, { verb: "engine", entityName: "Roadmap_Police_Car" }); wait(2500); on(2, { verb: "siren", entityName: "Roadmap_Police_Car", mode: 1 }); wait(1500); stand(1, "Roadmap_Police_Payphone"); },
    cam: { orbit: { point: [243.5, 0.3, 246], r: 7.5, az: 0, arc: 110, h: 2.2 } } },
  S16: { film: 2, scene: "trailer-police", seconds: 7, note: "first person: dispatch board",
    prep: () => { on(2, { verb: "siren", entityName: "Roadmap_Police_Car", mode: 0 }); on(2, { verb: "engine", entityName: "out" }); wait(1500); stand(2, "Roadmap_Police_DispatchBoard"); on(2, { verb: "look" }); },
    body: async (at) => { await at(1.0); use(2, "LF_WantedBoardReadAction", "Roadmap_Police_DispatchBoard"); },
    after: () => on(2, { verb: "click", widget: "Close" }) },
  S03: { film: 2, scene: "trailer-square", seconds: 6, park2: "SpawnHub_Globe", note: "client 1 at the square payphone",
    prep: () => { stand(1, "Village_Payphone"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.0); use(1, "LF_PayphoneAction", "Village_Payphone"); },
    cam: { drone: [[133.8, 1.6, 237.6], [130.0, 1.3, 240.5], [133.2, 1.55, 238.2], [130.0, 1.3, 240.5]], fov: 50 } },
  // ---- hospital
  S18: { film: 1, scene: "trailer-hospital", seconds: 7, prep: () => { stand(2, "Roadmap_Police_Desk"); stand(1, "Roadmap_Police_Payphone"); },
    cam: { drone: [[259.0, 1.8, 242.6], [266, 1.3, 250.5], [260.2, 1.7, 244.0], [266, 1.3, 250.5]], fov: 55 } },
  S17: { film: 2, scene: "trailer-hospital", seconds: 11, note: "client 1 medic does CPR on client 2, then the revive",
    prep: () => {
      stand(1, "Roadmap_Hospital_Duty_Desk"); on(1, { verb: "look" }); use(1, "LF_DutyAction", "Roadmap_Hospital_Duty_Desk"); wait(2000);
      stand(2, "Sign_HospitalBay", "w"); stand(1, "Sign_HospitalBay", "w"); on(1, { verb: "shoot", playerId: 2 }); wait(2500);
      SHOTS.S17.patient = P2(); stand(1, SHOTS.S17.patient); on(2, { verb: "click", widget: "BillClose" });
      const m = posOf(1), p = posOf(2);
      SHOTS.S17.cam = { orbit: { point: [(m[0] + p[0]) / 2, m[1], (m[2] + p[2]) / 2], r: 4.5, az: 200, arc: 70, h: 1.6 } };
    },
    body: async (at) => { await at(0.8); on(1, { verb: "cpr", playerId: 2, mode: "start" }); await at(5.8); on(1, { verb: "cpr", playerId: 2, mode: "stop" }); use(1, "LF_ReviveAction", SHOTS.S17.patient); },
    cam: { orbit: { point: [259.5, 0, 252.6], r: 4.5, az: 200, arc: 70, h: 1.6 } } },
  // ---- farm (S22 lands client 1 here)
  S09: { film: 2, scene: "trailer-farm", seconds: 7, park2: "Farm_PressTable",
    prep: () => { stand(1, "Farm_SeedCrate"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.2); use(1, "LF_GatherAction", "Farm_SeedCrate"); },
    cam: { drone: [[148.5, 1.8, 291.0], [146.5, 0.6, 298.5], [147.2, 2.7, 290.0], [147.0, 0.4, 299.0]], fov: 55 } },
  S10: { film: 2, scene: "trailer-farm", seconds: 7, park2: "Farm_SeedCrate",
    prep: () => { stand(1, "AppleGatherTree", "s"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.0); use(1, "LF_GatherAction", "AppleGatherTree"); },
    cam: { static: [[136.2, 1.2, 302.8], [130.0, 1.6, 310.0]], fov: 50 } },
  S10b: { film: 2, scene: "trailer-farm", seconds: 7, park2: "Farm_SeedCrate",
    prep: () => { on(1, { verb: "apples" }); stand(1, "AppleProcessor"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.0); use(1, "LF_ProcessAction", "AppleProcessor"); },
    cam: { static: [[143.6, 1.5, 308.4], [141.0, 1.0, 311.0]], fov: 50 } },
  R4: { film: 2, scene: "trailer-farm", hour: 17.7, seconds: 7, park2: "AppleProcessor", prep: () => stand(1, "AppleProcessor"),
    cam: { drone: [[139, 3.2, 289], [150, 0.4, 302], [141.5, 3.8, 288.5], [151, 0.4, 302]], fov: 58 } },
  // ---- quarry
  S11: { film: 2, scene: "trailer-quarry", seconds: 11, park2: "Furnace",
    prep: () => {
      stand(1, "Pickaxe_Debug"); use(1, "SCR_EquipWeaponAction", "Pickaxe_Debug"); wait(2500);
      on(1, { verb: "input", action: "SwitchWeaponCategory2" }); wait(2500);
      stand(1, "IronOre_Debug"); on(1, { verb: "input", action: "CharacterTurnUp", value: "-30" }); wait(800);
    },
    body: async (at) => { for (const t of [0.8, 4.0, 7.2]) { await at(t); on(1, { verb: "input", action: "CharacterMelee" }); } },
    cam: { static: [[184.9, 1.3, 169.4], [181.9, 0.7, 171.7]], fov: 50 } },
  S12: { film: 2, scene: "trailer-quarry", seconds: 7, park2: "IronOre_Debug",
    prep: () => { on(1, { verb: "input", action: "SwitchWeaponCategory2" }); on(1, { verb: "nuggets" }); stand(1, "Furnace"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.0); use(1, "LF_ProcessAction", "Furnace"); },
    cam: { static: [[166.8, 1.5, 168.6], [169.9, 1.0, 171.5]], fov: 50 } },
  // ---- v2 reshoots (independent review, 2026-11-11)
  G2: { film: 1, scene: "trailer-square", seconds: 8, note: "first person: travel globe list, pick Bank, land at the bank",
    prep: () => { stand(1, "SpawnHub_Globe"); on(1, { verb: "look" }); },
    body: async (at) => {
      await at(1.0); use(1, "LF_SpawnHubAction", "SpawnHub_Globe");
      await at(3.2); on(1, { verb: "click", widget: "HubGo_bank_Action" });
    } },
  T1: { film: 2, scene: "trailer-bank", seconds: 7, park2: "Roadmap_Garage", note: "the player at the teller window, the teller in frame",
    prep: () => { stand(1, "Bank_Teller"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.5); use(1, "LF_ATMAction", "Bank_Teller"); },
    after: () => on(1, { verb: "click", widget: "ExitButton" }),
    cam: { drone: [[179.4, 1.7, 255.8], [176.6, 1.3, 259.2], [179.0, 1.65, 256.4], [176.6, 1.3, 259.3]], fov: 45 } },
  O1: { film: 2, scene: "trailer-bank", seconds: 14, park2: "Roadmap_Garage", note: "cold open: the robber drills the van box from frame one",
    prep: () => { stand(1, "Roadmap_Cash_Van_Box"); on(1, { verb: "look" }); on(1, { verb: "drill" }); },
    body: async (at) => { await at(0.8); use(1, "LF_HeistRobAction", "Roadmap_Cash_Van_Box"); },
    cam: { drone: [[200.4, 1.3, 256.9], [197.4, 0.6, 253.8], [199.8, 1.2, 257.3], [197.4, 0.55, 253.8]], fov: 45 } },
  V1: { film: 2, scene: "trailer-bank", seconds: 15, park2: "Roadmap_Garage", note: "the vault door on the bank's east wall, the bank front in frame",
    prep: () => { stand(1, "Roadmap_Bank_Vault"); on(1, { verb: "look" }); on(1, { verb: "drill" }); },
    body: async (at) => { await at(1.0); use(1, "LF_BankRobAction", "Roadmap_Bank_Vault"); },
    cam: { drone: [[200.8, 2.3, 256.6], [194.3, 1.2, 263.6], [200.0, 2.2, 257.6], [194.3, 1.2, 263.8]], fov: 52 } },
  M2: { film: 2, scene: "trailer-quarry", seconds: 7, park2: "IronOre_Debug", note: "smelt: the bar drops out of the boiler",
    prep: () => { on(1, { verb: "nuggets" }); stand(1, "Furnace"); on(1, { verb: "look" }); },
    body: async (at) => { await at(1.2); use(1, "LF_ProcessAction", "Furnace"); },
    cam: { static: [[167.4, 1.35, 169.4], [170.0, 0.6, 171.8]], fov: 48 } },
  C2: { film: 2, scene: "trailer-chop", hour: 17.9, seconds: 7, park2: "Roadmap_Hideout_Door", prep: () => stand(1, "Roadmap_Hideout_Door", "w"),
    cam: { drone: [[317.6, 1.9, 295.4], [323.0, 0.6, 301.0], [318.8, 1.8, 296.8], [323.0, 0.6, 301.0]], fov: 50 } },
  P1: { film: 1, scene: "trailer-police", seconds: 7, note: "an officer on duty reads the wanted board",
    prep: () => { stand(2, "Roadmap_Police_Desk"); on(2, { verb: "look" }); use(2, "LF_DutyAction", "Roadmap_Police_Desk"); wait(2500); stand(2, "Roadmap_Police_WantedBoard", "n"); on(2, { verb: "look" }); stand(1, "Roadmap_Garage"); },
    body: async (at) => { await at(1.0); use(2, "LF_WantedBoardReadAction", "Roadmap_Police_WantedBoard"); },
    after: () => on(2, { verb: "click", widget: "Close" }),
    cam: { drone: [[238.9, 1.65, 247.6], [235.4, 1.3, 243.0], [238.4, 1.6, 247.0], [235.4, 1.3, 243.0]], fov: 45 } },
  // ---- crime side
  S20: { film: 2, scene: "trailer-chop", hour: 17.9, seconds: 8, park2: "Roadmap_Hideout_Door", prep: () => stand(1, "Roadmap_Hideout_Door", "w"),
    cam: { orbit: { point: [322, 0.4, 301], r: 10, az: 180, arc: 90, h: 4.5 } } },
  R3: { film: 2, scene: "trailer-chop", hour: 17.9, seconds: 7, park2: "Roadmap_Lockpick_Car",
    cam: { drone: [[294.5, 2.2, 292.0], [304, 1.0, 297.0], [295.5, 2.4, 293.5], [304, 1.0, 297.5]], fov: 55 } },
  S21: { film: 2, scene: "trailer-convoy", seconds: 7, park2: "Roadmap_Convoy_Start", prep: () => stand(1, "Roadmap_Convoy_Start"),
    cam: { drone: [[467.5, 2.4, 315.5], [474, 1.0, 324.5], [465.5, 3.6, 317.5], [473, 1.0, 326]], fov: 55 } },
};

// ------------------------------------------------------------------ runners
function prepShot(id, s) {
  if (s.scene) scene(s.film, s.scene, s.hour ?? "-");
  if (s.park2 && s.film === 2) stand(2, s.park2, "n");
  if (s.prep) s.prep();
}

async function scout(ids) {
  for (const id of ids) {
    const s = SHOTS[id];
    if (!s) throw new Error(`no shot ${id}`);
    log(`scout ${id}`);
    prepShot(id, s);
    const ps = poses(s);
    if (!ps.length) {
      on(s.film, { verb: "camera", args: "hud off" });
      const r = on(s.film, { verb: "capture", mode: "ui", name: `tv1_${id}` }, 25000);
      keep(r, s.film, `${id}`);
      continue;
    }
    ps.forEach((p, k) => {
      cam(s.film, `static ${v(p[0])} ${v(p[1])} 4 ${s.cam.fov ?? 55}`);
      wait(1500);
      const r = on(s.film, { verb: "capture", mode: "ui", name: `tv1_${id}_${k}` }, 25000);
      keep(r, s.film, `${id}_${k}`);
      cam(s.film, "stop");
      wait(300);
    });
  }
}

function keep(r, n, name) {
  H.setBoxes([box(n)]);
  const f = H.resolveCapture(r.detail);
  if (f) {
    copyFileSync(f, join(SCOUT, `${name}${f.slice(f.lastIndexOf("."))}`));
    log(`  still -> scout/${name}`);
  } else log(`  still missing for ${name}: ${r.status} ${r.detail}`);
}

async function take(id) {
  const s = SHOTS[id];
  if (!s) throw new Error(`no shot ${id}`);
  log(`take ${id} (${s.note || ""})`);
  prepShot(id, s);
  const n = s.film;
  on(n, { verb: "camera", args: "hud off" });
  const pid = A.clientPid(join(dir, "session.json"), n);
  const send = (spec, t) => on(n, spec, t);
  const resolveAlias = (alias) => {
    H.setBoxes([box(n)]);
    return H.resolveCapture(alias);
  };
  const out = join(TAKES, `${id}.mp4`);
  let camStart = 0;
  const at = async (sec) => {
    const left = camStart + sec * 1000 - Date.now();
    if (left > 0) wait(left);
  };
  const r = await A.shootWithAudio({
    send, resolveAlias, name: `tv1_${id}`, fps: 30, seconds: Math.ceil(s.seconds + 10), pid, out, label: `tv1_${id}`, log,
    body: async () => {
      camStart = Date.now();
      const args = s.cam ? camArgs(s, s.seconds) : null;
      if (args) cam(n, args);
      if (s.body) await s.body(at);
      await at(s.seconds + 0.3);
      if (args) cam(n, "stop");
    },
  });
  if (s.after) s.after();
  if (r.ok) log(`  ${id}: ${out} mean ${r.mean} dB drift ${r.driftMs?.toFixed?.(0)} ms offset ${r.offsetSec?.toFixed?.(3)}`);
  else {
    log(`  ${id}: audio failed (${r.error})`);
    if (r.clip?.ok) {
      copyFileSync(r.clip.out, out);
      log(`  ${id}: kept the silent clip ${out}`);
    }
  }
  if (r.clip) log(`  ${E.describe(r.clip)}`);
  // Where the camera started inside the take: the start slate is at 0.3 s and the body 1.2 s after it.
  const meta = { id, seconds: s.seconds, camStartSec: 1.5, audio: !!r.ok, summary: r.clip?.summary };
  writeFileSync(join(TAKES, `${id}.json`), JSON.stringify(meta, null, 2));
  return r.ok || r.clip?.ok;
}

const [cmd, arg, ...rest] = process.argv.slice(2);
if (cmd === "arrive") {
  for (const n of [1, 2]) if (!(n === 1 && rest.concat(arg ?? []).includes("--keep1"))) S.arrive(on, n, wait);
} else if (cmd === "scout") {
  await scout(arg === "all" ? Object.keys(SHOTS) : arg.split(","));
} else if (cmd === "take") {
  for (const id of arg.split(",")) {
    try {
      await take(id);
    } catch (e) {
      log(`  ${id}: FAILED ${e.message}`);
    }
  }
  on(1, { verb: "camera", args: "hud on" });
} else if (cmd === "cmd") {
  const [verb, ...a] = rest;
  const spec = { verb };
  if (verb === "camera" || verb === "scene") spec.args = a.join(" ");
  else if (verb === "use") Object.assign(spec, { className: a[0], entityName: a[1] });
  else if (verb === "stand") Object.assign(spec, { entityName: a[0], side: a[1] });
  else if (verb === "click") spec.widget = a[0];
  else if (verb === "capture") Object.assign(spec, { mode: "ui", name: a[0] });
  else if (verb === "siren") Object.assign(spec, { entityName: a[0], mode: Number(a[1]) });
  else if (verb === "engine") spec.entityName = a[0];
  else if (verb === "input") Object.assign(spec, { action: a[0], value: a[1] });
  const r = on(Number(arg), spec, 25000);
  if (verb === "capture") keep(r, Number(arg), a[0]);
} else {
  console.log("usage: shoot.mjs arrive [--keep1] | scout <ids|all> | take <ids> | cmd <n> <verb> ...");
}
