// tools/town/drops.mjs - `tools\cli town drops <place> <slot>`: fit an `outposts` slot, a set of small drop-offs that stand on
// buildings of OTHER places' dossiers (the parcel run's drop points, 300 m or more from the depot).
//
//   town drops <place> <slot> [--dry] [--no-audit] [--no-look] [--out-dir d]
//
// Reads  docs/everon/places/<place>/mapping.json: the slot's entry lists `drops: [{ place, building }]`;
//        docs/everon/places/<drop place>/dossier.json for each drop's building (footprint, entrance, floor).
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_<slot>.layer   (one mailbox per drop)
//        tools/town/claims/<place>-<slot>.json                                               (the drops it names)
//
// No step types a coordinate. Each drop is one kit row, a parcel mailbox (LF_ParcelDrop, which carries the job-site component
// and the drop-off action) a metre outside the footprint on the building's entrance side, beside the door when the dossier
// knows one. The same offline seat `town fit` uses (tools/town/seat.mjs) then slides it onto the host's real wall from its
// model colliders, so the mailbox stands against the wall, back to it. A drop whose host has no readable model is refused.
// Each entity is named `Fit_<drop place>_<slot>_drop_<building>`: the jobs system registers a drop by its entity name.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CLAIMS_DIR, LAYER_DIR, claimNameOf, layerNameOf } from "./check.mjs";
import { anchorFor, framedRows, planFitSeated } from "./fit.mjs";
import { toLocal, toWorld } from "./kit.mjs";
import { checkMapping, loadTemplate } from "./map.mjs";
import { shellReader } from "./seat.mjs";

export const help = "drops <place> <slot> [--dry] [--no-audit] [--no-look]   an outposts slot (parcel drop-offs on other places' buildings): one mailbox per drop on the entrance wall, seated on the host's real wall; writes the layer and the claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
export const DROP_PREFAB = "{CF8596F431D9EE52}Prefabs/Props/Roleplay/LF_ParcelDrop.et";
export const DROP_SOURCE = "addons/LF_Everon1989/Prefabs/Props/Roleplay/LF_ParcelDrop.et";
// The slot's kit picks the prop: the parcel run's mailbox, the fuel run's drum (LF_FuelDrop, #3370) or the illicit buyer's spot marker (core LF_BuyerSpot, #3416).
export const DROP_KITS = {
  "parcel-drops": { prefab: DROP_PREFAB, source: DROP_SOURCE },
  // The postal van run (LF_PostalRun): a post office stand where the run starts, unloads and hands in, one per office.
  "postal-drops": { prefab: "{BEEEAB7F4544E89C}Prefabs/Props/Roleplay/LF_PostalPoint_Everon.et", source: "addons/LF_Everon1989/Prefabs/Props/Roleplay/LF_PostalPoint_Everon.et" },
  "fuel-drops": { prefab: "{3E8B5A27C94D61F0}Prefabs/Props/Roleplay/LF_FuelDrop.et", source: "addons/LF_Everon1989/Prefabs/Props/Roleplay/LF_FuelDrop.et" },
  "buyer-spots": { prefab: "{208688EDE535B6A4}Prefabs/Systems/Laundering/LF_BuyerSpot.et", source: "addons/LifeFramework/Prefabs/Systems/Laundering/LF_BuyerSpot.et" },
  // The placeable ATM (#3352, #3355): each machine overrides the prefab's place id (`placeComp` is the prefab's own LF_PlaceComponent GUID) so the ATMs are told apart in LF_Places.
  atms: { prefab: "{7070D89FFB6DC880}Prefabs/Props/Money/LF_ATM.et", source: "addons/LifeFramework/Prefabs/Props/Money/LF_ATM.et", placeComp: "{7070D89FFB6DC889}", placeId: (dr) => `atm.${dr.place}.${dr.building}` },
  // A payphone booth at a kerb beside a building (#3358): back to the wall, front to the street, like a mailbox.
  payphones: { prefab: "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et", source: "addons/LifeFramework/Prefabs/Comms/LF_Payphone.et" },
  // A bus stop sign at a kerb beside a building, like a payphone: each overrides its LF_BusStopComponent stop id, and its
  // name when the mapping entry gives a `label` (a pack string key), so the timetable tells the stops apart.
  "bus-stops": {
    prefab: "{D3B1C7A4E2F05986}Prefabs/Travel/LF_BusStop.et", source: "addons/LifeFramework/Prefabs/Travel/LF_BusStop.et",
    placeComp: "{9900D66B63362F47}", placeClass: "LF_BusStopComponent", placeIdField: "m_sStopId",
    placeId: (dr) => `bus.${dr.place}.${dr.building}`, placeFields: (dr) => (dr.label ? { m_sLabel: dr.label } : {}),
  },
  // A home's "For sale" sign (#3584, `town homes`): placed with the same entrance-wall seat as a drop.
  // A resident home's brass name plate (#3233, `town residenthomes`): the small vanilla wall plate, seated like a drop.
  "resident-plate": { prefab: "{9182BD264965878C}Prefabs/Props/Signs/LF_Sign_PlateSmall.et", source: "addons/LifeFramework/Prefabs/Props/Signs/LF_Sign_PlateSmall.et" },
  // The police car pool board (#4352): a cork board on the station wall, its bay 6 m in front. No slot kit names it; the station card seats its one board with this entry.
  "police-car-board": { prefab: "{AEFD4F9B8A69C66E}Prefabs/Props/Police/LF_PoliceCarPoolBoard.et", source: "addons/LifeFramework/Prefabs/Props/Police/LF_PoliceCarPoolBoard.et" },
  // The respray bay's bench (#4589, WO-L9b): on the mechanic's garage wall. Its pad offset is world-axis, so the placed bay overrides
  // it, and its place id and pad id too; the seated line carries those by hand, so no slot kit names it.
  "livery-bay": { prefab: "{837750C2B0CBF9D9}Prefabs/Props/LF_LiveryBay.et", source: "addons/LifeFramework/Prefabs/Props/LF_LiveryBay.et" },
  // The medic fleet board (#4432, #4566): a cork board on the clinic wall, its bay 6 m in front. No slot kit names it; the clinic board card seats its one board with this entry.
  "medic-fleet-board": { prefab: "{7626F19FBE72C466}Prefabs/Props/Medic/LF_MedicCarPoolBoard.et", source: "addons/LifeFramework/Prefabs/Props/Medic/LF_MedicCarPoolBoard.et" },
  // The opening-hours plate beside a shop door (#4834): LF_Sign_HoursPlate (enamel style, prints the keeper's hours), flush on the customer-side wall
  // at eye height (pivot 1.5 m over the ground: `lift` stands its base over the ground for the wall seat, `dy` raises it after), out of every
  // door's swing and walk-up (`bounds` is the body's box from `place bounds`, for the door test).
  "hours-plates": {
    prefab: "{379F4E8692F36269}Prefabs/Props/Signs/LF_Sign_HoursPlate.et", source: "addons/LifeFramework/Prefabs/Props/Signs/LF_Sign_HoursPlate.et",
    lift: 0.25, dy: 1.25, scale: 0.8, bounds: { min: [-0.467, -0.225, -0.002], max: [0.433, 0.225, 0.034] },
  },
  "house-sale": { prefab: "{2F815241C89039BC}Prefabs/Props/Property/LF_HouseSaleSign.et", source: "addons/LifeFramework/Prefabs/Props/Property/LF_HouseSaleSign.et" },
};
const OUTSIDE = 1.0; // the mailbox starts this far outside the footprint; the wall seat slides it the rest of the way
const GROUND_LIFT = 0.03; // the mailbox's foot stands this far above the dossier's floor height: a hairline over the ground, never in it
const WALL_AIR = 0.05; // the seat puts the back on the wall plane; a corrugated or ribbed wall still clips it, so it stands this far off
const BESIDE_DOOR = 1.6; // beside a known entrance, this far along the wall
const r3 = (n) => Math.round(n * 1000) / 1000;
const readJson = (f, what) => {
  if (!existsSync(f)) throw new Error(`no ${what}: ${f} does not exist`);
  return JSON.parse(readFileSync(f, "utf8"));
};

// Pure: how far a ray from (x, z) along (dx, dz) travels before it leaves the polygon (its farthest crossing), 0 when none.
export function exitDistance(poly, x, z, dx, dz) {
  let best = 0;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [ax, az] = poly[j];
    const [bx, bz] = poly[i];
    const ex = bx - ax;
    const ez = bz - az;
    const den = dx * ez - dz * ex;
    if (Math.abs(den) < 1e-9) continue;
    const t = ((ax - x) * ez - (az - z) * ex) / den;
    const u = ((ax - x) * dz - (az - z) * dx) / den;
    if (t > 0 && u >= 0 && u <= 1 && t > best) best = t;
  }
  return best;
}

// Pure: the drop's kit and the host it sits on, from the dossier's building: a one-row kit whose row stands outside the
// footprint on the side the kit's front (+z) is turned to, which is the entrance side.
// `sideways` (optional) overrides the mailbox's offset along the wall, as a share of the wall's half width; `lift` raises it
// (metres) off the dossier's floor height, for a drop on sloping ground.
// `turn` (0 to 3, quarter turns) puts the mailbox on another side than the entrance side (0), for a building whose entrance
// side is a loading dock or a canopy.
export function dropKit(building, slotId, sideways = null, lift = GROUND_LIFT, turn = 0, kitName = "parcel-drops", alongAt = null) {
  const prop = DROP_KITS[kitName] ?? DROP_KITS["parcel-drops"];
  const front = "+z";
  const a = anchorFor(building, { anchoring: { front, floorY: building.firstFloor ?? 0 } });
  const [lx, lz] = [[0, 1], [1, 0], [0, -1], [-1, 0]][((turn % 4) + 4) % 4];
  const [dx, dz] = toWorld(lx, lz, a.yaw);
  const f = building.footprint ?? {};
  const poly = f.corners?.length >= 3 ? f.corners : [[a.x - (f.w ?? 6) / 2, a.z - (f.d ?? 6) / 2], [a.x + (f.w ?? 6) / 2, a.z - (f.d ?? 6) / 2], [a.x + (f.w ?? 6) / 2, a.z + (f.d ?? 6) / 2], [a.x - (f.w ?? 6) / 2, a.z + (f.d ?? 6) / 2]];
  const reach = exitDistance(poly, a.x, a.z, dx, dz);
  const ent = (building.entrances ?? []).find((e) => e.side === "front") ?? (building.entrances ?? [])[0];
  // The entrance wall's half width: the footprint's extent along the kit's x axis. A mailbox beside the door stays on that wall;
  // with no door known it stands off the middle, where a door usually is (a ray through a doorway finds no wall to sit on).
  const [ux, uz] = toWorld(lz, -lx, a.yaw);
  const along = poly.map(([px, pz]) => (px - a.x) * ux + (pz - a.z) * uz);
  const half = Math.min(Math.abs(Math.min(...along)), Math.max(...along));
  let ox = r3(half * 0.45);
  if (ent && !turn) {
    const door = toLocal(ent.x - a.x, ent.z - a.z, a.yaw)[0];
    ox = door >= 0 ? door + BESIDE_DOOR : door - BESIDE_DOOR;
    ox = r3(Math.max(-(half - 0.6), Math.min(half - 0.6, ox)));
  }
  if (sideways !== null) ox = r3(half * sideways);
  // `along` (metres, from the kit's centre line, + toward the kit's +x): an exact spot on the wall, for a plate on a pier between a window and a door.
  if (alongAt !== null && Number.isFinite(alongAt)) ox = r3(alongAt);
  const reachOut = r3(reach + OUTSIDE);
  const offset = [r3(lx * reachOut + lz * ox), lift, r3(lz * reachOut - lx * ox)];
  const yaw = r3((Math.atan2(lx, lz) * 180) / Math.PI);
  const id = `drop-${building.id}`;
  return {
    id,
    kit: {
      schema: 1,
      kit: kitName,
      system: "jobs",
      source: { file: prop.source, group: "*", origin: "prefab", frame: "building-local" },
      scope: "room",
      anchoring: { floorY: building.firstFloor ?? 0, floors: [{ id: "f0", y: building.firstFloor ?? 0 }], footprint: { source: "rows", min: [offset[0], offset[2]], max: [offset[0], offset[2]] }, front, counters: [], adapt: "One mailbox outside the entrance wall; the offset is the footprint's own reach, then the host's wall seat slides it to the wall." },
      rows: [{ id, prefab: prop.prefab, role: "interactable", verb: "against wall", tags: ["wall"], offset, yaw, config: [], wall: "+z", floor: "f0" }],
    },
  };
}

// The game's own prefab text, for the inherited component GUIDs of a vanilla prefab; null when no game install is readable.
async function gameReader() {
  try {
    const vfs = await (await import("../lib/game-prefab.mjs")).gameVfs();
    return (rel) => (vfs.exists(rel) ? vfs.readTextFile(rel) : null);
  } catch {
    return null;
  }
}

const entityLine = (e) => `GenericEntity ${e.name} : "${e.ref}" {${e.placeId ? `
 components {
  ${e.placeClass ?? "LF_PlaceComponent"} "${e.placeComp}" {
   ${e.placeIdField ?? "m_sId"} "${e.placeId}"${Object.entries(e.placeFields ?? {}).map(([k, v]) => `
   ${k} "${v}"`).join("")}
  }
 }` : ""}
 coords ${e.coords.join(" ")}
 angleY ${e.yaw}${e.scale ? `
 scale ${e.scale}` : ""}
}`;

// Pure: the prop's oriented box in the audit's shape ({ c, h, a }) at its final spot: the seated row `ent` (coords, yaw), a hair off the wall
// (WALL_AIR along its facing) and raised by `dy`.
export function boxAt(bounds, ent, dy, scale = 1) {
  const k = (ent.yaw * Math.PI) / 180;
  const at = [ent.coords[0] + Math.sin(k) * WALL_AIR, ent.coords[1] + dy, ent.coords[2] + Math.cos(k) * WALL_AIR];
  const mid = [0, 1, 2].map((i) => ((bounds.min[i] + bounds.max[i]) / 2) * scale);
  const [ox, oz] = toWorld(mid[0], mid[2], ent.yaw);
  return {
    c: [at[0] + ox, at[1] + mid[1], at[2] + oz],
    h: [0, 1, 2].map((i) => ((bounds.max[i] - bounds.min[i]) / 2) * scale),
    a: [[Math.cos(k), 0, -Math.sin(k)], [0, 1, 0], [Math.sin(k), 0, Math.cos(k)]],
  };
}

// One drop's plan: the shared town-fit planner, with an in-memory mapping and template for the drop's own place.
export async function planDrop({ drop, slotId, dossier, root, shell, readGame, kitName = "parcel-drops", accept = null }) {
  const building = dossier.buildings.find((b) => b.id === drop.building);
  if (!building) return { ok: false, errors: [`${drop.place}: building ${drop.building} is not in its dossier`], warnings: [] };
  const template = { schema: 1, kind: "drops", slots: [{ id: slotId, required: false, kit: kitName, host: "building", needs: { interior: "optional" }, places: [] }] };
  const mapping = { schema: 1, template: template.kind, place: dossier.place.slug, slots: { [slotId]: { building: building.id } } };
  // The wall in front of a doorway or window is no wall to sit on: try the door side, then the middle and both thirds of the wall.
  let plan = null;
  // A mapping entry's own `sideways` and `lift` (set after a look or the audit shows the default spot clips) skip the search.
  // `accept(entity)` (optional, #3886): a seat on the wall is only taken when it returns true, so a caller that knows the doors
  // slides the prop along the wall to the next spot; a wall with no accepted spot is a refused plan.
  // A prop with `bounds` (a wall plate at eye height) is taken only out of every door's swing and walk-up, as `town homes` does for its sign.
  const prop = DROP_KITS[kitName];
  if (!accept && prop?.bounds) {
    const { doorHit, loadDoors } = await import("./homes.mjs");
    const doors = loadDoors(root, dossier.place.slug);
    accept = (ent) => !doorHit(boxAt(prop.bounds, ent, drop.dy ?? prop.dy ?? 0, prop.scale ?? 1), doors);
  }
  const tries = drop.sideways !== undefined || drop.along !== undefined ? [drop.sideways ?? null] : accept ? [null, 0, 0.45, -0.45, 0.8, -0.8, 0.25, -0.25, 0.65, -0.65, 0.95, -0.95] : [null, 0, 0.45, -0.45, 0.8, -0.8];
  let accepted = false;
  for (const sideways of tries) {
    const { id, kit } = dropKit(building, slotId, sideways, drop.lift ?? prop?.lift ?? GROUND_LIFT, drop.turn ?? 0, kitName, drop.along ?? null);
    const opts = { place: drop.place, slot: slotId, dossier, mapping, template, kits: { [kit.kit]: kit }, skipClaims: true, fetch: false };
    plan = { ...(await planFitSeated(opts, { root, readGame, shell })), rowId: id, building };
    if (!plan.ok) break;
    if (plan.seat?.rows?.[0]?.wall && (!accept || accept(plan.ents.rowEnts[0]))) {
      accepted = true;
      break;
    }
  }
  if (accept && plan?.ok && !accepted) plan = { ...plan, ok: false, errors: [...(plan.errors ?? []), "no spot on this wall clears the doors"] };
  // Off the wall by WALL_AIR along the mailbox's facing (the wall it was seated on is behind it).
  const e = plan?.ok ? plan.ents.rowEnts[0] : null;
  if (e && prop?.scale) e.scale = prop.scale;
  if (e && prop?.placeId) Object.assign(e, { placeId: prop.placeId(drop), placeComp: prop.placeComp, placeClass: prop.placeClass, placeIdField: prop.placeIdField, placeFields: prop.placeFields?.(drop) });
  const onWall = !!plan?.seat?.rows?.[0]?.wall;
  // A mapping entry's `dy` (metres, after the wall seat): the ground at the seated spot is higher or lower than the dossier's
  // floor height of the host, as the contact sheet (`look contact`) reads it. Unlike `lift` it never moves the wall seat (#3880).
  const dy = Number.isFinite(drop.dy) ? drop.dy : prop?.dy;
  if (e && Number.isFinite(dy)) e.coords = [e.coords[0], r3(e.coords[1] + dy), e.coords[2]];
  if (e && onWall) {
    const a = (e.yaw * Math.PI) / 180;
    e.coords = [r3(e.coords[0] + Math.sin(a) * WALL_AIR), e.coords[1], r3(e.coords[2] + Math.cos(a) * WALL_AIR)];
  }
  if (e && (onWall || prop?.placeId)) {
    // Written in its prefab's frame like every fitted row (tools/lib/height-frame.mjs); a drop prop that reads y over the
    // terrain has no probed ground here and is refused rather than written in the wrong frame.
    const f = framedRows([e], { root, frameOfRef: shell?.frameOf ?? null });
    if (f.pending.length) plan = { ...plan, ok: false, errors: [...(plan.errors ?? []), `${e.ref} reads y over the terrain (RELATIVE_Y): a wall drop is planned in world heights`] };
    else plan.layerText = `${entityLine(f.rowEnts[0])}
`;
  }
  return plan;
}

function parseArgs(args) {
  const flags = {};
  const pos = [];
  const bad = [];
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--out-dir") flags["out-dir"] = args[++i];
    else if (["--dry", "--no-audit", "--no-look", "--help"].includes(a)) flags[a.slice(2)] = true;
    else if (a.startsWith("--")) bad.push(a);
    else pos.push(a);
  }
  return { flags, pos, bad };
}

const USAGE = "usage: tools\\cli town drops <place> <slot> [--dry] [--no-audit] [--no-look] [--out-dir d]";

export async function main(args, { root = ROOT, shell: ctxShell = null } = {}) {
  const { flags, pos, bad } = parseArgs(args);
  if (flags.help || bad.length || pos.length !== 2) {
    console.log(bad.length ? `unknown flag ${bad.join(", ")}\n${USAGE}` : USAGE);
    return flags.help ? 0 : 2;
  }
  const [place, slotId] = pos;
  if (!SLUG_RX.test(place) || !SLUG_RX.test(slotId)) {
    console.log(USAGE);
    return 2;
  }
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.dry && !flags["out-dir"] && !new RegExp(`^ws/town-draft-${place}-${slotId}(-[a-z0-9-]+)?$`).test(branch ?? "")) {
    console.log(`REFUSE  town drops writes an island layer only from the worktree ws/town-draft-${place}-${slotId}[-word] (this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }

  let plans;
  let mapping;
  let slotKit = "parcel-drops";
  try {
    const placeDir = join(root, "docs", "everon", "places", place);
    const dossier = readJson(join(placeDir, "dossier.json"), "dossier");
    mapping = readJson(join(placeDir, "mapping.json"), "mapping");
    const template = loadTemplate(mapping.template, root);
    const slot = template.slots.find((s) => s.id === slotId);
    if (!slot || slot.host !== "outposts") throw new Error(`the ${template.kind} template has no outposts slot '${slotId}'`);
    slotKit = slot.kit;
    const entry = mapping.slots?.[slotId];
    if (!entry || entry.none) throw new Error(`slot ${slotId} is not mapped (an entry with "drops": [{ place, building }])`);
    const outposts = {};
    for (const dr of entry.drops ?? []) if (dr?.place && !outposts[dr.place]) outposts[dr.place] = readJson(join(root, "docs", "everon", "places", dr.place, "dossier.json"), `dossier of ${dr.place}`);
    const problems = checkMapping(mapping, dossier, template, outposts).filter((p) => p.startsWith(`slot ${slotId}:`));
    if (problems.length) throw new Error(problems.join("; "));
    const shell = ctxShell ?? shellReader(root);
    const readGame = await gameReader();
    plans = [];
    for (const drop of entry.drops) plans.push({ drop, plan: await planDrop({ drop, slotId, dossier: outposts[drop.place], root, shell, readGame, kitName: slotKit }) });
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }

  let failed = 0;
  for (const { drop, plan } of plans) {
    for (const w of plan.warnings ?? []) console.log(`warn  ${drop.place}/${drop.building}: ${w}`);
    if (!plan.ok) {
      failed++;
      for (const e of plan.errors) console.log(`REFUSE  ${drop.place}/${drop.building}: ${e}`);
      continue;
    }
    const r = plan.seat?.rows?.[0];
    const e = plan.ents.rowEnts[0];
    console.log(`drop  ${drop.place}/${drop.building} ${plan.building.stem}: ${e ? e.coords.join(" ") : "(left out)"}${r?.wall ? ` wall ${r.wall.side} shift ${r.wall.shift}` : ""}${r?.notes?.length ? ` (${r.notes.join("; ")})` : ""}`);
    if (!e) failed++;
  }
  if (failed) {
    console.log(`SUMMARY town-drops fail ${place}.${slotId}: ${failed} of ${plans.length} drop(s) refused`);
    return 1;
  }

  const layerText = plans.map(({ plan }) => plan.layerText).join("");
  const layerName = layerNameOf(place, slotId);
  const claim = {
    schema: 1,
    place,
    slot: slotId,
    drops: mapping.slots[slotId].drops.map((d) => ({ place: d.place, building: d.building })),
    kit: slotKit,
    // Five lit booths are five lights: the small budget allows three (#3358), so a payphones slot is medium.
    size: slotKit === "payphones" ? "medium" : "small",
    layer: `${LAYER_DIR}/${layerName}`,
    kinds: [],
    rows: plans.length,
    date: new Date().toISOString().slice(0, 10),
  };
  const layerFile = join(resolve(root, flags["out-dir"] ?? LAYER_DIR), layerName);
  const claimFile = join(resolve(root, flags["out-dir"] ?? CLAIMS_DIR), claimNameOf(place, slotId));
  const shown = (f) => relative(root, f).replaceAll("\\", "/");
  if (flags.dry) {
    console.log(layerText);
    console.log(JSON.stringify(claim, null, 2));
    console.log(`SUMMARY town-drops dry ${place}.${slotId}: ${plans.length} drop(s); would write ${shown(layerFile)} and ${shown(claimFile)}`);
    return 0;
  }
  mkdirSync(dirname(layerFile), { recursive: true });
  mkdirSync(dirname(claimFile), { recursive: true });
  writeFileSync(layerFile, layerText);
  writeFileSync(claimFile, `${JSON.stringify(claim, null, 2)}\n`);

  const steps = [];
  let ok = true;
  const cli = join(root, "tools", process.platform === "win32" ? "cli.cmd" : "cli");
  if (!flags["no-audit"]) {
    const r = spawnSync(cli, ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", layerName, "--hard-layer", layerName], { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    steps.push(`audit ${r.status === 0 ? "ok" : "fail"}`);
    ok = r.status === 0;
  }
  console.log(`SUMMARY town-drops ${ok ? "ok" : "fail"} ${place}.${slotId}: ${plans.length} drop(s), ${shown(layerFile)}, ${shown(claimFile)}${steps.length ? `; ${steps.join(", ")}` : "; audit skipped"}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
