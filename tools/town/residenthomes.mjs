// tools/town/residenthomes.mjs - `tools\cli town residenthomes <place>`: the resident cast live in their own houses, with name plates (#3233).
//
//   town residenthomes <place> [--dry] [--out-dir d]
//
// Per home in HOMES below (a building of the place's purpose.json row with purpose "c"):
//   a house marker (Prefabs/Systems/Houses/LF_HouseMarker.et, no owner and no key: the doors stay locked to players for good; residents are AI and
//     walk through) that is also an LF_Places `House` place with its own id, `<place>.home.<resident id>`, at the footprint centre;
//   a brass name plate (LF_Sign_PlateSmall, small, brass colours) on the entrance wall, seated by the drop planner (tools/town/drops.mjs);
//   a `door` resident anchor a metre and a half in front of the plate, so the resident stands on the doorstep, not in the middle of the house.
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Homes_<place>_resident.layer
//        addons/LF_Everon1989/Configs/Signs/LF_Signs_Everon1989/LF_Signs_ResidentHomes.conf (+ .meta): one sign row per plate (#3555: its own file)
//        addons/LF_Everon1989/Language/lf_everon1989_localization.st: the plate titles (append-only; then `tools\cli regen-localization`)
//        docs/everon/places/<place>/resident-homes.json: the homes done; `town locks` drops their Private lock marker (the house marker carries them).
// The resident overlay names each home by `m_sHomePlaceId` (addons/LF_Everon1989/Configs/Residents/LF_Residents_Everon1989.conf).
// A generated file: rerun the script, never hand-edit. The island layer is written only from a ws/town-draft-<place>-* worktree (or --dry / --out-dir).
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { planDrop } from "./drops.mjs";
import { doorHit, loadDoors } from "./homes.mjs";
import { toLocal } from "./kit.mjs";
import { shellReader } from "./seat.mjs";

export const help = "residenthomes <place> [--dry]   the resident cast's houses (#3233): a locked house marker that is an LF_Places House with its own id, a brass name plate on the entrance wall and a door anchor; writes the layer, the sign rows, the plate strings and resident-homes.json";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const SIGN_DIR = "addons/LF_Everon1989/Configs/Signs/LF_Signs_Everon1989";
export const ST_FILE = "addons/LF_Everon1989/Language/lf_everon1989_localization.st";
export const MARKER_PREFAB = "{4D1B7E92A6C03F58}Prefabs/Systems/Houses/LF_HouseMarker.et";
export const LOCK_COMPONENT_GUID = "{A3F1C5D7092E4B68}";
export const PLATE_PREFAB = "{9182BD264965878C}Prefabs/Props/Signs/LF_Sign_PlateSmall.et";
export const PLATE_SIGN_COMPONENT_GUID = "{A19037297E81C6EC}";
export const HOUSE_COMPONENT_GUID = "{6E2F9A13B7D40C85}";
export const KEY_OWNED = "#LF-Everon-Lock_Private";
export const KEY_PRIVATE = "#LF-Everon-Lock_Private";
// A home: the purpose row it stands on, the resident who lives there (LF_Residents_Everon1989.conf) and the plate's wording. The teller's and
// the clerk's names are this card's invention (the pack names neither yet).
export const HOMES = [
  { row: "sp:b15", resident: "cafe_owner", key: "CafeOwner", plate: "Mr. O. Marchand", existing: "{place}.residents" },
  { row: "sp:b17", resident: "old_man", key: "OldMan", plate: "Mr. G. Lenoir" },
  { row: "sp:b117", resident: "night_shift", key: "NightShift", plate: "Mr. R. Duval" },
  { row: "sp:b50", resident: "teller", key: "Teller", plate: "Mrs. C. Girard" },
  { row: "sp:b09", resident: "store_clerk", key: "StoreClerk", plate: "Mr. P. Roche" },
];
const PLATE_LIFT = 1.4; // metres over the ground: a name plate sits at about door-handle height
const PLATE_SCALE = 0.5; // the vanilla 90 x 45 cm plate, halved
const DOOR_OUT = 1.5; // the door anchor stands this far in front of the plate
const BENCH_OUT = 3.2; // the bench anchor: further out on the street side, and to the side along the wall
const BENCH_SIDE = 1.5;
const SURVEYS = { sp: "", di: "-districts", dk: "-dock" };
const r3 = (n) => Math.round(n * 1000) / 1000;
const guid = (seed) => `{${createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase()}}`;
const entityName = (prefix, place, id) => `${prefix}_${place.replace(/-/g, "_")}_${id.replace(":", "_")}`;

// b15 already has the residents slot's House place, its marker and its anchors (one shared home, #3001) and stays as it is: the plate is all it gains
// and its resident names that place. Two Houses on one spot would tie in LF_ResidentAnchors, so it gets no second.
export const placeIdOf = (place, h) => (h.existing ? h.existing.replace("{place}", place) : `${place}.home.${h.resident}`);
export const titleKey = (h) => `LF-Sign_Home_${h.key}_Title`;
export const signId = (h) => `home_${h.resident}`;

// ---- the plate must hang on the OUTSIDE of the house (#3887). The drop planner seats a plate on the first wall its ray finds, and a ray that
// goes in through the doorway finds a room wall (b09's plate hung over the kitchen sink). A spot is taken only when nothing of the house's
// shell stands in front of it: three horizontal rays from the plate's face (straight out and 40 degrees either side) meet no wall within
// OUT_REACH. In a room they hit the opposite walls; on the street they leave the building behind.
export const OUT_REACH = 10;
export const OUT_SPREAD = 40;
export const OUT_STEP = 0.3; // the rays start this far in front of the plate's origin
const PLATE_BOX = { half: [0.23, 0.12, 0.03] }; // the scaled plate: about 45 x 22 cm, a few cm deep

// Segment/ray against one triangle, both faces (Moller-Trumbore): the distance along `d`, or null.
function rayTri(o, d, [a, b, c], max) {
  const sub = (p, q) => [p[0] - q[0], p[1] - q[1], p[2] - q[2]];
  const cross = (p, q) => [p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]];
  const dot = (p, q) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
  const e1 = sub(b, a);
  const e2 = sub(c, a);
  const h = cross(d, e2);
  const det = dot(e1, h);
  if (Math.abs(det) < 1e-9) return null;
  const s = sub(o, a);
  const u = dot(s, h) / det;
  if (u < -1e-4 || u > 1 + 1e-4) return null;
  const q = cross(s, e1);
  const v = dot(d, q) / det;
  if (v < -1e-4 || u + v > 1 + 1e-4) return null;
  const t = dot(e2, q) / det;
  return t > 1e-4 && t <= max ? t : null;
}

// Pure: the nearest wall triangle in front of a plate, or null when it faces open air. `walls` are the host shell's triangles in host-local
// space ({ p: [[x,y,z] x3] }); `host` { x, y, z, yaw } is where that shell stands; `coords` and `yaw` are the plate's world place.
export function wallInFront(walls, host, coords, yaw) {
  const [lx, lz] = toLocal(coords[0] - host.x, coords[2] - host.z, host.yaw ?? 0);
  let best = null;
  for (const off of [0, -OUT_SPREAD, OUT_SPREAD]) {
    const k = ((yaw + off - (host.yaw ?? 0)) * Math.PI) / 180;
    const d = [Math.sin(k), 0, Math.cos(k)];
    const k0 = ((yaw - (host.yaw ?? 0)) * Math.PI) / 180;
    const o = [lx + Math.sin(k0) * OUT_STEP, coords[1] - host.y, lz + Math.cos(k0) * OUT_STEP];
    for (const w of walls) {
      const t = rayTri(o, d, w.p, OUT_REACH);
      if (t !== null && (!best || t < best.t)) best = { t, angle: off };
    }
  }
  return best;
}

// Pure: the plate's oriented box in the audit's shape ({ c, h, a }), for the door swing and walk-up test.
export function plateBox(coords, yaw) {
  const k = (yaw * Math.PI) / 180;
  return { c: [coords[0], coords[1] + PLATE_BOX.half[1], coords[2]], h: PLATE_BOX.half, a: [[Math.cos(k), 0, -Math.sin(k)], [0, 1, 0], [Math.sin(k), 0, Math.cos(k)]] };
}

// Pure-ish: why a plate at `coords` is not on a street-side wall, or null when it is. `walls` null (no readable shell) judges by the doors alone.
export function plateProblem({ walls, host, doors = [], coords, yaw }) {
  const hit = walls ? wallInFront(walls, host, coords, yaw) : null;
  if (hit) return `a wall stands ${Math.round(hit.t * 100) / 100} m in front of it (${hit.angle} deg): it hangs inside the house`;
  const door = doorHit(plateBox(coords, yaw), doors);
  return door ? door.text : null;
}

// Pure: the layer text for one home: the marker that is the House place, the plate and the door anchor.
export function renderHome({ place, h, marker, plate }) {
  const id = h.row;
  const a = (plate.yaw * Math.PI) / 180;
  const door = [r3(plate.coords[0] + Math.sin(a) * DOOR_OUT), r3(plate.coords[1] - PLATE_LIFT), r3(plate.coords[2] + Math.cos(a) * DOOR_OUT)];
  const marked = h.existing ? [] : [
    `GenericEntity ${entityName("ResidentHome", place, id)} : "${MARKER_PREFAB}" {`,
    " components {",
    `  LF_BuildingLockComponent "${LOCK_COMPONENT_GUID}" {`,
    `   m_sSignKey "${KEY_PRIVATE}"`,
    `   m_sOwnedSignKey "${KEY_OWNED}"`,
    "  }",
    `  LF_HouseManagerComponent "${HOUSE_COMPONENT_GUID}" {`,
    "  }",
    `  LF_PlaceComponent "${guid(`resident-home-place:${placeIdOf(place, h)}`)}" {`,
    '   m_sKind "House"',
    `   m_sId "${placeIdOf(place, h)}"`,
    "  }",
    " }",
    ` coords ${marker.x} ${marker.y} ${marker.z}`,
    "}",
  ];
  const plated = [
    `GenericEntity ${entityName("ResidentPlate", place, id)} : "${PLATE_PREFAB}" {`,
    " components {",
    `  LF_SignComponent "${PLATE_SIGN_COMPONENT_GUID}" {`,
    `   m_sSignId "${signId(h)}"`,
    "   m_vRimColor 0.42 0.3 0.1",
    "   m_vPaintColor 0.72 0.55 0.22",
    "   m_vTextColor 0.1 0.07 0.03",
    "  }",
    " }",
    ` coords ${plate.coords.join(" ")}`,
    ` angleY ${plate.yaw}`,
    ` scale ${PLATE_SCALE}`,
    "}",
  ];
  // The `bench` anchor stands outside too: without one a resident sits at the house marker origin, inside the shell, and the walk out is a
  // 34 s stuck timeout (teller, 2026-11-19). b15 keeps the residents slot anchors (door and bench); the others get both outside.
  const benchAt = [r3(plate.coords[0] + Math.sin(a) * BENCH_OUT + Math.cos(a) * BENCH_SIDE), r3(plate.coords[1] - PLATE_LIFT), r3(plate.coords[2] + Math.cos(a) * BENCH_OUT - Math.sin(a) * BENCH_SIDE)];
  const benchstep = h.existing ? [] : [
    `GenericEntity ${entityName("ResidentBench", place, id)} {`,
    " components {",
    `  LF_ResidentAnchorComponent "${guid(`resident-home-bench:${id}`)}" {`,
    '   m_sAnchorId "bench"',
    "  }",
    " }",
    ` coords ${benchAt.join(" ")}`,
    ` angleY ${plate.yaw}`,
    "}",
  ];
  // b15 keeps its own door anchor (the residents slot placed it); the others get one on the doorstep.
  const doorstep = h.existing ? [] : [
    `GenericEntity ${entityName("ResidentDoor", place, id)} {`,
    " components {",
    `  LF_ResidentAnchorComponent "${guid(`resident-home-door:${id}`)}" {`,
    '   m_sAnchorId "door"',
    "  }",
    " }",
    ` coords ${door.join(" ")}`,
    ` angleY ${plate.yaw}`,
    "}",
  ];
  return [...marked, ...plated, ...doorstep, ...benchstep, ""].join(String.fromCharCode(10));
}

// Pure: the sign rows (one per plate): the title only; the card shows just the name.
export function renderSigns(homes) {
  const out = ["LF_SignConfig {", " m_aSigns {"];
  for (const h of homes) out.push(`  LF_SignDef "${guid(`resident-home-sign:${h.resident}`)}" {`, '   m_sOp "add"', `   m_sId "${signId(h)}"`, `   m_sTitle "#${titleKey(h)}"`, "  }");
  out.push(" }", "}", "");
  return out.join("\n");
}

// Pure: the string table with the plate titles appended (rows already there are left alone). Never rewrites a row.
export function withTitles(st, homes) {
  const add = homes.filter((h) => !st.includes(`Id "${titleKey(h)}"`));
  if (!add.length) return st;
  const rows = add.map((h) => `    CustomStringTableItem "${guid(`resident-home-title:${h.resident}`)}" {\n     Id "${titleKey(h)}"\n     Target_en_us "${h.plate}"\n     Comment "Brass name plate beside the door of ${h.resident}'s home on Saint-Philippe (Everon 1989 pack, #3233). Printed on the plate and shown in the prompt as Read: ${h.plate}."\n    }`);
  const at = st.search(/\n  \}\n \}\n\}\s*$/);
  if (at < 0) throw new Error("the pack string table does not end as expected");
  return `${st.slice(0, at)}\n${rows.join("\n")}${st.slice(at)}`;
}

export const metaText = (name) => `MetaFileClass {
 Name "${name}"
 Configurations {
  CONFResourceClass PC {
  }
  CONFResourceClass XBOX_ONE : PC {
  }
  CONFResourceClass XBOX_SERIES : PC {
  }
  CONFResourceClass PS4 : PC {
  }
  CONFResourceClass HEADLESS : PC {
  }
 }
}
`;

async function gameReader() {
  try {
    const vfs = await (await import("../lib/game-prefab.mjs")).gameVfs();
    return (rel) => (vfs.exists(rel) ? vfs.readTextFile(rel) : null);
  } catch {
    return null;
  }
}

const USAGE = "usage: tools\\cli town residenthomes <place> [--dry] [--out-dir d]";

export async function main(args, { root = ROOT, shell: ctxShell = null } = {}) {
  const flags = {};
  const pos = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--out-dir") flags["out-dir"] = args[++i];
    else if (args[i] === "--dry" || args[i] === "--help") flags[args[i].slice(2)] = true;
    else pos.push(args[i]);
  }
  if (flags.help || pos.length !== 1 || !SLUG_RX.test(pos[0])) {
    console.log(USAGE);
    return flags.help ? 0 : 2;
  }
  const place = pos[0];
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.dry && !flags["out-dir"] && !new RegExp(`^ws/town-draft-${place}-[a-z0-9-]+$`).test(branch ?? "")) {
    console.log(`REFUSE  town residenthomes writes an island layer only from a ws/town-draft-${place}-<word> worktree (this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }
  const purposeFile = join(root, "docs", "everon", "places", place, "purpose.json");
  if (!existsSync(purposeFile)) {
    console.log(`REFUSE  no docs/everon/places/${place}/purpose.json`);
    return 1;
  }
  const purpose = JSON.parse(readFileSync(purposeFile, "utf8"));
  const dossiers = {};
  for (const [prefix, suffix] of Object.entries(SURVEYS)) {
    const f = join(root, "docs", "everon", "places", `${place}${suffix}`, "dossier.json");
    if (existsSync(f)) dossiers[prefix] = JSON.parse(readFileSync(f, "utf8"));
  }
  const shell = ctxShell ?? shellReader(root);
  const readGame = await gameReader();

  const doors = loadDoors(root, place);
  const done = [];
  const refused = [];
  let layerText = "";
  for (const h of HOMES) {
    const row = (purpose.buildings ?? []).find((r) => r.id === h.row);
    if (!row || row.purpose !== "c") {
      refused.push(`${h.row}: not a purpose "c" row of ${place}`);
      continue;
    }
    const dossier = dossiers[h.row.split(":")[0]];
    const building = dossier?.buildings?.find((b) => b.id === row.building);
    const fp = building?.footprint ?? {};
    const marker = building ? { x: fp.cx ?? building.x, y: building.y, z: fp.cz ?? building.z } : null;
    if (!marker || ![marker.x, marker.y, marker.z].every(Number.isFinite)) {
      refused.push(`${h.row}: no footprint centre or ground height in the dossier`);
      continue;
    }
    // The entrance side first; a building with no known entrance, or one whose wall takes no plate, tries its other sides.
    let plan = null;
    let e = null;
    // A wall spot is taken only when it is on the street side of the house and clear of every door's swing and walk-up (#3887).
    const walls = (await shell.shell(building.prefab))?.walls ?? null;
    const accept = (ent) => {
      const k = (ent.yaw * Math.PI) / 180;
      const at = [r3(ent.coords[0] + Math.sin(k) * 0.03), ent.coords[1], r3(ent.coords[2] + Math.cos(k) * 0.03)];
      return !plateProblem({ walls, host: building, doors, coords: at, yaw: ent.yaw });
    };
    for (const turn of [0, 1, 3, 2]) {
      plan = await planDrop({ drop: { place: dossier.place.slug, building: row.building, turn, lift: PLATE_LIFT }, slotId: "resident-homes", dossier, root, shell, readGame, kitName: "resident-plate", accept });
      e = plan?.ok ? plan.ents?.rowEnts?.[0] : null;
      if (e && plan.seat?.rows?.[0]?.wall) break;
      e = null;
    }
    if (!e) {
      refused.push(`${h.row}: the plate could not be seated on a wall${plan?.errors?.length ? ` (${plan.errors.join("; ")})` : ""}`);
      continue;
    }
    const a = (e.yaw * Math.PI) / 180;
    const coords = [r3(e.coords[0] + Math.sin(a) * 0.03), e.coords[1], r3(e.coords[2] + Math.cos(a) * 0.03)];
    layerText += renderHome({ place, h, marker, plate: { coords, yaw: e.yaw } });
    done.push(h);
    console.log(`home  ${h.row} ${placeIdOf(place, h)} "${h.plate}": plate ${coords.join(" ")} yaw ${e.yaw}`);
  }
  for (const r of refused) console.log(`REFUSE  ${r}`);

  const outRoot = resolve(root, flags["out-dir"] ?? ".");
  const layerFile = join(outRoot, LAYER_DIR, `Homes_${place}_resident.layer`);
  const signFile = join(outRoot, SIGN_DIR, "LF_Signs_ResidentHomes.conf");
  const stFile = join(outRoot, ST_FILE);
  const homesFile = join(outRoot, "docs", "everon", "places", place, "resident-homes.json");
  const shown = (f) => relative(outRoot, f).replaceAll("\\", "/");
  if (flags.dry) {
    console.log(`SUMMARY town-residenthomes dry ${place}: ${done.length} of ${HOMES.length} home(s); would write ${shown(layerFile)}, ${shown(signFile)}, ${shown(homesFile)}`);
    return refused.length ? 1 : 0;
  }
  if (refused.length) {
    console.log(`SUMMARY town-residenthomes fail ${place}: ${refused.length} home(s) refused, nothing written`);
    return 1;
  }
  for (const f of [layerFile, signFile, homesFile]) mkdirSync(dirname(f), { recursive: true });
  writeFileSync(layerFile, layerText);
  writeFileSync(signFile, renderSigns(done));
  writeFileSync(`${signFile}.meta`, metaText(`${guid("resident-homes-sign-conf")}Configs/Signs/LF_Signs_Everon1989/LF_Signs_ResidentHomes.conf`));
  if (existsSync(stFile)) writeFileSync(stFile, withTitles(readFileSync(stFile, "utf8"), done));
  writeFileSync(homesFile, `${JSON.stringify({ schema: 1, place, homes: done.map((h) => ({ id: h.row, resident: h.resident, placeId: placeIdOf(place, h) })) }, null, 1)}\n`);
  console.log(`SUMMARY town-residenthomes ok ${place}: ${done.length} home(s), ${shown(layerFile)}, ${shown(signFile)}`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
