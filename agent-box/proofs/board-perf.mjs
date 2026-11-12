import { arrive, clientBox, sessionDir } from "../session.mjs";

// MEASUREMENT (docs/architecture/visual-boards-budget.md) on a running
// `up --profile full --levers uncapped` session with one client: a fixed camera 4 m in front of
// the wanted board, then for each configuration the client replaces the board's posters with
// `stress <boards> <posters> <pictures>`, settles, and samples `perf` for the window.
const BOARD = { x: 235.4, y: 1.3, z: 242.9 };
const SETTLE_MS = Number(process.env.LF_PERF_SETTLE_MS ?? 5000);
const WINDOW_S = Number(process.env.LF_PERF_WINDOW_S ?? 10);
// The side of the board the posters face: +1 is world +Z for the Roadmap board (angleY 0; a wall behind -Z).
const SIDE = Number(process.env.LF_PERF_SIDE ?? 1);
// LF_PERF_PREFLIGHT=1: draw one board, photograph it from both sides, and stop.

const ROUND2 = [
  { label: "base", boards: 0, posters: 0, pictures: 0 },
  { label: "b1_p4", boards: 1, posters: 4, pictures: 1 },
  { label: "b1_p4_prefab", boards: 1, posters: 4, pictures: 2 },
  { label: "b1_p4_frozen", boards: 1, posters: 4, pictures: 3 },
  { label: "b8_p4", boards: 8, posters: 4, pictures: 1 },
  { label: "b8_p4_prefab", boards: 8, posters: 4, pictures: 2 },
  { label: "b8_p4_frozen", boards: 8, posters: 4, pictures: 3 },
  { label: "base_end", boards: 0, posters: 0, pictures: 0 },
];

const CONFIGS = [
  { label: "base", boards: 0, posters: 0, pictures: 0 },
  { label: "b1_p4_text", boards: 1, posters: 4, pictures: 0 },
  { label: "b1_p4", boards: 1, posters: 4, pictures: 1 },
  { label: "b1_p8", boards: 1, posters: 8, pictures: 1 },
  { label: "b4_p4", boards: 4, posters: 4, pictures: 1 },
  { label: "b8_p4", boards: 8, posters: 4, pictures: 1 },
  { label: "b8_p4_text", boards: 8, posters: 4, pictures: 0 },
  { label: "b4_p8", boards: 4, posters: 8, pictures: 1 },
  { label: "b8_p4_away", boards: 8, posters: 4, pictures: 1, away: true },
  { label: "base_end", boards: 0, posters: 0, pictures: 0 },
];

async function runBoardPerf() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(2000);
  on(1, { verb: "time", hour: 12 });
  on(1, { verb: "camera", args: "hud off" });
  const look = (away, side = SIDE) => {
    const eye = `${BOARD.x} ${BOARD.y + 0.3} ${BOARD.z + 4 * side}`;
    const at = away ? `${BOARD.x} ${BOARD.y} ${BOARD.z + 12 * side}` : `${BOARD.x} ${BOARD.y} ${BOARD.z}`;
    on(1, { verb: "camera", args: "stop" });
    on(1, { verb: "camera", args: `static ${eye} ${at} 900 55` });
  };
  if (process.env.LF_PERF_PREFLIGHT === "1") {
    on(1, { verb: "stress", boards: 8, posters: 4, pictures: 1 });
    for (const side of [-1, 1]) {
      look(false, side);
      wait(SETTLE_MS);
      on(1, { verb: "capture", mode: "ui", name: `board_perf_side${side < 0 ? "neg" : "pos"}` }, 25000);
    }
    return 0;
  }
  const rows = [];
  const { execFileSync } = await import("node:child_process");
  const { readFileSync } = await import("node:fs");
  const { join } = await import("node:path");
  const pid = JSON.parse(readFileSync(join(dir, "session.json"), "utf8")).clients.find((c) => c.n === 1).pid;
  const mem = () => {
    try {
      const out = execFileSync("powershell", ["-NoProfile", "-Command", `$p = Get-Process -Id ${pid}; "$([math]::Round($p.PrivateMemorySize64/1MB)) $([math]::Round($p.WorkingSet64/1MB))"`], { encoding: "utf8", windowsHide: true, timeout: 20000 }).trim().split(" ");
      return { privateMB: Number(out[0]), workingMB: Number(out[1]) };
    } catch {
      return { privateMB: -1, workingMB: -1 };
    }
  };
  const list = process.env.LF_PERF_ROUND === "2" ? ROUND2 : CONFIGS;
  for (const c of list) {
    look(!!c.away);
    on(1, { verb: "stress", boards: c.boards, posters: c.posters, pictures: c.pictures });
    wait(SETTLE_MS);
    const r = on(1, { verb: "perf", seconds: WINDOW_S, label: c.label }, (WINDOW_S + 20) * 1000);
    const m = mem();
    rows.push({ ...c, detail: `${r.detail} privateMB=${m.privateMB} workingMB=${m.workingMB}`, status: r.status });
    if (["b1_p4", "b8_p4", "b8_p4_away", "b8_p4_frozen", "b1_p4_frozen"].includes(c.label)) on(1, { verb: "capture", mode: "ui", name: `board_perf_${c.label}` }, 25000);
  }
  on(1, { verb: "stress", boards: 0, posters: 0, pictures: 0 });
  on(1, { verb: "camera", args: "stop" });
  on(1, { verb: "camera", args: "hud on" });
  console.log("\nboard-perf results:");
  for (const r of rows) console.log(`  ${r.label.padEnd(12)} ${r.status} ${r.detail}`);
  return rows.every((r) => r.status === "ok") ? 0 : 1;
}

export default {
  name: "board-perf",
  usage: "board-perf",
  run: () => runBoardPerf(),
};
