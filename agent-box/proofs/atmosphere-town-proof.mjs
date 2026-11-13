import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { rosterHas } from "./scanner-proof.mjs";
import { ROOT, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The second batch of town sound (#897, docs/architecture/atmosphere.md) on one client: bank
// counters, hospital beds and duty desk, a payphone while its receiver is up, the cash van's idle,
// and the radio on the pub terrace taking the music from the building and giving it back when it
// is switched off. Evidence is the client's own [LFDebug:Atmosphere] lines (`start`, `voice up`,
// `stop` with the reason); on `session up --profile full --sound` the proof also records what the
// client process plays and measures the level in the last 2.5 s of each step.
// Needs `session up --clients 2` with client 2 on the police roster (the payphone line goes busy when
// an on-duty officer can be rung). Not driven here: a real fuelling step (needs a vehicle and the vanilla refuel action; the server
// side is LF_Test_AmbientFuelPump) and driving the van off.
const QUIET = "Lot_Display_uaz469";
// The pub building is not named in its layer, so the client logs the sound project.
const PUB_BUILDING = "Atmosphere_PubRadio";
const PHONE = "Roadmap_Hospital_Payphone";

async function runTownProof() {
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
  const wav = join(ROOT, "tmp", "atmosphere-town-proof.wav");
  let audio;
  try {
    audio = await import("../../tools/film/audio.mjs");
    capture = await audio.startCapture({ pid: audio.clientPid(sessionPaths(dir).state, 1), wav });
  } catch (e) {
    console.log(`note: no audio capture (${String(e.message).slice(0, 160)}); the log lines are the evidence`);
  }
  let seen = 0;
  const step = (label, ms = 5000) => { wait(ms); if (capture) marks.push({ label, endMs: Date.now() - capture.epochMs }); const all = lines(); const fresh = all.slice(seen); seen = all.length; record.push(`## ${label}`, ...fresh.map((l) => `  ${l}`)); for (const l of fresh) console.log(`    ${l}`); return fresh; };
  const has = (fresh, verb, who, why) => fresh.some((l) => l.startsWith(verb + " " + who) && (!why || l.includes(why)));
  const expectStart = (fresh, who, where) => {
    if (!has(fresh, "start", who)) fails.push(`${where}: ${who} did not start`);
    else if (!has(fresh, "voice up", who)) fails.push(`${where}: the engine reported no looping voice for ${who}`);
  };

  // The payphone only goes busy when somebody on duty can be rung, so client 2 is the officer:
  // `session.mjs roster 2` before `up --clients 2`. Client 1 is the one listened to.
  if (!rosterHas(dir, 2)) {
    console.log("atmosphere town proof: FAIL  client 2 is not on the police roster; session.mjs down, session.mjs roster 2, then up --clients 2");
    return 1;
  }
  arrive(on, 1, wait);
  arrive(on, 2, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2000);
  on(2, { verb: "stand", entityName: "Roadmap_Dispatch_Phone" });
  on(1, { verb: "time", hour: 12 }, 10000);
  on(1, { verb: "stand", entityName: QUIET });
  step("settle at the car lot (nothing in range)", 5000);

  // Bank: the east counter murmurs, the west counter ticks; at the west one both are in range.
  on(1, { verb: "stand", entityName: "Bank_Counter_West" });
  let fresh = step("bank, west counter (the till and the clock)");
  expectStart(fresh, "Bank_Counter_West", "bank");
  expectStart(fresh, "Bank_Counter_East", "bank");

  // Hospital: beds share one voice (group), the duty desk has the ward.
  on(1, { verb: "stand", entityName: "Hospital_Bed_1" });
  fresh = step("hospital beds (one voice for three beds) and the duty desk");
  if (!fresh.some((l) => l.startsWith("start Hospital_Bed_"))) fails.push("hospital: no bed started its equipment hum");
  expectStart(fresh, "Roadmap_Hospital_Duty_Desk", "hospital");

  // Payphone: silent with the receiver down, hums while the line is BUSY, stops on hang-up.
  on(1, { verb: "stand", entityName: PHONE });
  fresh = step("walk to the hospital payphone (receiver down)");
  if (has(fresh, "start", PHONE)) fails.push("payphone: hummed with the receiver down");
  const dial = on(1, { verb: "use", className: "LF_PhoneDialAction", entityName: PHONE }, 10000);
  fresh = step("dial from the payphone (receiver up)", 6000);
  if (dial.status !== "ok") fails.push(`payphone: dial returned ${dial.status} ${dial.detail}`);
  expectStart(fresh, PHONE, "payphone");
  const hang = on(1, { verb: "use", className: "LF_PhoneHangUpAction", entityName: PHONE }, 10000);
  fresh = step("hang up", 5000);
  if (hang.status !== "ok") fails.push(`payphone: hang up returned ${hang.status} ${hang.detail}`);
  if (!has(fresh, "stop", PHONE, "condition")) fails.push("payphone: the hum did not stop on hang-up");

  // Cash van: idles at its stop.
  on(1, { verb: "stand", entityName: "Roadmap_Cash_Van" });
  fresh = step("walk to the cash van at its stop");
  expectStart(fresh, "Roadmap_Cash_Van_Box", "cash van");

  // Fuel pumps: nobody is fuelling, so the hum prop is silent.
  on(1, { verb: "stand", entityName: "Fuel_PumpHum" });
  fresh = step("walk to the fuel pumps (nothing fuelling)");
  if (has(fresh, "start", "Fuel_PumpHum")) fails.push("fuel: the pump hummed with nothing fuelling");

  // Pub terrace: the radio wins the group; off hands the music back to the building.
  on(1, { verb: "stand", entityName: "Pub_Terrace_Radio" });
  fresh = step("pub terrace radio");
  expectStart(fresh, "Pub_Terrace_Radio", "pub");
  if (has(fresh, "start", PUB_BUILDING)) fails.push("pub: the building started beside the radio (they share a group)");
  const off = on(1, { verb: "use", className: "LF_AmbientSwitchAction", entityName: "Pub_Terrace_Radio" }, 10000);
  fresh = step("switch the terrace radio off");
  if (off.status !== "ok") fails.push(`pub: switch off returned ${off.status} ${off.detail}`);
  if (!has(fresh, "stop", "Pub_Terrace_Radio", "condition")) fails.push("pub: the radio did not stop when switched off");
  expectStart(fresh, PUB_BUILDING, "pub (radio off)");
  on(1, { verb: "use", className: "LF_AmbientSwitchAction", entityName: "Pub_Terrace_Radio" }, 10000);
  fresh = step("switch the terrace radio on");
  expectStart(fresh, "Pub_Terrace_Radio", "pub (radio on again)");

  const errors = lines().filter((l) => /no voice|stays silent|no sound project/.test(l));
  for (const e of errors) fails.push(`client error: ${e}`);
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
  const out = join(ROOT, "tmp", "atmosphere-town-proof.txt");
  mkdirSync(join(ROOT, "tmp"), { recursive: true });
  writeFileSync(out, record.join("\n") + "\n");
  console.log(`record: ${out}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "atmosphere town proof: FAIL" : "atmosphere town proof: PASS (bank, hospital, payphone hum and stop, cash van, pub radio and building hand-over)");
  return fails.length ? 1 : 0;
}

export default {
  name: "atmosphere-town-proof",
  usage: "atmosphere-town-proof",
  run: () => runTownProof(),
};
