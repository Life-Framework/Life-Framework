import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { ROOT, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// Background sound (docs/architecture/atmosphere.md) on one client. An agent cannot listen, so the
// evidence is the client's own [LFDebug:Atmosphere] lines: `start` when the mix gives an emitter a
// voice, `voice up` when the engine's handle for it is valid and has not finished (a loop the engine
// is mixing), `stop` with the reason. The client walks from a quiet spot to each emitter and back,
// switches the store radio off and on through its action, and moves the clock past the day sounds'
// hours. Two frame-time windows (quiet spot, quarry with its voices) go in the perf note.
// The session must run with sound (`session up --profile full --sound`): a -noSound client has no
// voices to report. With sound the proof also records what the client process plays (the film
// pipeline's process loopback) and measures the level in the last seconds of every step, and the
// recording stays in tmp/ for a human to listen to.
const QUIET = "Lot_Display_uaz469";
const STOPS = [
  { at: "Furnace", starts: ["Furnace", "MiningArea_Debug"], silent: ["Sawmill_Debug"], note: "the furnace and the quarry; the sawmill is off until a log is cut" },
  { at: "Moonshine_Still", starts: [], silent: ["Moonshine_Still"], note: "an empty still is silent" },
  { at: "Roadmap_Police_Desk", starts: ["Roadmap_Police_Desk"], note: "the duty desk" },
  { at: "Roadmap_ChopShop", starts: ["Roadmap_ChopShop"], note: "the chop yard" },
  { at: "FarmPlot_2", starts: ["FarmPlot_"], note: "one voice for the three farm plots" },
  { at: "Sign_PubHanging", starts: ["Atmosphere_PubRadio", "Atmosphere_Square"], note: "the pub and the square" },
];

async function runAtmosphereProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const logFile = () => newestLog(sessionPaths(dir).clients[0].logs);
  const lines = () => { const f = logFile(); return f ? readFileSync(f, "utf8").split("\n").filter((l) => l.includes("[LFDebug:Atmosphere]")).map((l) => l.replace(/^.*\[LFDebug:Atmosphere\]\s*/, "").trim()) : []; };
  const fails = [];
  const record = [];
  const marks = [];
  let capture = null;
  const wav = join(ROOT, "tmp", "atmosphere-proof.wav");
  let audio;
  try {
    audio = await import("../../tools/film/audio.mjs");
    capture = await audio.startCapture({ pid: audio.clientPid(sessionPaths(dir).state, 1), wav });
  } catch (e) {
    console.log(`note: no audio capture (${String(e.message).slice(0, 160)}); the log lines are the evidence`);
  }
  let seen = 0;
  // What the client logged since the last step.
  const step = (label, ms = 5000) => { wait(ms); if (capture) marks.push({ label, endMs: Date.now() - capture.epochMs }); const all = lines(); const fresh = all.slice(seen); seen = all.length; record.push(`## ${label}`, ...fresh.map((l) => `  ${l}`)); for (const l of fresh) console.log(`    ${l}`); return fresh; };
  const has = (fresh, verb, who) => fresh.some((l) => l.startsWith(verb + " ") && l.includes(who));

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "time", hour: 12 }, 10000);
  on(1, { verb: "stand", entityName: QUIET });
  step("settle at the car lot (nothing in range)", 5000);
  const playingAtStart = lines().filter((l) => l.startsWith("mix ")).pop();

  // The store radio: range, the engine's voice, the switch, range again.
  on(1, { verb: "stand", entityName: "Shop_GeneralStore_Radio" });
  let fresh = step("walk to the general store radio");
  if (!has(fresh, "start", "Shop_GeneralStore_Radio")) fails.push("the store radio did not start when the client stood at it");
  if (!has(fresh, "voice up", "Shop_GeneralStore_Radio")) fails.push("the engine reported no looping voice for the store radio");
  on(1, { verb: "look" });
  const off = on(1, { verb: "use", className: "LF_AmbientSwitchAction", entityName: "Shop_GeneralStore_Radio" }, 10000);
  fresh = step("switch the radio off");
  if (off.status !== "ok") fails.push(`switch off returned ${off.status} ${off.detail}`);
  if (!fresh.some((l) => l.startsWith("stop Shop_GeneralStore_Radio") && l.includes("condition"))) fails.push("the radio did not stop when it was switched off");
  on(1, { verb: "use", className: "LF_AmbientSwitchAction", entityName: "Shop_GeneralStore_Radio" }, 10000);
  fresh = step("switch the radio on");
  if (!has(fresh, "start", "Shop_GeneralStore_Radio")) fails.push("the radio did not start again when it was switched on");
  on(1, { verb: "stand", entityName: QUIET });
  fresh = step("walk back to the car lot");
  if (!fresh.some((l) => l.startsWith("stop Shop_GeneralStore_Radio") && l.includes("out of range"))) fails.push("the radio did not stop out of range");
  const quiet = on(1, { verb: "perf", seconds: 5, label: "quiet" }, 20000).detail;

  let busy = "";
  for (const s of STOPS) {
    on(1, { verb: "stand", entityName: s.at });
    fresh = step(`walk to ${s.at} (${s.note})`);
    for (const who of s.starts) {
      if (!has(fresh, "start", who)) fails.push(`${s.at}: ${who} did not start`);
      if (!has(fresh, "voice up", who)) fails.push(`${s.at}: the engine reported no looping voice for ${who}`);
    }
    for (const who of s.silent || []) if (has(fresh, "start", who)) fails.push(`${s.at}: ${who} started and should be silent`);
    if (s.at === "Furnace") busy = on(1, { verb: "perf", seconds: 5, label: "quarry" }, 20000).detail;
  }

  // Hours: at 23:00 the square (6 to 22) stops and the pub (10 to 2) plays on.
  on(1, { verb: "time", hour: 23 }, 10000);
  fresh = step("clock to 23:00 at the pub sign", 4000);
  if (!fresh.some((l) => l.startsWith("stop Atmosphere_Square") && l.includes("hours"))) fails.push("the square did not stop outside its hours");
  if (fresh.some((l) => l.startsWith("stop Atmosphere_PubRadio"))) fails.push("the pub stopped at 23:00, inside its hours");
  on(1, { verb: "time", hour: 12 }, 10000);
  step("clock back to 12:00", 3000);

  const errors = lines().filter((l) => /no voice|stays silent/.test(l));
  for (const e of errors) fails.push(`client error: ${e}`);
  const last = lines().filter((l) => l.startsWith("mix ")).pop() || "";
  record.push("## perf", `  quiet spot: ${quiet}`, `  quarry (furnace and quarry voices): ${busy}`, `  first mix line: ${playingAtStart || "(none: nothing had started)"}`, `  last mix line: ${last}`);
  if (capture) {
    const done = await capture.stop();
    const film = await import("../../tools/film/film.mjs");
    const ff = film.resolveFfmpeg();
    record.push("## what the client played (process loopback, mean and peak dBFS over the last 2.5 s of each step)");
    console.log(`audio: ${wav} (${done.frames} frames, peak ${done.peakDb} dB)`);
    for (const m of marks) {
      const from = Math.max(0, m.endMs / 1000 - 2.5);
      const r = spawnSync(ff.path, ["-hide_banner", "-nostats", "-ss", from.toFixed(2), "-t", "2.5", "-i", wav, "-vn", "-af", "volumedetect", "-f", "null", "-"], { windowsHide: true, encoding: "utf8" });
      const v = audio.parseVolume(r.stderr || "");
      const line = `  ${from.toFixed(1).padStart(6)} s  mean ${String(v.mean).padStart(6)} dB  peak ${String(v.max).padStart(6)} dB  ${m.label}`;
      record.push(line);
      console.log(line);
    }
  }
  const out = join(ROOT, "tmp", "atmosphere-proof.txt");
  mkdirSync(join(ROOT, "tmp"), { recursive: true });
  writeFileSync(out, record.join("\n") + "\n");
  console.log(`perf quiet:  ${quiet}`);
  console.log(`perf quarry: ${busy}`);
  console.log(`last mix: ${last}`);
  console.log(`record: ${out}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "atmosphere proof: FAIL" : "atmosphere proof: PASS (each emitter starts in range with a looping engine voice, stops out of range, the radio follows its switch, the square keeps its hours)");
  return fails.length ? 1 : 0;
}

export default {
  name: "atmosphere-proof",
  usage: "atmosphere-proof",
  run: (ctx) => runAtmosphereProof(),
};
