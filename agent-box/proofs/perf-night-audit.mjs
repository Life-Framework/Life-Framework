import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, readSession, sessionDir } from "../session.mjs";

// The night-light and shelf-goods audit (docs/perf/night-audit-2026-11-27.md). One session, one or two clients:
//   node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped --mission everon
//   node agent-box/session.mjs perf-night-audit [square supermarket gunstore dock outpost] [--hours 12,22]
// Per spot and hour: stand there, settle 15 s, then three interleaved rounds of 5 s `perf` windows with all lights
// on, shadows off (`perfstress lights noshadow`) and lights off, plus `framegap` max and the entity, light and
// shadow-caster counts within 30 m. Server frame time comes from the -logStats lines of the same run (client 1
// only is measured; --clients 2 shows the server with both). Use --mission island.

const SPOTS = {
  square: { entity: "Fit_saint_philippe_board_Place", side: "s3" }, // the market board, on the square
  supermarket: { entity: "Fit_saint_philippe_supermarket_Place" },
  gunstore: { entity: "Fit_saint_philippe_gun_counter_Place" },
  dock: { entity: "Fit_saint_philippe_dock_dock_Place" },
  outpost: { entity: "Fit_outpost_north_camp_clear_box1" },
};
const SLOG = /FPS: ([\d.]+), frame time \(avg: ([\d.]+) ms, min: ([\d.]+) ms, max: ([\d.]+) ms.*?Mem: (\d+) kB/;

function newestLog(dir) {
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => d.startsWith("logs_")).sort();
  const f = runs.length ? join(dir, runs[runs.length - 1], "console.log") : "";
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

function procMB(pid) {
  if (!pid) return null;
  const r = spawnSync("powershell", ["-NoProfile", "-Command", `$p=Get-Process -Id ${pid} -ErrorAction SilentlyContinue; if ($p) { "$($p.PrivateMemorySize64)" }`], { encoding: "utf8", windowsHide: true });
  const priv = Number(String(r.stdout ?? "").trim());
  return Number.isFinite(priv) ? Math.round(priv / 1048576) : null;
}

const kv = (detail) => Object.fromEntries(String(detail).split(/\s+/).map((w) => w.split("=")).filter((p) => p.length === 2));

async function run(argv) {
  let args = [...argv];
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const s = readSession(dir) ?? {};
  const clientPid = (s.clients ?? [])[0]?.pid;
  const hi = args.indexOf("--hours");
  const hours = hi === -1 ? [12, 22] : String(args[hi + 1]).split(",").map(Number);
  if (hi !== -1) args = [...args.slice(0, hi), ...args.slice(hi + 2)];
  const names = args.length ? args : Object.keys(SPOTS);
  const on = (spec, timeout = 10000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    if (r.status !== "ok") console.log(`  ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  if (on({ verb: "state" }).detail === "no_live_session") {
    console.log("FAIL  no live session: node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped --mission everon");
    return 1;
  }
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(8000);
  on({ verb: "camera", args: "hud off" });
  const rows = [];
  for (const name of names) {
    const spot = SPOTS[name];
    if (!spot) { console.log(`FAIL  unknown spot ${name}`); return 1; }
    for (const hour of hours) {
      on({ verb: "time", hour }, 15000);
      on({ verb: "camera", args: "stop" });
      const r = on({ verb: "stand", entityName: spot.entity, side: spot.side }, 60000);
      if (r.status !== "ok") { console.log(`FAIL  stand ${spot.entity}`); continue; }
      on({ verb: "camera", args: "hud off" });
      wait(15000);
      const counts = kv(on({ verb: "perfstress", words: ["count", spot.entity, 30] .map(String) }, 30000).detail);
      on({ verb: "framegap" });
      const before = newestLog(join(dir, "server", "logs")).length;
      // Paired A/B inside one view: all lights, shadows off, lights off, interleaved 3 times (a busy box moves the
      // absolute fps; the difference between neighbouring windows is the lights' cost).
      const win = { on: [], noshadow: [], off: [] };
      for (let rep = 0; rep < 3; rep++) {
        for (const mode of ["on", "noshadow", "off"]) {
          on({ verb: "perfstress", words: ["lights", mode, "80"] });
          if (mode !== "on") wait(1500);
          const w = kv(on({ verb: "perf", seconds: 5, label: `${name}_${hour}_${mode}` }, 30000).detail);
          win[mode].push({ mean: Number(w.meanMs), p95: Number(w.p95Ms) });
        }
        on({ verb: "perfstress", words: ["lights", "on", "80"] });
      }
      const gap = kv(on({ verb: "framegap" }).detail).max;
      const slog = newestLog(join(dir, "server", "logs")).slice(before).split("\n").map((l) => SLOG.exec(l)).filter(Boolean);
      const avg = (i) => (slog.length ? slog.reduce((a, m) => a + Number(m[i]), 0) / slog.length : NaN);
      const med = (arr) => { const v = arr.map((x) => x.mean).filter(Number.isFinite).sort((a, b) => a - b); return v.length ? v[Math.floor(v.length / 2)] : NaN; };
      const best = (arr) => Math.min(...arr.map((x) => x.mean).filter(Number.isFinite));
      const row = {
        spot: name, hour, medOnMs: med(win.on), medNoShadowMs: med(win.noshadow), medOffMs: med(win.off),
        bestOnMs: best(win.on), bestNoShadowMs: best(win.noshadow), bestOffMs: best(win.off), gapMs: Number(gap), win,
        entities: Number(counts.entities), lights: Number(counts.lights), lightsOn: Number(counts.lightsOn), shadows: Number(counts.shadow),
        clientMB: procMB(clientPid), serverMB: procMB(s.serverPid), serverAvgMs: avg(2), serverMaxMs: slog.length ? Math.max(...slog.map((m) => Number(m[4]))) : NaN,
      };
      rows.push(row);
      const f = (ms) => `${ms.toFixed(1)} ms/${(1000 / ms).toFixed(0)} fps`;
      console.log(`${(name + "_" + hour).padEnd(18)} median on ${f(row.medOnMs)} | noshadow ${f(row.medNoShadowMs)} | off ${f(row.medOffMs)} | best on ${f(row.bestOnMs)} off ${f(row.bestOffMs)} | gap ${gap} | ents ${counts.entities} lights ${counts.lights} on ${counts.lightsOn} shadow ${counts.shadow} | server ${row.serverAvgMs.toFixed(2)} ms (${slog.length} lines)`);
    }
  }
  on({ verb: "camera", args: "stop" });
  on({ verb: "camera", args: "hud on" });
  console.log("NIGHTAUDIT_JSON " + JSON.stringify({ at: new Date().toISOString(), clients: (s.clients ?? []).length, rows }));
  return rows.length ? 0 : 1;
}

export default {
  name: "perf-night-audit",
  usage: "perf-night-audit [square supermarket gunstore dock outpost] [--hours 12,22]   (up --clients 1 --profile full --levers uncapped,slog,suncapped --mission everon)",
  run: (ctx) => run(ctx?.args ?? []),
};
