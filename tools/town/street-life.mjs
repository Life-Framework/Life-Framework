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

export const help = "street-life <place> [--dry] [--no-audit]   the street-life outposts slot: benches, bins, flower boxes and washing lines on the residential houses the mapping names; writes the layer and the claim";

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
  bench: { prefab: "{BE839ACB6CB2726A}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et", source: "addons/LifeFramework/Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et", wall: true, gap: 1.6 },
  "bench-red": { prefab: "{24388E37E11BA18F}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_red.et", source: "addons/LifeFramework/Prefabs/Props/Seats/LF_Seat_BenchStreet_01_red.et", wall: true, gap: 1.6 },
  bin: { prefab: "{32CDE9FE3FB35D51}Prefabs/Props/Garbage/Bins/BinStreet_01.et", source: VANILLA_SOURCE, wall: true, gap: 0.9, cls: "SCR_DestructibleEntity" },
  flowers: { prefab: "{06ABA7878E8CC278}Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et", source: VANILLA_SOURCE, wall: true, gap: 1.2, cls: "SCR_DestructibleEntity" },
  line: { prefab: "{801E1CA69F368C44}Prefabs/Structures/Civilian/ClotheslineFrame_01.et", source: VANILLA_SOURCE, wall: false, gap: 2.5, rear: 3.2 },
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
const DOOR_CLEAR = 1.4;
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
async function planProp({ kind, building, dossier, root, shell, readGame, others }) {
  const prop = PROPS[kind];
  DROP_KITS[`street-${kind}`] = { prefab: prop.prefab, source: prop.source };
  const template = { schema: 1, kind: "drops", slots: [{ id: SLOT, required: false, kit: `street-${kind}`, host: "building", needs: { interior: "optional" }, places: [] }] };
  const mapping = { schema: 1, template: template.kind, place: dossier.place.slug, slots: { [SLOT]: { building: building.id } } };
  const turn = prop.wall ? 0 : 2;
  const refusals = {};
  for (const sideways of prop.wall ? SIDEWAYS : [0, 0.5, -0.5]) {
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
      e.coords = [r3(e.coords[0] + Math.sin(a) * 0.05), e.coords[1], r3(e.coords[2] + Math.cos(a) * 0.05)];
    } else {
      // Along the back wall: the row's facing is away from the house, the frame's long side is across it.
      e.yaw = r3(((e.yaw + 90 + 540) % 360) - 180);
    }
    return { ok: true, e, kind, building: building.id, gap: prop.gap };
  }
  return { ok: false, kind, building: building.id, why: Object.keys(refusals).join("; ") };
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
    if (["--dry", "--no-audit", "--help"].includes(a)) flags[a.slice(2)] = true;
    else if (a.startsWith("--")) bad.push(a);
    else pos.push(a);
  }
  return { flags, pos, bad };
}

const USAGE = "usage: tools\\cli town street-life <place> [--dry] [--no-audit]";

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
    results = [];
    for (const dr of entry.drops) {
      if (dr.place !== place) throw new Error(`drop ${dr.place}/${dr.building}: this step fits ${place}'s own buildings only`);
      const building = dossier.buildings.find((b) => b.id === dr.building);
      if (!building) throw new Error(`${dr.building} is not in the dossier`);
      const kinds = PLAN[dr.building];
      if (!kinds) throw new Error(`no props planned for ${dr.building} (PLAN in tools/town/street-life.mjs)`);
      const others = [];
      for (const kind of kinds) {
        const res = await planProp({ kind, building, dossier, root, shell, readGame, others });
        results.push(res);
        if (res.ok) others.push({ x: res.e.coords[0], z: res.e.coords[2], gap: res.gap });
      }
    }
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  let failed = 0;
  const layerParts = [];
  const counts = {};
  for (const r of results) {
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
