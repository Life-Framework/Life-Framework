// tools/buildings/general-store/dress.mjs - the general store's dressing: shelf stock that is not for sale,
// wall and counter props, floor clutter, ceiling lights, written into the pack furniture prefab.
//
//   node tools/buildings/general-store/dress.mjs           rewrite the generated children of the furniture prefab
//   node tools/buildings/general-store/dress.mjs --check   exit 1 when the prefab differs from what this gives
//   node tools/buildings/general-store/dress.mjs --count   print the entity count per kind
//
// Everything this writes is a child whose ID starts with "D0" (a hash of its seed, so a rerun writes the same
// file); the rest of the furniture prefab is never touched. What is generated:
//   - shelf stock: every shelf board the store's lines do not use (spec.layer_store.lines: fixture, pick,
//     boards) gets plain vanilla props (tins, bottles, bread, cartons). They are Props, not Items: nothing
//     here can be picked up, taken or sold. The sold lines keep their store slots.
//   - spec.dressing[]: hand-placed props { prefab: "Prefabs/...et", at, yaw, pitch, roll, scale, why }.
//   - spec.lights[]: ceiling lights { at, color, lv, radius, angle } as an inline LightEntity, the same
//     recipe as the building pipeline's lighting preset (tools/buildings/pipeline/lighting.mjs): the base
//     game's Lamp_LightEntity_base is off by day, a plain LightEntity is always on. Optional `lamp` puts a
//     vanilla fixture mesh at the same point. `type: "Point"` with `attenuation` is the base game's ambient fill
//     (LightCeiling_01_on's LightAmbient): a downlight's spot never reaches the ceiling, a point light does.
// Prefab paths are resolved to {GUID}path from the game's own resource database (pipeline/gamedb.mjs).
import { readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { readChildren, fixturesOf, toBuilding, slotsOf, FIXTURES, ITEMS } from "../pipeline/store-slots.mjs";
import { boardsOf, itemOf, itemKey } from "../pipeline/fixtures.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { prefabChildren, snapItems, specSupports } from "../pipeline/surface.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
const spec = JSON.parse(readFileSync(join(HERE, "spec.json"), "utf8"));
const FILE = join(ROOT, "addons", spec.addon, spec.prefab_dir, `${spec.vanilla.furniture}.et`);
const num = (n) => String(+(+n).toFixed(3));
const id = (seed) => "D0" + createHash("sha1").update(`general-store|${seed}`).digest("hex").slice(0, 14).toUpperCase();

// ---- shelf stock ----------------------------------------------------------------------------
const P = {
  tin1: "Prefabs/Props/Garbage/Generic/LitterInterior_01/Single/LitterInterior_USSR_01_tincan_V1.et",
  tin2: "Prefabs/Props/Garbage/Generic/LitterInterior_01/Single/LitterInterior_USSR_01_tincan_V2.et",
  smokes: "Prefabs/Props/Garbage/Generic/LitterInterior_01/Single/LitterInterior_USSR_01_cigarettes_V1.et",
  vodka: "Prefabs/Props/Garbage/Generic/LitterInterior_01/Single/LitterInterior_01_vodka_V1.et",
  beerG: "Prefabs/Props/Civilian/BeerBottle_01_green.et",
  beerB: "Prefabs/Props/Civilian/BeerBottle_01_brown.et",
  beerS: "Prefabs/Props/Civilian/BeerBottle_01_slava.et",
  soda: "Prefabs/Props/Civilian/SodaBottle_01.et",
  bread: "Prefabs/Props/S2/S2_Bread/S2_Bread.et",
  box: (n) => `Prefabs/Props/Crates/CardboardBox/CardboardBox_0${n}.et`,
};
// Footprints (x along the shelf when unturned, z deep), measured with tools\cli measure.
const SIZE = { tin1: [0.1, 0.1], tin2: [0.1, 0.1], smokes: [0.085, 0.077], vodka: [0.07, 0.07], beerG: [0.082, 0.082], beerB: [0.082, 0.082], beerS: [0.082, 0.082], soda: [0.18, 0.137], bread: [0.37, 0.14] };

// A small deterministic generator, so the stock looks mixed but never changes between runs.
function rng(seed) {
  let s = parseInt(createHash("sha1").update(seed).digest("hex").slice(0, 8), 16) || 1;
  return () => ((s = (Math.imul(s, 1664525) + 1013904223) >>> 0) / 4294967296);
}

// One board's stock: items { key, x, z, y (lift), yaw } in the fixture's own space.
function stockBoard(board, seed, band) {
  const r = rng(seed);
  const out = [];
  const x0 = board.x[0] + 0.07;
  const x1 = board.x[1] - 0.07;
  const zF = board.z[1] - 0.07; // front row
  const zB = board.z[0] + 0.1; // back row
  const row = (keys, pitch, z, lift = 0, jitter = 0.012) => {
    for (let x = x0 + 0.05; x < x1 - 0.02; x += pitch) {
      const k = keys[Math.floor(r() * keys.length)];
      out.push({ key: k, x: x + (r() - 0.5) * jitter, z: z + (r() - 0.5) * jitter, y: lift, yaw: Math.round(r() * 360) });
    }
  };
  if (band === "low") {
    // Cartons and a bread crate on the floor board.
    const w = x1 - x0;
    const n = Math.max(2, Math.floor(w / 0.74));
    for (let i = 0; i < n; i++) out.push({ box: 1 + Math.floor(r() * 9), x: x0 + 0.34 + (w - 0.68) * (n > 1 ? i / (n - 1) : 0.5), z: (board.z[0] + board.z[1]) / 2, y: 0, yaw: r() < 0.5 ? 90 : -90 });
  } else if (band === "bread") {
    for (const [i, x] of [x0 + 0.22, x0 + 0.22 + 0.42, x0 + 0.22 + 0.84].entries()) {
      if (x > x1 - 0.18) break;
      out.push({ key: "bread", x, z: zF - 0.02, y: 0, yaw: 0 });
      out.push({ key: "bread", x: x + (r() - 0.5) * 0.04, z: zF - 0.02, y: 0.1, yaw: 4 * (i - 1) });
    }
    row(["tin1", "tin2"], 0.3, zB + 0.03, 0);
  } else if (band === "tins") {
    row(["tin1", "tin2"], 0.125, zF - 0.02);
    row(["tin2", "tin1"], 0.26, zB + 0.04);
  } else if (band === "bottles") {
    row(["beerG", "beerB", "beerS", "vodka"], 0.135, zF - 0.02);
    row(["soda", "beerG", "beerB"], 0.3, zB + 0.04);
  } else {
    // the high boards: mixed
    row(["vodka", "beerS", "tin2", "soda"], 0.2, zF - 0.02);
    row(["smokes", "smokes", "tin1"], 0.25, zB + 0.05);
  }
  return out;
}

// A single back row of small upright goods (for boards the store sells from).
function stockRow(board, seed, z) {
  const r = rng(seed);
  const out = [];
  const keys = ["tin1", "beerG", "tin2", "beerB", "vodka", "beerS"];
  for (let x = board.x[0] + 0.12; x < board.x[1] - 0.1; x += 0.14) out.push({ key: keys[Math.floor(r() * keys.length)], x: x + (r() - 0.5) * 0.01, z, y: 0, yaw: Math.round(r() * 360) });
  return out;
}

const BAND = (y, i, clear) => {
  if (y < 0.2) return clear !== null && clear < 0.46 ? "tins" : "low"; // cartons are 0.43 m tall
  if (y < 0.7) return i % 2 ? "bread" : "tins";
  if (y < 1.3) return i % 2 ? "bottles" : "tins";
  if (y < 1.8) return i % 2 ? "tins" : "bottles";
  return "high";
};

// Where the store's own goods stand on a board, in the fixture's space: { zMin } of the occupied band.
function soldBand(f, board, item, line) {
  const pts = slotsOf(f, board, item, { perBoard: line.per_board, rows: line.rows });
  if (!pts.length) return null;
  const r = (f.yaw * Math.PI) / 180;
  const yawed = Math.abs(Math.sin(r)) > 0.7;
  const across = yawed ? item.size[0] : item.size[2];
  const zs = pts.map((p) => {
    const dx = p[0] - f.at[0];
    const dz = p[2] - f.at[2];
    return dx * Math.sin(r) + dz * Math.cos(r); // the point's z in the fixture's space
  });
  return { zMin: Math.min(...zs) - across / 2 };
}

export function shelfStock(furnitureText, fixtures, items) {
  const ch = readChildren(furnitureText).filter((c) => !c.name.match(/^$/));
  const used = new Map(); // "Fixture#pick" -> Map(board index -> [lines])
  for (const l of spec.layer_store.lines) for (const p of l.pick) for (const b of l.boards) {
    const key = `${l.fixture}#${p}`;
    if (!used.has(key)) used.set(key, new Map());
    if (!used.get(key).has(b)) used.get(key).set(b, []);
    used.get(key).get(b).push(l);
  }
  const out = [];
  const want = spec.dress?.shelves ?? {};
  for (const [name, picks] of Object.entries(want)) {
    const list = fixturesOf(ch, name);
    const boards = fixtures[name].boards;
    for (const p of picks) {
      const f = list[p];
      if (!f) throw new Error(`dress.shelves: ${name} has ${list.length} instances, not pick ${p}`);
      boards.forEach((b, bi) => {
        if (b.y > 2.2 || b.x[1] - b.x[0] < 0.6) return; // the top cap
        const sold = used.get(`${name}#${p}`)?.get(bi);
        let stock;
        if (sold) {
          // a board the store sells from: one row of bottles and tins along the back, behind the goods' rows
          const zMin = Math.min(...sold.map((l) => soldBand(f, b, items[itemKey(l.item)], l)?.zMin ?? b.z[1]));
          const free = zMin - b.z[0] - 0.02;
          if (free < 0.09 || (b.clear !== null && b.clear < 0.3)) return;
          const z = b.z[0] + 0.02 + 0.045;
          stock = stockRow(b, `${name}|${p}|${bi}|back`, z);
        } else {
          stock = stockBoard(b, `${name}|${p}|${bi}`, BAND(b.y, bi + p, b.clear));
        }
        for (const it of stock) {
          const key = it.box ? P.box(it.box) : P[it.key];
          const lift = it.key === "tin1" || it.key === "tin2" ? 0 : 0;
          const at = toBuilding(f, [it.x, b.y + 0.005 + it.y + lift, it.z]);
          out.push({ seed: `shelf|${name}|${p}|${bi}|${out.length}`, prefab: key, at, yaw: +(f.yaw + it.yaw).toFixed(1), kind: "stock" });
        }
      });
    }
  }
  return out;
}

// ---- overstock on top of the wall units, price cards on the board edges -----------------------
// spec.dress.tops: { "<fixture>": [pick, ...] }: a unit that reaches 2.6 m has the cartons and crates of the
// next delivery on its top (the room has 1.1 m to the ceiling above them), as a village shop keeps them.
// Each unit gets one to three cartons from a seeded roll; plain props, never items.
const TOP = { y: 2.592 + 0.005, depth: [0.02, 0.47], width: { ShelfUniversal_01_wide: [-1.445, 0], ShelfUniversal_01_Base: [-1.025, 0] } };
export function shelfTops(furnitureText) {
  const ch = readChildren(furnitureText);
  const out = [];
  for (const [name, picks] of Object.entries(spec.dress?.tops ?? {})) {
    const list = fixturesOf(ch, name);
    const [x0, x1] = TOP.width[name] ?? [-1, 0];
    for (const p of picks) {
      const f = list[p];
      if (!f) throw new Error(`dress.tops: ${name} has ${list.length} instances, not pick ${p}`);
      const r = rng(`top|${name}|${p}`);
      const n = Math.max(1, Math.floor((x1 - x0) / 0.72));
      const w = (x1 - x0) / n;
      for (let i = 0; i < n; i++) {
        const cx = x0 + w * (i + 0.5) + (r() - 0.5) * 0.04;
        const box = 1 + Math.floor(r() * 9);
        const yaw = 90 + Math.round((r() - 0.5) * 10);
        out.push({ seed: `top|${name}|${p}|${i}`, prefab: P.box(box), at: toBuilding(f, [cx, TOP.y, 0.25 + (r() - 0.5) * 0.03]), yaw: +(f.yaw + yaw).toFixed(1), kind: "overstock" });
        if (r() < 0.45) out.push({ seed: `top|${name}|${p}|${i}|up`, prefab: P.box(1 + Math.floor(r() * 9)), at: toBuilding(f, [cx + (r() - 0.5) * 0.05, TOP.y + 0.43, 0.25 + (r() - 0.5) * 0.03]), yaw: +(f.yaw + yaw + Math.round((r() - 0.5) * 16)).toFixed(1), kind: "overstock" });
      }
    }
  }
  return out;
}

// spec.dress.cards: { fixture, picks, boards, prefab, scale, pitch }: a strip of small price cards on the front
// edge of each listed board, one every "step" m, turned the way the unit faces. Posters are flat and face +z in
// the unit's own space, so the card stands just off the board's front edge, a little under its top.
export function priceCards(furnitureText, fixtures) {
  const ch = readChildren(furnitureText);
  const out = [];
  for (const c of spec.dress?.cards ?? []) {
    const list = fixturesOf(ch, c.fixture);
    const boards = fixtures[c.fixture].boards;
    for (const p of c.picks) {
      const f = list[p];
      if (!f) throw new Error(`dress.cards: ${c.fixture} has ${list.length} instances, not pick ${p}`);
      for (const bi of c.boards) {
        const b = boards[bi];
        const r = rng(`card|${c.fixture}|${p}|${bi}`);
        let k = 0;
        for (let x = b.x[0] + 0.2; x < b.x[1] - 0.15; x += c.step ?? 0.3) {
          const v = c.variants[Math.floor(r() * c.variants.length)];
          out.push({ seed: `card|${c.fixture}|${p}|${bi}|${k++}`, prefab: v, at: toBuilding(f, [x, b.y - (c.drop ?? 0.03), b.z[1] + (c.out ?? 0.012)]), yaw: +f.yaw.toFixed(1), scale: c.scale ?? 0.2, kind: "price-card" });
        }
      }
    }
  }
  return out;
}

// ---- hand-placed dressing and lights --------------------------------------------------------
function dressing() {
  return (spec.dressing ?? []).map((d, i) => ({ seed: `dress|${i}|${d.prefab}|${d.at.join(",")}`, ...d, kind: "dressing" }));
}

function lights() {
  const out = [];
  for (const [i, l] of (spec.lights ?? []).entries()) {
    if (l.lamp) out.push({ seed: `lamp|${i}`, prefab: l.lamp, at: l.lamp_at ?? [l.at[0], l.at[1] + 0.2, l.at[2]], yaw: l.yaw ?? 0, kind: "lamp" });
    out.push({ seed: `light|${i}`, light: l, kind: "light" });
  }
  return out;
}

// ---- doors stay clear ---------------------------------------------------------------------------
// spec.doors.leaves lists the ground-floor doors (centre x,z; `wall` is the axis the wall runs along;
// `half` is half the opening). A prop whose centre stands inside the leaf pushed 1 m out each way (plus a
// 0.15 m margin for its own size) is a blocked door, as the placement audit counts it; a flat mat
// (`walkable`) is fine. Furniture-prefab children are not audited, so the generator checks them itself.
export function doorClashes(entities) {
  const bad = [];
  for (const d of spec.doors?.leaves ?? []) {
    const along = d.half + 0.15;
    const out = 1.15;
    const [hx, hz] = d.wall === "x" ? [along, out] : [out, along];
    for (const e of entities) {
      if (e.light || e.walkable || !e.at || e.at[1] > 2.4 || e.at[1] < -0.5) continue;
      if (Math.abs(e.at[0] - d.at[0]) <= hx && Math.abs(e.at[2] - d.at[1]) <= hz) bad.push(`${e.prefab.split("/").pop()} at ${e.at.join(",")} blocks the ${d.name}`);
    }
  }
  return bad;
}

// ---- writing --------------------------------------------------------------------------------
let DB;
function ref(path) {
  DB ??= gameDb();
  if (!DB) throw new Error("no base game install: cannot resolve prefab GUIDs");
  const g = DB.guidOf(path);
  if (!g) throw new Error(`not in the game's resource database: ${path}`);
  return `{${g}}${path}`;
}

function text(e) {
  if (e.light) {
    const l = e.light;
    return [
      "  LightEntity {",
      `   ID "${id(e.seed)}"`,
      `   coords ${l.at.map(num).join(" ")}`,
      `   angles ${l.angles ? l.angles.join(" ") : "-90 0 0"}`,
      `   Color ${l.color ?? "1 0.96 0.88 1"}`,
      `   LV ${l.lv ?? 6}`,
      "   ClipEV -10",
      // a Point light is the base game's default LightEntity type, which its own prefabs leave unwritten
      ...((l.type ?? "Spot") === "Spot" ? ["   Type Spot"] : []),
      `   Radius ${l.radius ?? 7}`,
      ...(l.attenuation !== undefined ? [`   DistanceAttenuation ${l.attenuation}`] : []),
      `   NearPlane ${l.near ?? 0.3}`,
      ...((l.type ?? "Spot") === "Spot" ? [`   SpotAngle ${l.angle ?? 140}`] : []),
      "   LensFlareType Disabled",
      "  }",
    ].join("\n");
  }
  const lines = [`  GenericEntity : "${ref(e.prefab)}" {`, `   ID "${id(e.seed)}"`, `   coords ${e.at.map(num).join(" ")}`];
  if (e.pitch || e.roll) lines.push(`   angles ${num(e.pitch ?? 0)} ${num(e.yaw ?? 0)} ${num(e.roll ?? 0)}`);
  else if (e.yaw) lines.push(`   angleY ${num(((((e.yaw + 180) % 360) + 360) % 360) - 180)}`);
  if (e.scale) lines.push(`   scale ${e.scale}`);
  lines.push("  }");
  return lines.join("\n");
}

export async function build() {
  const original = readFileSync(FILE, "utf8");
  // strip what an earlier run wrote
  const stripped = original.replace(/^ {2}\w+(?: : "[^"]*")? \{\n {3}ID "D0[0-9A-F]{14}"\n(?: {3,}[^\n]*\n)*? {2}\}\n/gm, "");
  const fixtures = { ...FIXTURES };
  for (const n of Object.keys(spec.dress?.shelves ?? {})) {
    if (!fixtures[n]) {
      const { measureOne } = await import("../../commands/measure.mjs");
      fixtures[n] = { boards: boardsOf((await measureOne(ROOT, n, { minSize: 0.05 })).surfaces) };
    }
  }
  const items = { ...ITEMS };
  const { measureOne } = await import("../../commands/measure.mjs");
  for (const l of spec.layer_store.lines) if (!items[itemKey(l.item)]) items[itemKey(l.item)] = itemOf((await measureOne(ROOT, l.item, { minSize: 0.05 })).bounds);
  const entities = [...shelfStock(stripped, fixtures, items), ...shelfTops(stripped), ...priceCards(stripped, fixtures), ...dressing(), ...lights()];
  // Every prop that stands on something is snapped onto the real top under it (surface.mjs): the typed heights floated a bread
  // loaf 10 cm and stacked cartons 2 cm. Hung kinds (cards, lamps, lights) stay where the spec puts them; a dressing row may name
  // `on` (an entity ID of the prefab or a spec.check_supports id) or set `free`.
  const rows = entities.filter((e) => e.prefab && e.at && !["price card", "lamp"].includes(e.kind) && !e.free).map((e) => ({ path: e.prefab, x: e.at[0], y: e.at[1], z: e.at[2], yaw: e.yaw ?? 0, scale: e.scale ?? 1, on: e.on, e }));
  const snap = await snapItems(rows, { fixed: [...prefabChildren(stripped), ...specSupports(spec)] });
  for (const r of rows) r.e.at[1] = r.y;
  if (process.argv.includes("--report")) for (const c of snap.changes) console.log(`  ${c.path.split("/").pop().replace(".et", "")} y ${c.from} -> ${c.to} (on ${c.on})`);
  const at = stripped.lastIndexOf("\n }\n}");
  if (at < 0) throw new Error("no child list in the furniture prefab");
  const body = entities.map(text).join("\n");
  return { out: `${stripped.slice(0, at + 1)}${body}\n${stripped.slice(at + 1)}`, entities, original, snap };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { out, entities, original, snap } = await build();
  console.log(snap.summary);
  const byKind = {};
  for (const e of entities) byKind[e.kind] = (byKind[e.kind] ?? 0) + 1;
  const summary = Object.entries(byKind).map(([k, n]) => `${n} ${k}`).join(", ");
  if (process.argv.includes("--count")) {
    console.log(summary);
  } else if (process.argv.includes("--check")) {
    console.log(out === original ? `SUMMARY general-store-dress ok ${summary}` : "SUMMARY general-store-dress fail: the furniture prefab differs; run node tools/buildings/general-store/dress.mjs");
    process.exit(out === original ? 0 : 1);
  } else {
    writeFileSync(FILE, out);
    console.log(`SUMMARY general-store-dress wrote ${summary}`);
  }
}
