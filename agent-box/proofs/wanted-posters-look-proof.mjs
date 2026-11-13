import { ROB_TARGET, arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// Readability of the wanted board's face (docs/architecture/visual-boards.md, spike issue 7) on a
// running `up --profile full --clients 1` session: client 1 robs the fuel till so the board holds
// its poster, then a fixed film camera photographs the board straight on from 1.7, 4, 8, 15 and
// 25 m at noon and again at 22:00. Read the pictures; the proof fails only if the poster never
// reaches the board or a capture fails.
const BOARD = "Roadmap_Police_WantedBoard";
const BOARD_AT = { x: 235.4, y: 0.555, z: 242.9 };
const DISTANCES = [1.7, 4, 8, 15, 25];

async function runLookProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 160)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(5000);
  on(1, { verb: "stand", entityName: BOARD });
  const seen = clientSees(on, wait, 1, (s) => (/^1\//.test(s.posters ?? "") ? [] : [`posters=${s.posters}`]), { label: "the poster is up", budgetMs: 15000 });
  fails.push(...seen.fails);
  // Out of every picture, still inside the draw radius of 30 m for every distance but the last.
  on(1, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(35000);

  on(1, { verb: "camera", args: "hud off" });
  for (const hour of [12, 22]) {
    on(1, { verb: "time", hour });
    wait(4000);
    for (const d of DISTANCES) {
      on(1, { verb: "camera", args: "stop" });
      on(1, { verb: "camera", args: `static ${BOARD_AT.x} ${BOARD_AT.y + 0.6 + d * 0.05} ${BOARD_AT.z + d} ${BOARD_AT.x} ${BOARD_AT.y} ${BOARD_AT.z} 60 50` });
      wait(2500);
      const r = on(1, { verb: "capture", mode: "ui", name: `board_h${hour}_d${String(d).replace(".", "p")}` }, 25000);
      if (r.status !== "ok") fails.push(`capture at ${d} m, ${hour}:00: ${r.status} ${r.detail}`);
    }
  }
  on(1, { verb: "camera", args: "stop" });
  on(1, { verb: "camera", args: "hud on" });
  on(1, { verb: "time", hour: 12 });
  on(1, { verb: "resetcrime" }, 10000);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "wanted posters look proof: FAIL" : "wanted posters look proof: PASS (read the pictures in the client log's el-captures)");
  return fails.length ? 1 : 0;
}

export default {
  name: "wanted-posters-look-proof",
  usage: "wanted-posters-look-proof   (up --profile full --clients 1)",
  run: () => runLookProof(),
};
