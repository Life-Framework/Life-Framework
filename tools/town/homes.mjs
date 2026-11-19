// tools/town/homes.mjs - `tools\cli town homes <place>`: every home for sale of a place gets a house, a sign, a price and an unlocked door (#3584).
//
//   town homes <place> [--dry] [--only sp:b01,di:b05] [--no-audit] [--out-dir d]
//
// Reads  docs/everon/places/<place>/purpose.json   the allocation: a row with purpose "a", a houseId and a tier is a home for sale;
//        the place's dossiers (<place>, <place>-districts, <place>-dock) for each building's footprint centre, ground height and entrance.
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Homes_<place>.layer
//          per home: a house marker (Prefabs/Systems/Houses/LF_HouseMarker.et: the lock marker plus LF_HouseManagerComponent, whose
//          m_sDebugIdentifier is the houseId, so the owner and the key pass the doors and the sale saves under the id) at the footprint centre,
//          and LF_HouseSaleSign.et with the same houseId on the entrance wall, seated by the drop planner (tools/town/drops.mjs);
//        addons/LF_Everon1989/Configs/Property/LF_Property_Everon1989/LF_Property_Homes_<Place>.conf (+ .meta)
//          per home: an `m_aHouses` row (tier label and price by tier, TIERS below), in its own file of the overlay folder (#3555);
//        docs/everon/places/<place>/homes.json   the homes done; `town locks` reads it and drops their ForSaleSoon lock marker.
// A home whose sign cannot be seated is left out whole (it keeps its ForSaleSoon lock), never half done. Rerun the script, never hand-edit.
// Branch rule as `town drops`: the island layer is written only from ws/town-draft-<place>-homes (or --dry / --out-dir).
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { planDrop } from "./drops.mjs";
import { main as locksMain } from "./locks.mjs";
import { shellReader } from "./seat.mjs";

export const help = "homes <place> [--dry] [--only ids] [--no-audit]   the homes for sale of a place: a house marker (owner and key pass the doors), a For sale sign on the entrance wall, a price row in the pack's property overlay, no ForSaleSoon lock; writes the layer, the overlay file and homes.json";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const CONF_DIR = "addons/LF_Everon1989/Configs/Property/LF_Property_Everon1989";
export const MARKER_PREFAB = "{4D1B7E92A6C03F58}Prefabs/Systems/Houses/LF_HouseMarker.et";
export const LOCK_COMPONENT_GUID = "{A3F1C5D7092E4B68}";
export const HOUSE_COMPONENT_GUID = "{6E2F9A13B7D40C85}";
export const SIGN_PREFAB = "{2F815241C89039BC}Prefabs/Props/Property/LF_HouseSaleSign.et";
export const SALE_COMPONENT_GUID = "{59380F11830D8A96}";
export const KEY_FOR_SALE = "#LF-Everon-Lock_ForSale";
export const KEY_OWNED = "#LF-Everon-Lock_Private";
// Price by the allocation's size tier. A courier earns about $600 an hour (`tools\cli economy`) and the cheapest plot is $2800, so the
// first cottage is a few hours of play and a farmhouse a weekend; a villa is the long goal.
export const TIERS = {
  T1: { label: "Cottage", price: 4000 },
  T2: { label: "Village house", price: 6000 },
  T3: { label: "Family house", price: 8000 },
  T4: { label: "Town house", price: 10000 },
  T5: { label: "Farmhouse", price: 13000 },
  T6: { label: "Villa", price: 20000 },
};
const SURVEYS = { sp: "", di: "-districts", dk: "-dock" };
const r3 = (n) => Math.round(n * 1000) / 1000;
const guid = (seed) => `{${createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase()}}`;
const entityName = (prefix, place, id) => `${prefix}_${place.replace(/-/g, "_")}_${id.replace(":", "_")}`;
const camel = (place) => place.split("-").map((w) => w[0].toUpperCase() + w.slice(1)).join("");

// Pure: the homes of a purpose file: a row with purpose "a", a houseId and a known tier.
export function homeRows(purpose) {
  return (purpose.buildings ?? []).filter((r) => r.purpose === "a" && r.houseId && TIERS[r.tier]);
}

// Pure: the layer text for one home: the house marker, then the sign.
export function renderHome({ place, row, marker, sign }) {
  const id = row.id;
  return [
    `GenericEntity ${entityName("Home", place, id)} : "${MARKER_PREFAB}" {`,
    " components {",
    `  LF_BuildingLockComponent "${LOCK_COMPONENT_GUID}" {`,
    `   m_sSignKey "${KEY_FOR_SALE}"`,
    `   m_sOwnedSignKey "${KEY_OWNED}"`,
    "  }",
    `  LF_HouseManagerComponent "${HOUSE_COMPONENT_GUID}" {`,
    `   m_sDebugIdentifier "${row.houseId}"`,
    "  }",
    " }",
    ` coords ${marker.x} ${marker.y} ${marker.z}`,
    "}",
    `GenericEntity ${entityName("HomeSign", place, id)} : "${SIGN_PREFAB}" {`,
    " components {",
    `  LF_HouseSaleComponent "${SALE_COMPONENT_GUID}" {`,
    `   m_sHouseId "${row.houseId}"`,
    "  }",
    " }",
    ` coords ${sign.coords.join(" ")}`,
    ` angleY ${sign.yaw}`,
    "}",
    "",
  ].join("\n");
}

// Pure: the overlay file: one m_aHouses row per home, added by house id.
export function renderOverlay(rows) {
  const out = ["LF_PropertyConfig {", " m_aHouses {"];
  for (const r of rows) {
    const t = TIERS[r.tier];
    out.push(`  LF_PropertyHouseDef "${guid(`home-row:${r.houseId}`)}" {`, '   m_sOp "add"', `   m_sHouseId "${r.houseId}"`, `   m_sTierLabel "${t.label}"`, `   m_iPrice ${t.price}`, "  }");
  }
  out.push(" }", "}", "");
  return out.join("\n");
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

function parseArgs(args) {
  const flags = {};
  const pos = [];
  const bad = [];
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--out-dir" || a === "--only") flags[a.slice(2)] = args[++i];
    else if (["--dry", "--no-audit", "--help"].includes(a)) flags[a.slice(2)] = true;
    else if (a.startsWith("--")) bad.push(a);
    else pos.push(a);
  }
  return { flags, pos, bad };
}

const USAGE = "usage: tools\\cli town homes <place> [--dry] [--only sp:b01,di:b05] [--no-audit] [--out-dir d]";

export async function main(args, { root = ROOT, shell: ctxShell = null } = {}) {
  const { flags, pos, bad } = parseArgs(args);
  if (flags.help || bad.length || pos.length !== 1 || !SLUG_RX.test(pos[0])) {
    console.log(bad.length ? `unknown flag ${bad.join(", ")}\n${USAGE}` : USAGE);
    return flags.help ? 0 : 2;
  }
  const place = pos[0];
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.dry && !flags["out-dir"] && branch !== `ws/town-draft-${place}-homes`) {
    console.log(`REFUSE  town homes writes an island layer only from the worktree ws/town-draft-${place}-homes (this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }
  const placeDir = join(root, "docs", "everon", "places", place);
  if (!existsSync(join(placeDir, "purpose.json"))) {
    console.log(`REFUSE  no docs/everon/places/${place}/purpose.json`);
    return 1;
  }
  const purpose = JSON.parse(readFileSync(join(placeDir, "purpose.json"), "utf8"));
  const dossiers = {};
  for (const [prefix, suffix] of Object.entries(SURVEYS)) {
    const f = join(root, "docs", "everon", "places", `${place}${suffix}`, "dossier.json");
    if (existsSync(f)) dossiers[prefix] = JSON.parse(readFileSync(f, "utf8"));
  }
  let rows = homeRows(purpose);
  if (flags.only) {
    const want = new Set(String(flags.only).split(",").map((s) => s.trim()));
    rows = rows.filter((r) => want.has(r.id));
  }
  const shell = ctxShell ?? shellReader(root);
  const readGame = await gameReader();

  const done = [];
  const refused = [];
  let layerText = "";
  for (const row of rows) {
    const prefix = row.id.split(":")[0];
    const dossier = dossiers[prefix];
    const building = dossier?.buildings?.find((b) => b.id === row.building);
    const fp = building?.footprint ?? {};
    const marker = building ? { x: fp.cx ?? building.x, y: building.y, z: fp.cz ?? building.z } : null;
    if (!marker || ![marker.x, marker.y, marker.z].every(Number.isFinite)) {
      refused.push(`${row.id}: no footprint centre or ground height in the dossier`);
      continue;
    }
    // The entrance side first; a building whose entrance wall takes no sign (a porch, a doorway the whole width) tries its other sides.
    let plan = null;
    let e = null;
    for (const turn of [0, 1, 3, 2]) {
      plan = await planDrop({ drop: { place: dossier.place.slug, building: row.building, turn }, slotId: "homes", dossier, root, shell, readGame, kitName: "house-sale" });
      e = plan?.ok ? plan.ents?.rowEnts?.[0] : null;
      if (e && plan.seat?.rows?.[0]?.wall) break;
      e = null;
    }
    if (!e) {
      refused.push(`${row.id}: the sign could not be seated on a wall${plan?.errors?.length ? ` (${plan.errors.join("; ")})` : ""}`);
      continue;
    }
    // Off the wall by a hair along the sign's facing, as a drop (the wall it was seated on is behind it).
    const a = (e.yaw * Math.PI) / 180;
    const coords = [r3(e.coords[0] + Math.sin(a) * 0.05), e.coords[1], r3(e.coords[2] + Math.cos(a) * 0.05)];
    layerText += renderHome({ place, row, marker, sign: { coords, yaw: e.yaw } });
    done.push(row);
    console.log(`home  ${row.id} ${row.houseId} ${TIERS[row.tier].label} $${TIERS[row.tier].price}: sign ${coords.join(" ")} yaw ${e.yaw}`);
  }
  for (const r of refused) console.log(`REFUSE  ${r}`);

  const outRoot = resolve(root, flags["out-dir"] ?? ".");
  const layerFile = join(outRoot, LAYER_DIR, `Homes_${place}.layer`);
  const confName = `LF_Property_Homes_${camel(place)}.conf`;
  const confFile = join(outRoot, CONF_DIR, confName);
  const homesFile = join(outRoot, "docs", "everon", "places", place, "homes.json");
  const shown = (f) => relative(outRoot, f).replaceAll("\\", "/");
  if (flags.dry) {
    console.log(`SUMMARY town-homes dry ${place}: ${done.length} of ${rows.length} home(s); would write ${shown(layerFile)}, ${shown(confFile)}, ${shown(homesFile)}`);
    return refused.length ? 1 : 0;
  }
  for (const f of [layerFile, confFile, homesFile]) mkdirSync(dirname(f), { recursive: true });
  writeFileSync(layerFile, layerText);
  writeFileSync(confFile, renderOverlay(done));
  writeFileSync(`${confFile}.meta`, metaText(`${guid(`home-conf:${place}`)}Configs/Property/LF_Property_Everon1989/${confName}`));
  const homes = done.map((r) => ({ id: r.id, houseId: r.houseId, tier: r.tier, price: TIERS[r.tier].price }));
  writeFileSync(homesFile, `${JSON.stringify({ schema: 1, place, homes }, null, 1)}\n`);
  // The homes that are done lose their ForSaleSoon lock marker.
  const lockCode = locksMain([place], { root: outRoot });
  let ok = refused.length === 0 && lockCode === 0;
  const steps = [];
  if (!flags["no-audit"] && !flags["out-dir"]) {
    const cli = join(root, "tools", process.platform === "win32" ? "cli.cmd" : "cli");
    const layerName = `Homes_${place}.layer`;
    const r = spawnSync(cli, ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", layerName, "--hard-layer", layerName], { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    steps.push(`audit ${r.status === 0 ? "ok" : "fail"}`);
    if (r.status !== 0) {
      console.log(String(r.stdout ?? "").split("\n").filter((l) => /FAIL|REFUSE|ERROR|SUMMARY/.test(l)).slice(0, 20).join("\n"));
      ok = false;
    }
  }
  console.log(`SUMMARY town-homes ${ok ? "ok" : "fail"} ${place}: ${done.length} of ${rows.length} home(s), ${shown(layerFile)}, ${shown(confFile)}${steps.length ? `; ${steps.join(", ")}` : "; audit skipped"}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
