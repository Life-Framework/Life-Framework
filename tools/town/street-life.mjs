// tools/town/street-life.mjs - `tools\cli town street-life <place> [--dry] [--no-audit]`: fit the `street-life` outposts slot (#3360):
// benches, street bins, flower boxes and washing lines on the residential houses the slot's mapping names.
//
// Reads  docs/everon/places/<place>/mapping.json (slot `street-life`: `drops: [{ place, building }]`) and each drop place's dossier.json.
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_street-life.layer, tools/town/claims/<place>-street-life.json.
//
// No coordinate is typed. A prop on a wall (bench, bin, flower box) is one kit row a metre outside the footprint on the entrance side,
// which the same offline seat `town drops` uses slides onto the host's real wall, back to it. A washing line stands free in the yard
// behind the house, along the back wall. A spot passes when it keeps clear of every door (1.4 m), of the carriageway of every road
// and of the props already placed on that house; the first sideways offset along the wall that passes wins.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CLAIMS_DIR, LAYER_DIR, claimNameOf, layerNameOf } from "./check.mjs";
import { dropKit, DROP_KITS } from "./drops.mjs";
import { planFitSeated } from "./fit.mjs";
import { checkMapping, loadTemplate } from "./map.mjs";
import { distToLine, ROAD_HALF } from "./parked-cars.mjs";
import { shellReader } from "./seat.mjs";
import { pinFileOf, pinKey, readPins, writePins } from "./terrain.mjs";

export const help = "street-life <place> [--dry] [--no-audit] [--terrain]   (--terrain probes the ground under the benches and keeps it in tools/town/terrain/<place>-street-life.json) the street-life outposts slot: benches, bins, flower boxes and washing lines on the residential houses the mapping names; writes the layer and the claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
export const SLOT = "street-life";
const r3 = (n) => Math.round(n * 1000) / 1000;

// The props: a vanilla street prop (or the bench's `LF_Seat_*` wrapper that makes it sittable). `wall`: stands against the house wall;
// `gap`: the least distance to anything else already on the house; `lift`: metres over the dossier's floor height.
// A kit names a source file in the repo, and a vanilla prop must appear in it or its `also` list: the clinic furniture prefab holds the bin and the flower box, the village dressing layer the clothesline.
const VANILLA_SOURCE = "addons/LF_Everon1989/Prefabs/Buildings/Clinic/Furniture/Clinic_furniture_01.et";
const VANILLA_ALSO = ["addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer"];
export const PROPS = {
  bench: { prefab: "{BE839ACB6CB2726A}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et", source: "addons/LifeFramework/Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et", wall: true, gap: 1.6, ground: true, optional: true },
  "bench-red": { prefab: "{24388E37E11BA18F}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_red.et", source: "addons/LifeFramework/Prefabs/Props/Seats/LF_Seat_BenchStreet_01_red.et", wall: true, gap: 1.6, ground: true, optional: true },
  bin: { prefab: "{32CDE9FE3FB35D51}Prefabs/Props/Garbage/Bins/BinStreet_01.et", source: VANILLA_SOURCE, wall: true, gap: 0.9, cls: "SCR_DestructibleEntity" },
  flowers: { prefab: "{06ABA7878E8CC278}Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et", source: VANILLA_SOURCE, wall: true, gap: 1.2, cls: "SCR_DestructibleEntity", ground: true, optional: true, half: 0.5, out: 0.15 },
  line: { prefab: "{801E1CA69F368C44}Prefabs/Structures/Civilian/ClotheslineFrame_01.et", source: VANILLA_SOURCE, wall: false, gap: 2.5, rear: 3.2, doors: true, half: 1.2 },
};

// One house, its props in order. A bench and a bin sit at the door, flower boxes under the front windows, a washing line behind.
export const PLAN = {
  b10: ["bench", "bin"],
  b09: ["bench-red", "bin", "flowers"],
  b16: ["bench", "flowers"],
  b17: ["bench", "bin"],
  b50: ["flowers", "line"],
  b47: ["bin", "flowers", "line"],
};

const SIDEWAYS = [null, 0.5, -0.5, 0.75, -0.75, 0.3, -0.3, 0.9, -0.9, 0.15, -0.15];
const LINE_SIDEWAYS = [0, 0.5, -0.5, 1, -1, 1.5, -1.5, 2, -2]; // a washing line also slides along the back wall to clear a door's swing (b50 stood 0.12 m in one, #3888)
const DOOR_CLEAR = 1.4;
// A bench stands on the ground the server probed (one vertical ray from just above its seat height at five points of its
// footprint: the middle, each end of the seat, front and back), not on the dossier's floor height of the house. It is refused
// where the ground under it spans more than SLOPE_MAX (a slope or a step under one leg), or where it lies STEP_MAX from the
// house floor (the stairs of the entrance), and the next sideways offset along the wall is tried.
const BENCH_HALF = 0.8; // metres from the middle to each end of the seat
const BENCH_DEPTH = 0.12; // metres from the middle to the front and back edge
// What a prop must not stand on: the probe's ray lands on a pile, a log stack or a crate beside the house, not on the ground (#3880:
// the b17 bench stood on WoodPile_02_B). The hit name of each probed point is kept in the pin file beside its height.
export const HIT_BAD = /pile|log|stack|crate|pallet|barrel|firewood|timber|plank|cart|truck|car_|vehicle/i;
export const hitBlocks = (hits) => hits.map((h) => String(h ?? "")).find((h) => HIT_BAD.test(h)) ?? null;
const SLOPE_MAX = 0.13;
const STEP_MAX = 0.2;
const RAY_UP = 0.8;
const RAY_DOWN = 2.5;
const DOOR_NEAR = 6; // metres: the doors kept in the pin file around a probed point
const DOOR_SWING = 1.8; // metres from a door leaf's middle: a bench point nearer than this stands in its arc

// The five points of a bench's footprint, rounded as the pin file keys them. yaw: the row's angleY in degrees.
export function benchPoints(x, z, yaw, half = BENCH_HALF) {
  const a = (yaw * Math.PI) / 180;
  const f = [Math.sin(a), Math.cos(a)];
  const s = [Math.cos(a), -Math.sin(a)];
  const at = (u, v) => [r3(x + s[0] * u + f[0] * v), r3(z + s[1] * u + f[1] * v)];
  return [at(0, 0), at(half, 0), at(-half, 0), at(0, BENCH_DEPTH), at(0, -BENCH_DEPTH)];
}

// Pure: the ground heights under a bench. { y } when it can stand there, { why } when not.
export function benchGround(heights, seatY) {
  const top = Math.max(...heights);
  const low = Math.min(...heights);
  if (top - low > SLOPE_MAX) return { why: `the ground under it spans ${r3(top - low)} m` };
  if (Math.abs(top - seatY) > STEP_MAX) return { why: `the ground is ${r3(top - seatY)} m from the house floor (steps)` };
  return { y: r3(top) };
}
const ROAD_CLEAR = ROAD_HALF + 0.4;
// The game's own prefab text, for the vanilla props' sources and inherited component GUIDs; null when no game install is readable.
async function gameReader() {
  try {
    const vfs = await (await import("../lib/game-prefab.mjs")).gameVfs();
    return (rel) => (vfs.exists(rel) ? vfs.readTextFile(rel) : null);
  } catch {
    return null;
  }
}
const readJson = (f, what) => {
  if (!existsSync(f)) throw new Error(`no ${what}: ${f} does not exist`);
  return JSON.parse(readFileSync(f, "utf8"));
};

// Pure: why a placed spot is refused, or null. `others`: [{ x, z, gap }] already on this house.
export function refuse(x, z, gap, building, roads, others) {
  for (const e of building.entrances ?? []) if (Math.hypot(e.x - x, e.z - z) < DOOR_CLEAR) return "in a doorway";
  for (const r of roads) if ((r.points ?? []).length > 1 && distToLine(x, z, r.points) < ROAD_CLEAR) return `on the carriageway of ${r.id}`;
  for (const o of others) if (Math.hypot(o.x - x, o.z - z) < Math.max(gap, o.gap)) return "too close to another prop";
  return null;
}

// One prop's plan on one house: the first sideways offset that seats on the wall and passes `refuse`.
// `pins`: { points: { "x,z": ground y } } for the ground props; a point it lacks goes into `need` ([x, z, rayFromY]) and the prop is
// reported pending, so one probe boot answers every candidate of every bench.
async function planProp({ kind, building, dossier, root, shell, readGame, others, pins = null, need = null }) {
  const prop = PROPS[kind];
  DROP_KITS[`street-${kind}`] = { prefab: prop.prefab, source: prop.source };
  const template = { schema: 1, kind: "drops", slots: [{ id: SLOT, required: false, kit: `street-${kind}`, host: "building", needs: { interior: "optional" }, places: [] }] };
  const mapping = { schema: 1, template: template.kind, place: dossier.place.slug, slots: { [SLOT]: { building: building.id } } };
  // A bench tries the entrance wall first, then the two side walls and the back one, when the ground at the door is a slope or stairs.
  const turns = prop.ground ? [0, 1, 3, 2] : [prop.wall ? 0 : 2];
  const cands = turns.flatMap((t) => (prop.wall ? SIDEWAYS : prop.doors ? LINE_SIDEWAYS : [0, 0.5, -0.5]).map((s) => [t, s]));
  const refusals = {};
  for (const [turn, sideways] of cands) {
    const { id, kit } = dropKit(building, SLOT, sideways, 0, turn, `street-${kind}`);
    kit.source.also = VANILLA_ALSO;
    const row = kit.rows[0];
    if (!prop.wall) {
      // A washing line: further out into the yard, on the ground, its length along the back wall.
      const [ox, oy, oz] = row.offset;
      const k = (Math.hypot(ox, oz) + prop.rear) / Math.hypot(ox, oz);
      row.offset = [r3(ox * k), oy, r3(oz * k)];
      row.verb = "on floor";
      row.tags = [];
      delete row.wall;
    }
    kit.anchoring.footprint = { source: "rows", min: [row.offset[0], row.offset[2]], max: [row.offset[0], row.offset[2]] };
    const opts = { place: dossier.place.slug, slot: SLOT, dossier, mapping, template, kits: { [kit.kit]: kit }, skipClaims: true, fetch: false };
    const plan = await planFitSeated(opts, { root, readGame, shell });
    if (!plan.ok) {
      refusals[plan.errors?.[0] ?? "refused"] = 1;
      continue;
    }
    const e = plan.ents.rowEnts[0];
    if (!e) {
      refusals["left out"] = 1;
      continue;
    }
    if (prop.wall && !plan.seat?.rows?.[0]?.wall) {
      refusals["no wall to sit on"] = 1;
      continue;
    }
    const why = refuse(e.coords[0], e.coords[2], prop.gap, building, dossier.roads ?? [], others);
    if (why) {
      refusals[why] = 1;
      continue;
    }
    if (prop.wall) {
      const a = (e.yaw * Math.PI) / 180;
      const out = prop.out ?? 0.05; // a long planter needs more than the bench's 5 cm: b16's stood through the house wall (#3888)
      e.coords = [r3(e.coords[0] + Math.sin(a) * out), e.coords[1], r3(e.coords[2] + Math.cos(a) * out)];
      if (prop.ground && need) {
        const pts = benchPoints(e.coords[0], e.coords[2], e.yaw, prop.half);
        const hs = pts.map((q) => pins?.points?.[pinKey(q[0], q[1])]);
        // A point with a height but no recorded hit is probed again: the hit name is what tells a pile from the ground.
        const unknown = pts.map((q, i) => !Number.isFinite(hs[i]) || pins?.hits?.[pinKey(q[0], q[1])] === undefined);
        if (unknown.some(Boolean)) {
          pts.forEach((q, i) => {
            if (unknown[i]) need.push([q[0], q[1], e.coords[1] + RAY_UP]);
          });
          continue;
        }
        const bad = hitBlocks(pts.map((q) => pins.hits[pinKey(q[0], q[1])]));
        if (bad) {
          refusals["it would stand on " + bad] = 1;
          continue;
        }
        const door = (pins?.doors ?? []).find((d) => pts.some((q) => Math.hypot(q[0] - d[0], q[1] - d[1]) < DOOR_SWING));
        if (door) {
          refusals["in a door's swing"] = 1;
          continue;
        }
        const g = benchGround(hs, e.coords[1]);
        if (g.why) {
          refusals[g.why] = 1;
          continue;
        }
        e.coords = [e.coords[0], g.y, e.coords[2]];
      }
    } else {
      // Along the back wall: the row's facing is away from the house, the frame's long side is across it.
      e.yaw = r3(((e.yaw + 90 + 540) % 360) - 180);
      // The island's door leaves (the back and side doors the dossier lacks): the frame's two ends and its middle stay out of every swing.
      if (prop.doors && need) {
        const pts = benchPoints(e.coords[0], e.coords[2], e.yaw, prop.half);
        const unknown = pts.some((q) => !Number.isFinite(pins?.points?.[pinKey(q[0], q[1])]) || pins?.hits?.[pinKey(q[0], q[1])] === undefined);
        if (unknown) {
          for (const q of pts) need.push([q[0], q[1], e.coords[1] + RAY_UP]);
          continue;
        }
        if ((pins?.doors ?? []).some((d) => pts.some((q) => Math.hypot(q[0] - d[0], q[1] - d[1]) < DOOR_SWING))) {
          refusals["in a door's swing"] = 1;
          continue;
        }
      }
    }
    return { ok: true, e, kind, building: building.id, gap: prop.gap };
  }
  if (need?.length && (prop.ground || prop.doors)) return { ok: false, pending: true, kind, building: building.id, why: "ground heights not probed yet" };
  return { ok: false, kind, building: building.id, why: Object.keys(refusals).join("; ") };
}

// The layer text without its bench entities (each entity is one block that starts at a line with no leading space).
export function withoutBenches(text) {
  return text
    .split("\n")
    .reduce((blocks, line) => {
      if (!line.startsWith(" ") && !line.startsWith("}") && line.trim()) blocks.push([]);
      if (blocks.length) blocks[blocks.length - 1].push(line);
      else blocks.push([line]);
      return blocks;
    }, [])
    .filter((b) => !/^\w+ Fit_\w+_(bench|flowers)\w*_b\d+ /.test(b[0]))
    .map((b) => b.join("\n"))
    .join("\n");
}

// The ground under every point in `need` ([x, z, rayFromY]): one boot of the island's test server, one vertical ray each. The
// answer is the higher of the hit point and the terrain under it (the terrain is not on every trace layer, as RayLine says). The
// bench rows of the slot's layer on disk are taken out for the probe and put back, so a ray never lands on the bench it replaces.
async function probeGround(root, place, need, layerName) {
  const audit = await import("../../agent-box/place-audit.mjs");
  const { parseRays } = await import("../place/place.mjs");
  const pts = [...new Map(need.map((n) => [pinKey(n[0], n[1]), n])).values()];
  const extra = pts.map((q, i) => `ray g${i} ${q[0]} ${r3(q[2])} ${q[1]} ${q[0]} ${r3(q[2] - RAY_UP - RAY_DOWN)} ${q[1]}`);
  const layerFile = join(root, LAYER_DIR, layerName);
  const before = existsSync(layerFile) ? readFileSync(layerFile, "utf8") : null;
  if (before != null) writeFileSync(layerFile, withoutBenches(before));
  let run;
  try {
    run = audit.measureTown(audit.auditRecords({ layers: [layerName], island: true }), { island: true, extra, preferLocal: true, log: console.log });
  } finally {
    if (before != null) writeFileSync(layerFile, before);
  }
  if (run.outText == null) throw new Error("the island's test server wrote no measurements (lines above)");
  const rays = parseRays(run.outText).rays;
  const points = {};
  const hits = {};
  const missing = [];
  pts.forEach((q, i) => {
    const r = rays.get(`g${i}`);
    const y = Math.max(Number.isFinite(r?.p?.[1]) ? r.p[1] : -1e9, Number.isFinite(r?.sy) ? r.sy : -1e9);
    if (y > -1e8) {
      points[pinKey(q[0], q[1])] = r3(y);
      // The ray's own hit counts as the ground only when it is the higher one: the terrain wins over a lower prop, so it has no name.
      hits[pinKey(q[0], q[1])] = Number.isFinite(r?.p?.[1]) && r.p[1] >= (Number.isFinite(r?.sy) ? r.sy : -1e9) ? String(r.hit ?? "") : "terrain";
    }
    else missing.push(pinKey(q[0], q[1]));
  });
  if (missing.length) throw new Error(`the server gave no ground height for ${missing.length} point(s): ${missing.slice(0, 3).join(" ")}`);
  // The leaves of the island's doors near a probed point: the audit's swing test counts a prop in a leaf's arc, and the dossier's
  // entrances miss the side and back doors a bench on a side wall can land in.
  const doors = [];
  for (const line of String(run.outText).split("\n")) {
    const m = /^door .*? c=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) /.exec(line.trim());
    if (!m) continue;
    const [x, y, z] = [Number(m[1]), Number(m[2]), Number(m[3])];
    if (pts.some((q) => Math.hypot(q[0] - x, q[1] - z) < DOOR_NEAR && Math.abs(q[2] - RAY_UP - y) < 3)) doors.push([r3(x), r3(z)]);
  }
  return { points, hits, doors };
}

const entityLine = (e, name, cls) => `${cls ?? e.cls ?? "GenericEntity"} ${name} : "${e.ref}" {
 coords ${e.coords.join(" ")}
 angleY ${e.yaw}
}
`;

function parseArgs(args) {
  const flags = {};
  const pos = [];
  const bad = [];
  for (const a of args) {
    if (["--dry", "--no-audit", "--terrain", "--help"].includes(a)) flags[a.slice(2)] = true;
    else if (a.startsWith("--")) bad.push(a);
    else pos.push(a);
  }
  return { flags, pos, bad };
}

const USAGE = "usage: tools\\cli town street-life <place> [--dry] [--no-audit] [--terrain]";

export async function main(args, { root = ROOT } = {}) {
  const { flags, pos, bad } = parseArgs(args);
  if (flags.help || bad.length || pos.length !== 1) {
    console.log(bad.length ? `unknown flag ${bad.join(", ")}\n${USAGE}` : USAGE);
    return flags.help ? 0 : 2;
  }
  const place = pos[0];
  if (!SLUG_RX.test(place)) {
    console.log(USAGE);
    return 2;
  }
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.dry && !new RegExp(`^ws/town-draft-${place}-${SLOT}(-[a-z0-9-]+)?$`).test(branch ?? "")) {
    console.log(`REFUSE  town street-life writes an island layer only from the worktree ws/town-draft-${place}-${SLOT} (this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }
  let results;
  let mapping;
  try {
    const placeDir = join(root, "docs", "everon", "places", place);
    const dossier = readJson(join(placeDir, "dossier.json"), "dossier");
    mapping = readJson(join(placeDir, "mapping.json"), "mapping");
    const template = loadTemplate(mapping.template, root);
    const entry = mapping.slots?.[SLOT];
    if (!entry || entry.none) throw new Error(`slot ${SLOT} is not mapped (an entry with "drops": [{ place, building }])`);
    const outposts = { [place]: dossier };
    const problems = checkMapping(mapping, dossier, template, outposts).filter((p) => p.startsWith(`slot ${SLOT}:`));
    if (problems.length) throw new Error(problems.join("; "));
    const shell = shellReader(root);
    const readGame = await gameReader();
    const planAll = async (pins, need) => {
      const out = [];
      for (const dr of entry.drops) {
        if (dr.place !== place) throw new Error(`drop ${dr.place}/${dr.building}: this step fits ${place}'s own buildings only`);
        const building = dossier.buildings.find((b) => b.id === dr.building);
        if (!building) throw new Error(`${dr.building} is not in the dossier`);
        const kinds = PLAN[dr.building];
        if (!kinds) throw new Error(`no props planned for ${dr.building} (PLAN in tools/town/street-life.mjs)`);
        const others = [];
        for (const kind of kinds) {
          const res = await planProp({ kind, building, dossier, root, shell, readGame, others, pins, need });
          out.push(res);
          if (res.ok) others.push({ x: res.e.coords[0], z: res.e.coords[2], gap: res.gap });
        }
      }
      return out;
    };
    let pins = readPins(root, place, SLOT);
    let need = [];
    results = await planAll(pins, need);
    if (need.length) {
      if (!flags.terrain) throw new Error(`the ground under ${new Set(need.map((n) => pinKey(n[0], n[1]))).size} bench point(s) is not in ${pinFileOf(place, SLOT)}: run again with --terrain`);
      const found = await probeGround(root, place, need, layerNameOf(place, SLOT));
      const doors = [...new Map([...(pins?.doors ?? []), ...found.doors].map((d) => [d.join(","), d])).values()];
      pins = { points: { ...(pins?.points ?? {}), ...found.points }, hits: { ...(pins?.hits ?? {}), ...found.hits }, doors };
      const pinFile = writePins(root, place, SLOT, pins.points, new Date().toISOString().slice(0, 10));
      const hitsSorted = Object.fromEntries(Object.entries(pins.hits).sort(([a], [b]) => a.localeCompare(b)));
      writeFileSync(pinFile, `${JSON.stringify({ ...JSON.parse(readFileSync(pinFile, "utf8")), hits: hitsSorted, doors }, null, 2)}\n`);
      console.log(`terrain ${Object.keys(found.points).length} point(s) probed -> ${relative(root, pinFile).replaceAll("\\", "/")}`);
      need = [];
      results = await planAll(pins, need);
      if (need.length) throw new Error("the ground probe did not cover every bench spot");
    }
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  let failed = 0;
  const layerParts = [];
  const counts = {};
  for (const r of results) {
    if (!r.ok && PROPS[r.kind].optional) {
      // A bench with no level ground along any wall of its house is left out: a bench on a slope or on the stairs hovers or sinks.
      console.log(`skip  ${r.building} ${r.kind}: no level ground for it (${r.why.split("; ").slice(0, 3).join("; ")}...)`);
      continue;
    }
    if (!r.ok) {
      failed++;
      console.log(`REFUSE  ${r.building} ${r.kind}: ${r.why}`);
      continue;
    }
    counts[r.kind] = (counts[r.kind] ?? 0) + 1;
    const name = `Fit_${place.replaceAll("-", "_")}_street_life_${r.kind.replaceAll("-", "_")}_${r.building}`;
    console.log(`prop  ${r.building} ${r.kind}: ${r.e.coords.join(" ")} yaw ${r.e.yaw}`);
    layerParts.push(entityLine(r.e, name, PROPS[r.kind].cls));
  }
  if (failed) {
    console.log(`SUMMARY town-street-life fail ${place}: ${failed} of ${results.length} prop(s) refused`);
    return 1;
  }
  const layerName = layerNameOf(place, SLOT);
  const claim = {
    schema: 1,
    place,
    slot: SLOT,
    drops: mapping.slots[SLOT].drops.map((d) => ({ place: d.place, building: d.building })),
    kit: SLOT,
    size: "small",
    layer: `${LAYER_DIR}/${layerName}`,
    kinds: [],
    rows: layerParts.length,
    date: new Date().toISOString().slice(0, 10),
  };
  const layerFile = join(root, LAYER_DIR, layerName);
  const claimFile = join(root, CLAIMS_DIR, claimNameOf(place, SLOT));
  const shown = (f) => relative(root, f).replaceAll("\\", "/");
  const summary = Object.entries(counts).map(([k, n]) => `${n} ${k}`).join(", ");
  if (flags.dry) {
    console.log(layerParts.join(""));
    console.log(`SUMMARY town-street-life dry ${place}: ${layerParts.length} prop(s) (${summary}); would write ${shown(layerFile)} and ${shown(claimFile)}`);
    return 0;
  }
  mkdirSync(dirname(layerFile), { recursive: true });
  mkdirSync(dirname(claimFile), { recursive: true });
  writeFileSync(layerFile, layerParts.join(""));
  writeFileSync(claimFile, `${JSON.stringify(claim, null, 2)}\n`);
  let ok = true;
  const steps = [];
  if (!flags["no-audit"]) {
    const cli = join(root, "tools", process.platform === "win32" ? "cli.cmd" : "cli");
    const r = spawnSync(cli, ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", layerName, "--hard-layer", layerName], { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    steps.push(`audit ${r.status === 0 ? "ok" : "fail"}`);
    if (r.status !== 0) console.log(String(r.stdout ?? "").split("\n").slice(-25).join("\n"));
    ok = r.status === 0;
  }
  console.log(`SUMMARY town-street-life ${ok ? "ok" : "fail"} ${place}: ${layerParts.length} prop(s) (${summary}), ${shown(layerFile)}, ${shown(claimFile)}${steps.length ? `; ${steps.join(", ")}` : "; audit skipped"}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
