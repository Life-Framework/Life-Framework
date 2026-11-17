import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// #2207: `townreach <from> <to>` paths a civilian on the world's navmesh (GM_Eden.nmn on the island) and fails on no path or
// a path over 3 times the straight line (LF_TownReachHarness.c). Run in a `--mission island` session
// (`tools\cli session up --mission island`).
//
//   town-reach                       the acceptance: a reachable marker (a junction of the place's road net) passes and a marker
//                                    inside a closed building fails. Markers come from the place's dossier; the verb gets x,y,z.
//   town-reach --place <slug>        the real proof `town ready` runs: from the arrival slot's entrance to every other mapped
//                                    slot's entrance, from the place's dossier and approved mapping.
//   town-reach --place <slug> --json   the same, one JSON line `TOWNREACH {...}` at the end for `town ready`.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r1 = (n) => Math.round(n * 10) / 10;
const spec = (p) => `${p.x},${p.y ?? 0},${p.z}`;
const dist = (a, b) => Math.hypot(a.x - b.x, a.z - b.z);

function readPlace(place) {
  const dir = join(ROOT, "docs", "everon", "places", place);
  const dossier = JSON.parse(readFileSync(join(dir, "dossier.json"), "utf8"));
  const mFile = join(dir, "mapping.json");
  return { dossier, mapping: existsSync(mFile) ? JSON.parse(readFileSync(mFile, "utf8")) : null };
}

// Pure: the acceptance's two markers. `reach` is the junction nearest 50..150 m from the first junction (open road); `closed` is
// the centre of the biggest building (a hole in the navmesh).
export function acceptanceMarkers(dossier) {
  const j = dossier.junctions ?? [];
  if (j.length < 2) throw new Error("the dossier has fewer than two junctions");
  const from = { x: j[0].x, z: j[0].z };
  const far = j.slice(1).map((p) => ({ x: p.x, z: p.z, d: dist(from, p) })).filter((p) => p.d >= 50 && p.d <= 150).sort((a, b) => a.d - b.d)[0];
  if (!far) throw new Error("no junction 50 to 150 m from the first one");
  const big = [...(dossier.buildings ?? [])].filter((b) => b.footprint?.area).sort((a, b) => b.footprint.area - a.footprint.area)[0];
  if (!big) throw new Error("the dossier has no building with a footprint");
  return { from, reach: far, closed: { x: big.footprint.cx ?? big.x, y: big.y, z: big.footprint.cz ?? big.z, building: big.stem ?? big.prefab } };
}

// Pure: [{ slot, from, to }] - the arrival slot's entrance to each other mapped slot's entrance (a plot slot uses the plot centre).
export function slotTargets(dossier, mapping) {
  const byId = new Map((dossier.buildings ?? []).map((b) => [b.id, b]));
  const plots = new Map((dossier.freePlots ?? []).map((p) => [p.id, p]));
  const pointOf = (entry) => {
    const b = entry.building ? byId.get(entry.building) : null;
    if (b) {
      const e = (b.entrances ?? []).find((x) => x.side === "front") ?? (b.entrances ?? [])[0];
      return e ? { x: e.x, y: e.y, z: e.z } : { x: b.x, y: b.y, z: b.z };
    }
    const p = entry.plot ? plots.get(entry.plot) : null;
    return p ? { x: p.x, y: p.y ?? 0, z: p.z } : null;
  };
  const slots = Object.entries(mapping?.slots ?? {});
  const arrival = slots.find(([id]) => id === "arrival");
  const from = arrival ? pointOf(arrival[1]) : null;
  if (!from) throw new Error("the mapping has no arrival slot with an entrance");
  return slots.filter(([id]) => id !== "arrival").map(([id, e]) => ({ slot: id, from, to: pointOf(e) })).filter((t) => t.to);
}

async function run(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, s, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(s, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const pi = args.indexOf("--place");
  const place = pi >= 0 ? args[pi + 1] : null;
  const asJson = args.includes("--json");
  const reach = (from, to) => on(1, { verb: "townreach", words: [spec(from), spec(to)] }, 40000);

  arrive(on, 1, wait);
  wait(8000);
  const fails = [];
  const results = [];
  if (place) {
    const { dossier, mapping } = readPlace(place);
    const targets = slotTargets(dossier, mapping);
    if (!targets.length) fails.push(`${place}: no mapped slot besides arrival to walk to`);
    for (const t of targets) {
      const r = reach(t.from, t.to);
      results.push({ slot: t.slot, ok: r.status === "ok", detail: r.detail });
      if (r.status !== "ok") fails.push(`${place}.${t.slot}: ${r.detail}`);
    }
  } else {
    const m = acceptanceMarkers(readPlace("saint-philippe").dossier);
    const ok = reach(m.from, m.reach);
    results.push({ slot: "reachable-marker", ok: ok.status === "ok", detail: ok.detail });
    if (ok.status !== "ok") fails.push(`a reachable marker ${r1(dist(m.from, m.reach))} m away did not pass: ${ok.status} ${ok.detail}`);
    const bad = reach(m.from, m.closed);
    results.push({ slot: "closed-building-marker", ok: bad.status !== "ok", detail: bad.detail });
    if (bad.status === "ok") fails.push(`a marker inside the closed building ${m.closed.building} passed: ${bad.detail}`);
    else if (!/noreach|navmesh/.test(bad.detail)) fails.push(`the closed-building marker failed for the wrong reason: ${bad.detail}`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  if (asJson) console.log(`TOWNREACH ${JSON.stringify({ place, pass: !fails.length, results })}`);
  console.log(fails.length ? "town-reach: FAIL" : `town-reach: PASS (${results.length} walk(s))`);
  return fails.length ? 1 : 0;
}

export default {
  name: "town-reach",
  usage: "town-reach [--place <slug>] [--json]   (run in a --mission island session)",
  run: (ctx) => run(ctx?.args ?? process.argv.slice(2)),
};
