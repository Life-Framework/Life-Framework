import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4569: a told rumor is heard. In a `session up --mission grid --profile full` session (the pack's grid mission, so the
// Everon 1989 rumor rows are loaded through the mission header) the player is put at the gossip bench cell (slice-4462)
// and the `rumortell` verb tells the first gossip rumor on this client: the row's recorded clip plays through
// LF_RumorVoice.Speak (from the nearest other character, else the player's own head: a client cannot spawn a replicated
// NPC) and the caption toast shows. The proof fails unless the clip started (`clip=1`) and captures the screen.
// The server half (Deliver, the RPC, the map mark) is the `residents/rumor-overlay` test and the real bench.
const BENCH_CELL = "grid:slice-4462";

async function run() {
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
  wait(6000);
  const moved = on(1, { verb: "teleport", words: [BENCH_CELL] }, 10000);
  if (moved.status !== "ok") {
    console.log(`FAIL  teleport to the bench cell: ${moved.status} ${moved.detail}`);
    return 1;
  }

  wait(4000);
  const told = on(1, { verb: "rumortell", words: [] }, 15000);
  if (told.status !== "ok" || !/clip=1/.test(told.detail)) {
    console.log(`FAIL  rumortell: ${told.status} ${told.detail} (expected ok with clip=1)`);
    return 1;
  }

  wait(1500);
  const shot = on(1, { verb: "capture", mode: "ui", name: "rumor_told" }, 25000);
  console.log(`rumortell: PASS (${told.detail}; picture ${shot.status} ${shot.detail})`);
  return shot.status === "ok" ? 0 : 1;
}

export default {
  name: "rumortell",
  usage: "rumortell   (run in a --mission grid --profile full session)",
  run,
};
