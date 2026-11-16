import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #1813: `teleport grid:<slug>` puts the player at that cell's spawn. Visits every claimed cell and the
// frame in one `--mission grid` session (node agent-box/session.mjs up --mission grid), then asks for an
// unknown slug and expects the player to stay put. Expected spots come from the claims and the yard
// plans in tools/ (cell-spawn.mjs), the same maths `grid place` wrote the layers with; the game finds
// them through LF_Places. Runnable as a demo.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function posOf(detail) {
  const m = /^<?\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*>?$/.exec(parseState(detail).pos ?? "");
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

async function targets() {
  const Z = await import("../../tools/lib/world-zones.mjs");
  const { spawnPoint, FRAME_SPAWN_AT } = await import("../../tools/grid/cell-spawn.mjs");
  const { isFrameClaim } = await import("../../tools/grid/lattice.mjs");
  const out = [{ slug: "frame", x: FRAME_SPAWN_AT[0], z: FRAME_SPAWN_AT[1] }];
  for (const c of Z.readCells(ROOT)) {
    if (c.error || isFrameClaim(c)) continue;
    const rect = Z.claimRect(c);
    if (!rect) continue;
    const p = spawnPoint(Z.yardPlan(rect));
    out.push({ slug: c.slug, x: p.x, z: p.z });
  }
  return out;
}

async function runGridTeleportProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const where = () => posOf(on(1, { verb: "state" }, 10000).detail);
  const fails = [];

  arrive(on, 1, wait);
  wait(8000);
  const list = await targets();
  for (const t of list) {
    const r = on(1, { verb: "teleport", words: [`grid:${t.slug}`] }, 10000);
    if (r.status !== "ok") {
      fails.push(`${t.slug}: the verb returned ${r.status} ${r.detail}`);
      continue;
    }
    wait(2500);
    const p = where();
    if (!p) fails.push(`${t.slug}: no position in state`);
    else if (Math.hypot(p.x - t.x, p.z - t.z) > 3) fails.push(`${t.slug}: player at ${p.x},${p.z}, expected within 3 m of ${t.x.toFixed(1)},${t.z.toFixed(1)}`);
  }

  // An unknown slug: the server warns and the player stays where the last cell put them.
  const before = where();
  on(1, { verb: "teleport", words: ["grid:no-such-cell"] }, 10000);
  wait(2500);
  const after = where();
  if (!before || !after) fails.push("no-such-cell: no position in state");
  else if (Math.hypot(before.x - after.x, before.z - after.z) > 1) fails.push(`no-such-cell moved the player from ${before.x},${before.z} to ${after.x},${after.z}`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "grid-teleport: FAIL" : `grid-teleport: PASS (${list.length} spawn(s) visited, an unknown slug moved nobody)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "grid-teleport",
  usage: "grid-teleport   (run in a --mission grid session)",
  run: () => runGridTeleportProof(),
};
