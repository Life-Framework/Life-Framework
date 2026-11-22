// agent-box/proofs/clutter-scan.mjs - `session.mjs clutter-scan [x z r] [--minutes N]`: what a player sees on the roads after the
// server has run its spawners (roadblocks, job props, a cash van's stop, garbage rounds), not only what the layers place.
// Client 1 runs the `clutterscan` harness verb (LF_ClutterScanVerb.c, the island audit's ClutterLines) now and every 5 minutes
// until --minutes have passed, and prints each clutter prop standing on a road in the open that is not road furniture
// (place-runtime.mjs ROAD_FURNITURE). Without x z r it scans 450 m round client 1's character. Exit 1 when any scan found one.
import { clientBox, sessionDir } from "../session.mjs";
import { ROAD_FURNITURE } from "../place-runtime.mjs";

const kv = (line) => Object.fromEntries(line.trim().split(/\s+/).map((w) => [w.slice(0, w.indexOf("=")), w.slice(w.indexOf("=") + 1)]).filter(([k]) => k));
const base = (p) => String(p || "").split("/").pop().replace(/\.et$/, "");

// The road props in a `clutterscan` answer that are not road furniture: [{ prefab, name, p }].
export function roadClutter(detail) {
  const out = [];
  for (const part of String(detail).split(" | ").slice(1)) {
    const v = kv(part.replace(/^clutter /, ""));
    if (v.roofed !== "0" || !/^RoadEntity/.test(v.on ?? "") || ROAD_FURNITURE.test(base(v.prefab))) continue;
    out.push({ prefab: base(v.prefab), name: v.name, p: v.p });
  }
  return out;
}

async function run(argv) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const mi = argv.indexOf("--minutes");
  const minutes = mi === -1 ? 0 : Number(argv[mi + 1]);
  const at = mi === -1 ? argv : [...argv.slice(0, mi), ...argv.slice(mi + 2)];
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const scan = () => {
    h.setBoxes([clientBox(dir, 1)]);
    return h.sendCommand({ verb: "clutterscan", words: at.map(String) }, 60000);
  };
  const seen = new Map();
  const t0 = Date.now();
  for (let i = 0; ; i++) {
    const r = scan();
    if (r.status !== "ok") {
      console.log(`clutter-scan: ${r.status} ${r.detail}`);
      return 1;
    }
    const found = roadClutter(r.detail);
    const props = /props=(\d+)/.exec(r.detail)?.[1];
    console.log(`clutter-scan t+${Math.round((Date.now() - t0) / 60000)} min: ${props} clutter props, ${found.length} on a road`);
    for (const f of found) {
      const key = `${f.prefab}@${f.p}`;
      if (!seen.has(key)) console.log(`  ROAD  ${f.prefab} (${f.name}) @${f.p}`);
      seen.set(key, f);
    }
    if ((Date.now() - t0) / 60000 >= minutes) break;
    // One scan every 5 minutes keeps the session lease alive (a harness command resets its idle clock).
    wait(Math.min(5 * 60000, Math.max(1000, minutes * 60000 - (Date.now() - t0))));
  }
  console.log(`SUMMARY clutter-scan ${seen.size ? "fail" : "ok"} onroad=${seen.size}`);
  return seen.size ? 1 : 0;
}

export function selftest(fail) {
  const d = "props=3 onroad=2 file=x | clutter prefab={A}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et name=GenericEntity p=1,2,3 on=RoadEntity roofed=0 | clutter prefab={B}Prefabs/Structures/Walls/Crashbarrier/CrashBarrier_01/CrashBarrier_01_4m_V1.et name=CrashBarrier_01_4m_V1 p=4,5,6 on=RoadEntity roofed=0";
  const r = roadClutter(d);
  if (r.length !== 1 || r[0].prefab !== "ConeTraffic_01_red") fail(`clutter-scan: expected the cone only, got ${JSON.stringify(r)}`);
}

export default {
  name: "clutter-scan",
  usage: "clutter-scan [x z r] [--minutes N]   (up --mission island|playtest --clients 2): road clutter a running client sees, now and every 5 min",
  run: (ctx) => run(ctx?.args ?? []),
};
