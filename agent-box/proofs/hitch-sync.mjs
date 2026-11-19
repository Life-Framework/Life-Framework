import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// #3284: a reaper hitched behind a UAZ469 follows it on every client, a late joiner included, and goes when the
// vehicle goes. Client 1 asks the server to spawn a UAZ469 and hitch a reaper (LF_ImplementHitchHarness). Client 2
// joins after the hitch, so it has only the join snapshot. The server drives the UAZ 50 m (the harness cannot sit
// in a seat, so the server pushes the empty UAZ east along its own axis; LF_ImplementHitchHarness). Each client's `hitch seen` must show the reaper as a proxy child of
// that UAZ, behind it, within GAP_M of where the vehicle's frame puts it, and the vehicle where the server says it
// is. Last, the server deletes the UAZ the way the garage does and the reaper must be gone on both clients.
//
//   session up --clients 1 --profile full ; session hitch-sync [out dir]
//   hitch-sync --selftest      the parser on made-up text, no game
const DRIVE_M = 50;
const DRIVE_SECONDS = 60;
const GAP_M = 0.15;
const SERVER_DRIFT_M = 4;

// `key=value` pairs from `hitch seen` / `hitch state`.
export function parseKv(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=(\S+)/g)) out[m[1]] = m[2];
  return out;
}

export function parsePos(text) {
  const p = String(text ?? "").split(",").map(Number);
  return p.length === 3 && p.every(Number.isFinite) ? p : null;
}

function selftest() {
  const fails = [];
  const s = parseKv("implements=1 implement=-2147481560 parent=-2147481823 vehicleAt=70,1.2,40 at=70,1.1,36.6 local=0,0,-3.4 behind=1 gap=0.01 proxy=1");
  if (s.implements !== "1" || s.parent !== "-2147481823" || s.behind !== "1" || s.proxy !== "1") fails.push("seen keys");
  if (parsePos(s.vehicleAt)?.join() !== "70,1.2,40") fails.push("seen vehicle position");
  if (parsePos("a,b") !== null) fails.push("position refuses junk");
  if (parseKv("implements=0").implements !== "0") fails.push("none seen");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "hitch-sync selftest: FAIL" : "hitch-sync selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runHitchSync(args) {
  if (args.includes("--selftest")) return selftest();
  const out = resolve(String(args.find((a) => !a.startsWith("--")) ?? "tmp/hitch-sync"));
  mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length ? "hitch-sync: FAIL" : `hitch-sync: PASS (the reaper follows the UAZ on both clients, the late joiner too, and goes with it; pictures in ${out})`);
    return fails.length ? 1 : 0;
  };
  const hitch = (n, word, timeout = 10000) => on(n, { verb: "hitch", words: [word] }, timeout);
  const seen = (n) => parseKv(hitch(n, "seen").detail);
  const server = () => parseKv(hitch(1, "state").detail);
  // Bounded: a late joiner streams the vehicle in a moment after it arrives.
  const seenOnce = (n, judge, seconds = 30) => {
    let last = {};
    for (let t = 0; t <= seconds; t += 2) {
      last = seen(n);
      if (judge(last)) return last;
      wait(2000);
    }
    return last;
  };
  const dist = (a, b) => (a && b ? Math.hypot(a[0] - b[0], a[2] - b[2]) : Infinity);
  const checkFollows = (n, s, label, vehicleId, serverAt) => {
    if (s.implements !== "1") { fails.push(`${label}: client ${n} sees ${s.implements ?? "?"} implements (wanted 1)`); return; }
    if (s.parent !== vehicleId) fails.push(`${label}: client ${n}'s reaper hangs off ${s.parent}, not the UAZ ${vehicleId}`);
    if (s.proxy !== "1" && s.proxy !== "true") fails.push(`${label}: client ${n}'s reaper is not a proxy (proxy=${s.proxy})`);
    if (s.behind !== "1" && s.behind !== "true") fails.push(`${label}: client ${n}'s reaper is not behind the UAZ`);
    if (!(Number(s.gap) >= 0 && Number(s.gap) <= GAP_M)) fails.push(`${label}: client ${n}'s reaper is ${s.gap} m from its hitch point`);
    if (serverAt && dist(parsePos(s.vehicleAt), serverAt) > SERVER_DRIFT_M) fails.push(`${label}: client ${n} has the UAZ at ${s.vehicleAt}, the server at ${serverAt.join(",")}`);
  };
  const shot = (n, name, at) => on(n, { verb: "capture", mode: "world", name, x: at[0], y: at[1], z: at[2], ox: -1.5, oy: 3, oz: 10, hour: 12 }, 25000);

  arrive(on, 1, wait);
  wait(6000);
  const spawned = hitch(1, "spawn", 15000);
  if (spawned.status !== "ok") { fails.push(`spawn: ${spawned.status} ${spawned.detail}`); return done(); }
  const vehicleId = parseKv(spawned.detail).vehicle;
  wait(3000);
  checkFollows(1, seenOnce(1, (s) => s.implements === "1"), "after the hitch", vehicleId, parsePos(server().at));

  // Client 2 joins after the hitch: it has only the join snapshot.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 2 did not join late"); return done(); }
  arrive(on, 2, wait);
  wait(6000);
  const startAt = parsePos(server().at);
  checkFollows(2, seenOnce(2, (s) => s.implements === "1"), "late join", vehicleId, startAt);
  if (startAt) shot(2, "hitch_sync_late_joiner_parked", startAt);

  hitch(1, "drive");
  let state = {};
  for (let t = 0; t <= DRIVE_SECONDS; t += 2) {
    wait(2000);
    state = server();
    if (Number(state.moved) >= DRIVE_M) break;
  }
  hitch(1, "stop");
  if (!(Number(state.moved) >= DRIVE_M)) fails.push(`drive: the server moved the UAZ only ${state.moved} m in ${DRIVE_SECONDS} s`);
  wait(4000);
  const endAt = parsePos(server().at);
  for (const n of [1, 2]) checkFollows(n, seenOnce(n, (s) => dist(parsePos(s.vehicleAt), endAt) <= SERVER_DRIFT_M, 20), `after ${state.moved} m`, vehicleId, endAt);
  if (endAt) {
    shot(2, "hitch_sync_late_joiner_after_drive", endAt);
    shot(1, "hitch_sync_driver_after_drive", endAt);
  }

  // The garage and impound delete a vehicle with DeleteEntityAndChildren: the reaper must not stay behind.
  hitch(1, "delete");
  for (const n of [1, 2]) {
    const s = seenOnce(n, (x) => x.implements === "0", 15);
    if (s.implements !== "0") fails.push(`delete: client ${n} still sees ${s.implements} implement(s) (parent=${s.parent}) after the UAZ was deleted`);
  }
  return done();
}

export default {
  name: "hitch-sync",
  noReset: true,
  usage: "hitch-sync [out dir] | --selftest   (session up --clients 1 --profile full; it joins client 2 itself)",
  run: (ctx) => runHitchSync(ctx.args ?? []),
};
