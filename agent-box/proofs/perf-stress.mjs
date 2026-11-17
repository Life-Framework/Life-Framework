import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, readSession, sessionDir } from "../session.mjs";

// The building performance measurement (docs/architecture/performance.md "Buildings"). One session, one
// client, measuring levers on:
//   node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped --mission everon
//   node agent-box/session.mjs perf-stress everon
//   node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped --mission playtest
//   node agent-box/session.mjs perf-stress playtest --shop <entity name> [shop|lights|shadows|points|props|server ...]
// Each row: settle 5 s, the client's `perf 10` window (frame time from World.GetTimeSlice), the client and
// server process memory (Get-Process), and the server's -logStats lines from the same window.

const SQUARE = { x: 4450, z: 10780 }; // Saint-Philippe square (docs/everon/world-bible.md)
const STRESS = { x: 230, z: 420 }; // open ground north of the playtest town
const SLOG = /FPS: ([\d.]+), frame time \(avg: ([\d.]+) ms, min: ([\d.]+) ms, max: ([\d.]+) ms.*?Mem: (\d+) kB/;

function newestLog(dir) {
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => d.startsWith("logs_")).sort();
  const f = runs.length ? join(dir, runs[runs.length - 1], "console.log") : "";
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

function procMB(pid) {
  if (!pid) return null;
  const r = spawnSync("powershell", ["-NoProfile", "-Command", `$p=Get-Process -Id ${pid} -ErrorAction SilentlyContinue; if ($p) { "$($p.PrivateMemorySize64) $($p.WorkingSet64)" }`], { encoding: "utf8", windowsHide: true });
  const [priv, ws] = String(r.stdout ?? "").trim().split(/\s+/).map(Number);
  return Number.isFinite(priv) ? { privMB: Math.round(priv / 1048576), wsMB: Math.round(ws / 1048576) } : null;
}

const kv = (detail) => Object.fromEntries(String(detail).split(/\s+/).map((w) => w.split("=")).filter((p) => p.length === 2));

async function runPerfStress(argv) {
  let args = [...argv];
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const s = readSession(dir) ?? {};
  const clientPid = (s.clients ?? [])[0]?.pid;
  // --shop <layer entity name>: the building to count and look into (a fitted shop in the playtest town).
  // --paired: an empty-scene reference right before each row, so drift between rows cancels out.
  const paired = args.includes("--paired");
  args = args.filter((a) => a !== "--paired");
  const si = args.indexOf("--shop");
  const shop = si === -1 ? "" : String(args[si + 1] ?? "");
  const rest = si === -1 ? args : [...args.slice(0, si), ...args.slice(si + 2)];
  const where = rest[0] ?? "playtest";
  const phases = rest.length > 1 ? rest.slice(1) : ["shop", "lights", "shadows", "points", "props", "server"];
  const on = (spec, timeout = 10000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    if (r.status !== "ok") console.log(`  ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const ground = (x, z) => Number(kv(on({ verb: "probe", mode: "ground", x, z }).detail).y ?? 0);
  const ps = (...words) => on({ verb: "perfstress", words: words.map(String) }, 30000);
  const cam = (from, to) => on({ verb: "camera", args: `static ${from.join(" ")} ${to.join(" ")} 60 60` });
  const rows = [];
  const measure = (label, extra = {}) => {
    wait(5000);
    const before = newestLog(join(dir, "server", "logs")).length;
    const r = on({ verb: "perf", seconds: 10, label }, 30000);
    const p = kv(r.detail);
    const slog = newestLog(join(dir, "server", "logs")).slice(before).split("\n").map((l) => SLOG.exec(l)).filter(Boolean);
    const avg = (i) => (slog.length ? slog.reduce((a, m) => a + Number(m[i]), 0) / slog.length : NaN);
    const row = {
      label,
      ...extra,
      meanMs: Number(p.meanMs), p95Ms: Number(p.p95Ms), p99Ms: Number(p.p99Ms), fps: Number(p.fps),
      client: procMB(clientPid), server: procMB(s.serverPid),
      serverFps: avg(1), serverAvgMs: avg(2), serverMaxMs: slog.length ? Math.max(...slog.map((m) => Number(m[4]))) : NaN, serverMemMB: Math.round(avg(5) / 1024),
    };
    rows.push(row);
    console.log(`${label.padEnd(26)} client ${row.meanMs} ms mean, ${row.p95Ms} p95, ${row.fps} fps, ${row.client?.privMB ?? "?"} MB | server ${row.serverAvgMs.toFixed(2)} ms avg, ${row.serverMaxMs} max, ${row.serverMemMB} MB (${slog.length} lines)`);
    return row;
  };

  if (on({ verb: "state" }).detail === "no_live_session") {
    console.log("FAIL  no live session: node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped");
    return 1;
  }
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(8000);
  on({ verb: "camera", args: "hud off" });
  on({ verb: "time", hour: 22 }, 15000);
  wait(3000);

  if (where === "everon") {
    const y = ground(SQUARE.x, SQUARE.z);
    for (const r of [30, 60, 150]) console.log(`count square r=${r}: ${ps("count", SQUARE.x, y, SQUARE.z, r).detail}`);
    cam([SQUARE.x - 45, y + 2, SQUARE.z - 12], [SQUARE.x + 25, y + 1.5, SQUARE.z]);
    measure("everon_square_night");
    on({ verb: "time", hour: 12 }, 15000);
    wait(3000);
    cam([SQUARE.x - 45, y + 2, SQUARE.z - 12], [SQUARE.x + 25, y + 1.5, SQUARE.z]);
    measure("everon_square_noon");
  } else {
    if (phases.includes("shop") && shop) {
      // The store's interior runs along its local +z, 0..3 m in x (its furniture prefab's lights).
      const c = kv(ps("count", shop, 10).detail);
      console.log(`count ${shop} r=10: ${Object.entries(c).map(([k, v]) => `${k}=${v}`).join(" ")}`);
      const [ox, oy, oz] = String(c.at ?? "0,0,0").split(",").map(Number);
      const yaw = (Number(c.yaw ?? 0) * Math.PI) / 180;
      const local = (lx, ly, lz) => [ox + lx * Math.cos(yaw) + lz * Math.sin(yaw), oy + ly, oz - lx * Math.sin(yaw) + lz * Math.cos(yaw)].map((v) => Number(v.toFixed(2)));
      console.log(`count town r=200: ${ps("count", ox, oy, oz, 200).detail}`);
      cam(local(1.5, 1.7, 6.8), local(1.5, 1.2, -0.5));
      measure("shop_night");
      cam(local(1.5, 12, 30), local(1.5, 0, 0));
      measure("shop_street_night");
    }
    const y = ground(STRESS.x, STRESS.z);
    console.log(`count stress ground r=12 (want empty): ${ps("count", STRESS.x, y, STRESS.z, 12).detail}`);
    const view = () => cam([STRESS.x, y + 4, STRESS.z - 9], [STRESS.x, y + 0.5, STRESS.z]);
    const sweep = (type, list) => {
      ps("at", STRESS.x, y, STRESS.z, type);
      view();
      for (const [L, S, K] of list) {
        if (paired) {
          ps("spawn", 0, 0, 0);
          measure(`ref_before_${type}_L${L}_S${S}_K${K}`, { reference: true });
        }
        ps("spawn", L, S, K);
        measure(`${type}_L${L}_S${S}_K${K}`, { type, lights: L, shadows: S, props: K });
      }
      ps("clear");
    };
    if (phases.some((p) => ["lights", "shadows", "points", "props"].includes(p))) {
      ps("at", STRESS.x, y, STRESS.z, "s");
      view();
      ps("spawn", 0, 0, 0);
      measure("stress_empty", { lights: 0, shadows: 0, props: 0 });
    }
    if (phases.includes("lights")) sweep("s", [[2, 0, 0], [4, 0, 0], [8, 0, 0], [16, 0, 0], [32, 0, 0], [64, 0, 0], [128, 0, 0]]);
    if (phases.includes("shadows")) sweep("s", [[8, 1, 0], [8, 2, 0], [8, 4, 0], [8, 8, 0], [16, 16, 0]]);
    if (phases.includes("points")) sweep("p", [[8, 0, 0], [8, 1, 0], [8, 2, 0], [8, 4, 0], [8, 8, 0]]);
    if (phases.includes("props")) sweep("s", [[0, 0, 150], [0, 0, 300], [0, 0, 600], [0, 0, 1200], [0, 0, 2400], [6, 2, 150]]);
    if (phases.includes("shot")) {
      // The stress scene as the camera sees it: proof the lights and boxes are drawn in view.
      ps("at", STRESS.x, y, STRESS.z, "s");
      view();
      ps("spawn", 16, 4, 150);
      wait(4000);
      console.log(`shot: ${on({ verb: "capture", mode: "ui", name: "perfstress_L16_S4_K150" }, 25000).detail}`);
      ps("clear");
    }
    if (phases.includes("server")) {
      ps("at", STRESS.x, y, STRESS.z, "s");
      for (const K of [0, 600, 2400, 4800]) {
        ps("server", K);
        wait(3000);
        measure(`server_K${K}`, { serverProps: K });
      }
      ps("server", 0);
    }
  }

  on({ verb: "camera", args: "stop" });
  on({ verb: "camera", args: "hud on" });
  console.log("PERFSTRESS_JSON " + JSON.stringify({ where, at: new Date().toISOString(), rows }));
  const empty = rows.filter((r) => !Number.isFinite(r.meanMs) || !Number.isFinite(r.serverAvgMs));
  for (const r of empty) console.log(`FAIL  ${r.label}: no client or server numbers`);
  return rows.length && !empty.length ? 0 : 1;
}

export default {
  name: "perf-stress",
  usage: "perf-stress everon|playtest [--paired] [--shop <entity name>] [shop lights shadows points props server]   (up --clients 1 --profile full --levers uncapped,slog,suncapped)",
  run: (ctx) => runPerfStress(ctx?.args ?? []),
};
