import { mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// The five paper items (fine ticket, receipt, hospital bill, sentence slip, ID card) dropped one at a time,
// each in its own spot, on grass (south of the police desk) and on a hard surface (the fuel counter slab).
// A free camera crashes its client a few seconds after a shot, so each client takes one: client 2 shoots the
// grass, client 1 the hard surface. Needs `session up --machine local --clients 2 --profile full`.
//   paper-ground-proof [out dir] [row|pair|hand]
//   row   a vanilla paper folder and all five dropped apart, one picture per surface (hand shot first)
//   pair  one sheet (the fine ticket) beside a vanilla paper folder, a close-up per surface
//   hand  the receipt held, seen by its owner (no free camera)
// Demo: node agent-box/session.mjs up --machine local --clients 2 --profile full, then paper-ground-proof.
const ITEMS = [
  ["FineTicket", "{3D6B0E8A41C7F259}Prefabs/Items/Roleplay/FineTicket.et"],
  ["Receipt", "{3E8A5C1D7B2F4096}Prefabs/Items/Roleplay/LF_Receipt.et"],
  ["HospitalBill", "{75055A5BFE4372D5}Prefabs/Items/Roleplay/HospitalBill.et"],
  ["SentenceSlip", "{4C1A9E7B52D83F60}Prefabs/Items/Roleplay/SentenceSlip.et"],
  ["IDCard", "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et"],
];
const VANILLA = "{5CF603986376EAA9}Prefabs/Items/Misc/IntelligenceFolder_E_01/IntelligenceFolder_E_01_base.et";
const SURFACES = [
  ["grass", "Roadmap_Police_Desk", 2],
  ["hard", "Roadmap_Fuel_Counter", 1],
];
const SIDES = ["s2", "e", "w", "n", "e2"];

async function run(ctx) {
  const out = resolve(String(ctx?.args?.[0] ?? "tmp/paper-ground"));
  const mode = String(ctx?.args?.[1] ?? "row");
  mkdirSync(out, { recursive: true });
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const vec = (text) => (String(text).match(/<([-\d.e]+),\s*([-\d.e]+),\s*([-\d.e]+)>/) || []).slice(1).map(Number);
  const drop = (prefab, side, entity) => {
    on(1, { verb: "stand", entityName: entity, side }, 10000);
    wait(1800);
    const g = on(1, { verb: "itemgive", prefab }, 10000);
    if (g.status !== "ok") fails.push(`give ${prefab}: ${g.detail}`);
    wait(900);
    const d = on(1, { verb: "itemdrop", prefab }, 10000);
    wait(1200);
    const at = vec(d.detail);
    if (at.length !== 3) fails.push(`drop ${prefab}: no position`);
    return at;
  };
  const mean = (pts) => [0, 1, 2].map((k) => pts.reduce((s, p) => s + p[k], 0) / pts.length);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  if (mode === "hand") {
    on(1, { verb: "stand", entityName: SURFACES[0][1], side: "s" }, 10000);
    wait(1800);
    on(1, { verb: "itemgive", prefab: ITEMS[1][1] }, 10000);
    wait(1000);
    on(1, { verb: "hold", item: "LF_Receipt" }, 10000);
    wait(3500);
    on(1, { verb: "capture", mode: "ui", name: "paper_hand_own" }, 25000);
  } else {
    // Drop everything first (the officer is the only body near), then the shots: a free camera ends the run.
    on(1, { verb: "stand", entityName: SURFACES[0][1], side: "s" }, 10000);
    wait(1500);
    on(1, { verb: "itemgive", prefab: ITEMS[1][1] }, 10000);
    wait(800);
    on(1, { verb: "hold", item: "LF_Receipt" }, 10000);
    wait(3500);
    on(1, { verb: "capture", mode: "ui", name: "paper_hand_own" }, 25000);
    const dropped = [];
    for (const [surface, entity, shooter] of SURFACES) {
      if (mode === "pair") {
        const folder = drop(VANILLA, "s", entity);
        const sheet = drop(ITEMS[0][1], "s2", entity);
        dropped.push({ surface, entity, shooter, pts: [folder, sheet] });
      } else {
        const folder = drop(VANILLA, "s", entity);
        dropped.push({ surface, entity, shooter, pts: [folder, ...ITEMS.map(([, prefab], i) => drop(prefab, SIDES[i], entity))] });
      }
    }
    for (const d of dropped) {
      if (d.pts.some((p) => p.length !== 3)) continue;
      const c = mean(d.pts);
      on(1, { verb: "stand", entityName: d.entity, side: "n2" }, 10000);
      wait(2500);
      const cam = mode === "pair" ? { ox: 0.15, oy: -0.7, oz: 0.5 } : { ox: 0.2, oy: -1.3, oz: 1.5 };
      on(d.shooter, { verb: "capture", mode: "world", name: `paper_${d.surface}_${mode}`, x: c[0], y: c[1], z: c[2], ox: cam.ox, oy: cam.oy, oz: cam.oz, hour: 12, free: true }, 25000);
      wait(1500);
    }
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "paper-ground proof: FAIL" : `paper-ground proof: done (read every paper_* picture in ${out})`);
  return fails.length ? 1 : 0;
}

export default { name: "paper-ground-proof", usage: "paper-ground-proof [out dir] [row|pair|hand]", run: (ctx) => run(ctx) };
