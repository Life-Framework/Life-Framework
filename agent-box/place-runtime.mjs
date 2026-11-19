// agent-box/place-runtime.mjs - what spawners put in the world, judged like a placed prop (docs/architecture/placement-system.md).
//
// The island audit (`tools\cli audit placement --world MainWorld`) sends a `runtime <x> <z> <r>` line; after the world has run its
// spawners LF_PlacementAudit.RuntimeLines answers with one line per thing found in that circle that no layer row places:
//   car marker=<m> <measure> found=<car>        a parked-car marker's car: measured, then judged by place-audit's singleProblems
//   car marker=<m> nocar=1 p=x,y,z               a marker that put nothing out
//   vehicle <measure> found=<label>              any other vehicle standing there
//   light name=<l> p=x,y,z att=<m> to=<label>    a night light and the distance to the nearest surface on six axes
//   char name=<l> p=x,y,z gap=<m> on=<label>     a character and its feet above the surface under it
//   loose name=<l> prefab=<file> p=x,y,z         street clutter standing on its own
// This file turns the light, char and loose lines (and nocar) into ranked rows; the car and vehicle lines go through place-audit's
// own measured-prop path. Pure; `selftest()` runs in `tools\cli audit selftest`.

export const RUNTIME_LIMITS = {
  attach: 0.6, // a light this far from every surface hangs in the air: a window glow stands 0.35 m off its wall, a lamp head on its post
  charFloat: 0.15, // a character's feet above the floor under it
  charSunk: 0.3, // feet below it (an NPC in a floor)
  charOverlap: 0.45, // two characters closer than this on the ground plane, within 1 m of height: two spawners filled one spot
  looseOwned: 1.5, // a loose prop within this of a layer row of the same prefab is that row's
};

export const RUNTIME_WEIGHT = { "unattached-light": 9, "no-car": 6, "char-floating": 10, "char-sunk": 7, "char-overlap": 8, orphan: 6 };

export const RUNTIME_LAYER = {
  car: "(runtime: parked cars)",
  vehicle: "(runtime: vehicles)",
  light: "(runtime: night lights)",
  char: "(runtime: characters)",
  loose: "(runtime: loose props)",
};

const kv = (rest) => {
  const out = {};
  for (const tok of rest.trim().split(/\s+/)) {
    const eq = tok.indexOf("=");
    if (eq > 0) out[tok.slice(0, eq)] = tok.slice(eq + 1);
  }
  return out;
};
const xyz = (s) => (s ? s.split(",").map(Number) : [0, 0, 0]);
const r2 = (n) => Math.round(n * 100) / 100;
const base = (p) => String(p || "").split("/").pop().replace(/\.et$/, "");

// The kind of a runtime line, or null for any other out.txt line.
export function runtimeKind(line) {
  const m = /^(car|vehicle|light|char|loose) /.exec(line);
  return m ? m[1] : null;
}

// Rows ({ name, layer, x, y, z, actions, problems, allowed }) for the light, char, loose and nocar lines of out.txt.
// `records` are the layer rows that went in (they own a loose prop of their prefab standing where they stand).
export function runtimeRows(outText, records = []) {
  const rows = [];
  const chars = [];
  // A store's display copies (`shelf` lines, LF_PlacementAudit.StockRows) are owned by their store.
  const stock = [];
  for (const raw of outText.split(/\r?\n/)) if (raw.startsWith("shelf ")) stock.push(xyz(kv(raw).c));
  for (const raw of outText.split(/\r?\n/)) {
    const line = raw.trim();
    const kind = runtimeKind(line);
    if (!kind) continue;
    const v = kv(line.slice(kind.length + 1));
    const p = xyz(v.p);
    const row = (name, layer, problems) => rows.push({ name, layer, x: r2(p[0]), y: r2(p[1]), z: r2(p[2]), actions: [], problems, allowed: [] });
    if (kind === "car" && v.nocar === "1") {
      row(`Car_${v.marker}`, RUNTIME_LAYER.car, [{ kind: "no-car", amount: 1, text: "the parked-car marker put out no car" }]);
    } else if (kind === "light") {
      const att = Number(v.att);
      if (!(att < RUNTIME_LIMITS.attach)) row(v.name, RUNTIME_LAYER.light, [{ kind: "unattached-light", amount: Math.min(att, 2), text: `a light ${att >= 1.5 ? "1.5 m or more" : `${r2(att)} m`} from every surface (attached to nothing: no wall, pole or ceiling within ${RUNTIME_LIMITS.attach} m)` }]);
    } else if (kind === "char") {
      const gap = Number(v.gap);
      const c = { name: v.name, p, problems: [] };
      if (gap > RUNTIME_LIMITS.charFloat) c.problems.push({ kind: "char-floating", amount: gap, text: `stands ${r2(gap)} m in the air over ${v.on}` });
      if (gap < -RUNTIME_LIMITS.charSunk) c.problems.push({ kind: "char-sunk", amount: -gap, text: `feet ${r2(-gap)} m inside ${v.on}` });
      chars.push(c);
    } else if (kind === "loose") {
      const owned = stock.some((c) => Math.hypot(c[0] - p[0], c[2] - p[2]) <= 0.5) || records.some((e) => e.name === v.name || (base(e.prefab) === v.prefab && Math.hypot(e.x - p[0], e.z - p[2]) <= RUNTIME_LIMITS.looseOwned));
      if (!owned) row(v.name, RUNTIME_LAYER.loose, [{ kind: "orphan", amount: 1, text: `${v.prefab} stands here and no layer row or kit slot owns it (left by a script, a proof or the base map)` }]);
    }
  }
  for (let i = 0; i < chars.length; i++)
    for (let j = i + 1; j < chars.length; j++) {
      const a = chars[i], b = chars[j];
      const d = Math.hypot(a.p[0] - b.p[0], a.p[2] - b.p[2]);
      if (d < RUNTIME_LIMITS.charOverlap && Math.abs(a.p[1] - b.p[1]) < 1) {
        a.problems.push({ kind: "char-overlap", amount: 1, text: `inside ${b.name} (${r2(d)} m apart): two spawners filled one spot` });
        b.problems.push({ kind: "char-overlap", amount: 1, text: `inside ${a.name} (${r2(d)} m apart): two spawners filled one spot` });
      }
    }
  for (const c of chars) if (c.problems.length) rows.push({ name: c.name, layer: RUNTIME_LAYER.char, x: r2(c.p[0]), y: r2(c.p[1]), z: r2(c.p[2]), actions: [], problems: c.problems, allowed: [] });
  for (const r of rows) r.score = Math.round(r.problems.reduce((s, q) => s + (RUNTIME_WEIGHT[q.kind] ?? 1) * (1 + Math.min(q.amount ?? 0, 2)), 0) * 10) / 10;
  return rows;
}

// A car or vehicle line as place-audit's measured-prop entry: { tag, e } (the line's measure is parsed by place-audit's parseRow).
// A vehicle a layer row places (named like the row) is that row's, already measured: null.
export function vehicleEntry(line, records = []) {
  const kind = runtimeKind(line);
  if (kind !== "car" && kind !== "vehicle") return null;
  const rest = line.slice(kind.length + 1);
  const v = kv(rest);
  if (v.nocar === "1") return null;
  if (kind === "vehicle" && records.some((e) => e.name && e.name === v.found)) return null;
  const name = kind === "car" ? `Car_${v.marker}` : `Vehicle_${v.found}`;
  // The prefab text marks it a vehicle for place-audit's kinds (RX.vehicle: exempt from the contact check, its wheels are in its bounds).
  return { rest, e: { name, prefab: `Prefabs/Vehicles/${v.found}`, layer: RUNTIME_LAYER[kind], actions: [] } };
}

// red-proof: 2026-11-19 with the unattached-light and orphan rows switched off `audit selftest` printed "FAILED expected 6 runtime rows, got 4"; restored, OK.
export function selftest(fail) {
  const out = [
    "light name=Glow_ok p=0,2,0 att=0.35 to=House",
    "light name=Glow_air p=5,4,0 att=1.5 to=none",
    "char name=Barman p=10,0,0 gap=0 on=Floor",
    "char name=Drinker p=10.2,0,0.1 gap=0.02 on=Floor",
    "char name=Sleeper p=20,1.2,0 gap=0.7 on=Bed",
    "loose name=Cone_A prefab=Roadblock_Cone p=30,0,0",
    "loose name=Cone_B prefab=Roadblock_Cone p=40,0,0",
    "loose name=Fit_x_bench prefab=LF_Bench p=90,0,0",
    "shelf shop/LF_Crackers@0 gap=0 c=70.1,1.15,0 found=LF_Crackers",
    "loose name=LF_Crackers prefab=LF_Crackers p=70,1.1,0",
    "car marker=M1 nocar=1 p=50,0,0",
    "1 gap=0 found=x",
  ].join("\n");
  const rows = runtimeRows(out, [{ prefab: "Prefabs/Props/Roleplay/Roadblock/Roadblock_Cone.et", x: 30.5, z: 0 }, { name: "Fit_x_bench", prefab: "Other.et", x: 0, z: 0 }]);
  const by = (n) => rows.find((r) => r.name === n);
  if (by("Glow_ok")) fail("a light 0.35 m off its wall was flagged");
  if (!by("Glow_air")?.problems.some((p) => p.kind === "unattached-light")) fail("a light 1.5 m from everything was not flagged unattached");
  if (!by("Barman")?.problems.some((p) => p.kind === "char-overlap") || !by("Drinker")) fail("two characters 0.22 m apart were not flagged as one spot");
  if (!by("Sleeper")?.problems.some((p) => p.kind === "char-floating")) fail("a character 0.7 m over a bed was not flagged floating");
  if (by("Cone_A")) fail("a cone a layer row owns was flagged orphan");
  if (!by("Cone_B")?.problems.some((p) => p.kind === "orphan")) fail("a cone no row owns was not flagged orphan");
  if (!by("Car_M1")?.problems.some((p) => p.kind === "no-car")) fail("a marker with no car was not flagged");
  if (rows.length !== 6) fail(`expected 6 runtime rows, got ${rows.length}`);
  const v = vehicleEntry("car marker=M2 gap=1.2 c=1,2,3 found=S1203_Taxi");
  if (!v || v.e.layer !== RUNTIME_LAYER.car || !/^Prefabs\/Vehicles\//.test(v.e.prefab) || !v.rest.startsWith("marker=M2")) fail(`a car line is not a measured vehicle entry: ${JSON.stringify(v)}`);
  const fl = floatingRows([...rows, { name: "Bench", problems: [{ kind: "contact", amount: 0.25, text: "hovers 25 cm" }] }, { name: "Cup", problems: [{ kind: "contact", amount: 0.02, text: "hovers 2 cm" }] }, { name: "Plate", problems: [{ kind: "clips", amount: 1, text: "clips Wall" }] }]).map((r) => r.name);
  if (!fl.includes("Glow_air") || !fl.includes("Sleeper") || !fl.includes("Car_M1") || !fl.includes("Bench") || fl.includes("Cup") || fl.includes("Plate") || fl.includes("Cone_B")) fail(`floatingRows picked ${fl.join(",")}`);
  if (vehicleEntry("car marker=M1 nocar=1 p=1,2,3")) fail("a no-car line became a measured entry");
  if (vehicleEntry("vehicle gap=0 found=Fit_lot_van", [{ name: "Fit_lot_van" }])) fail("a layer row's vehicle was measured a second time");
  if (by("LF_Crackers")) fail("a store's display copy was flagged orphan");
  if (by("Fit_x_bench")) fail("a prop named like its layer row was flagged orphan");
}

// --floating (the gate for "nothing a player sees hangs in the air", placement-system.md): the rows that float, whatever their
// zone. A small prop's contact gap counts from 5 cm up (a bench 25 cm over the pavement), the rest by their own thresholds.
export const FLOAT_KINDS = new Set(["floating", "unattached-light", "char-floating", "no-car"]);
export function floatingRows(ranked) {
  return ranked.filter((r) => r.problems.some((p) => FLOAT_KINDS.has(p.kind) || (p.kind === "contact" && /^hovers/.test(p.text) && p.amount > 0.05)));
}
