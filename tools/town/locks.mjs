// tools/town/locks.mjs - `node tools/town/locks.mjs <place> [--check]`: the island lock layer of a place (#3204).
//
// Reads  docs/everon/places/<place>/purpose.json (one row per enterable building: purpose a-e, use, status, card, partOf)
//        and the place's dossiers (<place>, <place>-districts, <place>-dock) for each building's footprint centre (cx, cz) and ground height (y).
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Locks_<place>.layer: one LF_BuildingLock.et per building that no other
// carrier covers, at its footprint centre, with the sign key for what the building is or will be. The marker is an invisible system
// prefab (#3203): every door of the building refuses to open and the prompt says why. A fitted place's open marker or a house marker
// beats the lock, so a slot card that fits a locked shop later needs no edit here.
// A generated file: rerun the script, never hand-edit. `--check` writes nothing and exits 1 when the file on disk differs.
//
// Skipped (they carry themselves): status exists/flight (a fitted slot), card D1 (church and chapels stay open), a row whose parent is such a
// building (`partOf`: a side shed of a fitted business), and SKIP below (sp:b49 is the org registry, its own card).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const PREFAB = "{9D4A2E6B1F83C570}Prefabs/Systems/Houses/LF_BuildingLock.et";
export const COMPONENT_GUID = "{5C81D0A7E3F2B469}"; // LF_BuildingLockComponent in the prefab: overridden by GUID
export const SKIP = new Set(["sp:b49"]);
const SURVEYS = { sp: "", di: "-districts", dk: "-dock" };

// Sign keys the pack words (addons/LF_Everon1989/Language, rule 7). Same order as the table in docs/features/houses.md.
export const KEYS = {
  private: "LF-Everon-Lock_Private", // homes, barns, lock-ups, outbuildings
  staff: "LF-Everon-Lock_StaffOnly", // fire station, co-op warehouse, dock warehouses, garage behind the gun counter
  forSale: "LF-Everon-Lock_ForSaleSoon", // the homes for sale, until their sale card lands
  opening: "LF-Everon-Lock_OpeningSoon", // planned shops
  military: "LF-Everon-Lock_Military", // dock barracks, guard house
  school: "LF-Everon-Lock_School", // school in summer holidays
  toLet: "LF-Everon-Lock_ToLetSoon", // dock workshops for the fishing chapter
  closed: "LF-Everon-Lock_Closed", // harbour company offices
};

// The sign key for a purpose row, or null when the row needs no lock. `byId` finds a row's parent.
export function keyFor(row, byId) {
  if (SKIP.has(row.id)) return null;
  if (row.status === "exists" || row.status === "flight") return null;
  if (row.card === "D1") return null;
  const parent = row.partOf ? byId.get(row.partOf) : null;
  if (parent && (parent.status === "exists" || parent.status === "flight")) return null; // a side building of a fitted business
  const use = String(row.use ?? "");
  if (/staff only/i.test(use)) return KEYS.staff;
  if (/zone militaire|acc.s r.serv/i.test(use)) return KEYS.military;
  if (/vacances|\bschool\b/i.test(use)) return KEYS.school;
  if (/louer/i.test(use)) return KEYS.toLet;
  if (/ferm./i.test(use) && !/fermer/i.test(use)) return KEYS.closed;
  if (row.partOf) return row.purpose === "b" ? KEYS.opening : KEYS.private; // a shed sold with its house stays private
  if (row.purpose === "a") return KEYS.forSale;
  if (row.purpose === "b") return KEYS.opening;
  return KEYS.private; // c resident homes, e outbuildings, any other civic building
}

export function loadPlace(root, place) {
  const purposeFile = join(root, "docs/everon/places", place, "purpose.json");
  if (!existsSync(purposeFile)) throw new Error(`no docs/everon/places/${place}/purpose.json`);
  const purpose = JSON.parse(readFileSync(purposeFile, "utf8"));
  const dossier = new Map(); // id -> { cx, cz, y }
  for (const [prefix, suffix] of Object.entries(SURVEYS)) {
    const f = join(root, "docs/everon/places", `${place}${suffix}`, "dossier.json");
    if (!existsSync(f)) continue;
    for (const b of JSON.parse(readFileSync(f, "utf8")).buildings ?? []) {
      const fp = b.footprint ?? {};
      dossier.set(`${prefix}:${b.id}`, { cx: fp.cx ?? b.x, cz: fp.cz ?? b.z, y: b.y, stem: b.stem ?? "" });
    }
  }
  return { purpose, dossier };
}

// -> { locks: [{ id, key, x, y, z, use }], errors }
export function buildLocks({ purpose, dossier }) {
  const rows = purpose.buildings ?? [];
  const byId = new Map(rows.map((r) => [r.id, r]));
  const locks = [];
  const errors = [];
  for (const r of rows) {
    const key = keyFor(r, byId);
    if (!key) continue;
    const d = dossier.get(r.id);
    if (!d || ![d.cx, d.cz, d.y].every(Number.isFinite)) {
      errors.push(`${r.id}: no footprint centre or ground height in the dossier`);
      continue;
    }
    locks.push({ id: r.id, key, x: d.cx, y: d.y, z: d.cz, use: String(r.use ?? "") });
  }
  return { locks, errors };
}

const entityName = (place, id) => `Lock_${place.replace(/-/g, "_")}_${id.replace(":", "_")}`;

// The layer text. One entity per lock, component overridden by the prefab's own GUID.
export function renderLayer(place, locks) {
  const out = [];
  for (const l of locks) {
    out.push(
      `GenericEntity ${entityName(place, l.id)} : "${PREFAB}" {`,
      " components {",
      `  LF_BuildingLockComponent "${COMPONENT_GUID}" {`,
      `   m_sSignKey "#${l.key}"`,
      "  }",
      " }",
      ` coords ${l.x} ${l.y} ${l.z}`,
      "}",
    );
  }
  return out.join("\n") + "\n";
}

export const layerPath = (root, place) => join(root, LAYER_DIR, `Locks_${place}.layer`);

export function main(args, { root = ROOT } = {}) {
  const check = args.includes("--check");
  const place = args.find((a) => !a.startsWith("--"));
  if (!place || !/^[a-z0-9][a-z0-9-]*$/.test(place)) {
    console.log("usage: node tools/town/locks.mjs <place> [--check]");
    return 1;
  }
  let loaded;
  try {
    loaded = loadPlace(root, place);
  } catch (e) {
    console.log(`locks: ${e.message}`);
    return 1;
  }
  const { locks, errors } = buildLocks(loaded);
  for (const e of errors) console.log(`ERROR ${e}`);
  const text = renderLayer(place, locks);
  const file = layerPath(root, place);
  const counts = {};
  for (const l of locks) counts[l.key] = (counts[l.key] ?? 0) + 1;
  const summary = Object.entries(counts).map(([k, n]) => `${k.replace("LF-Everon-Lock_", "")}=${n}`).join(" ");
  if (check) {
    const same = existsSync(file) && readFileSync(file, "utf8") === text;
    console.log(`SUMMARY locks ${same && !errors.length ? "ok" : "fail"} place=${place} locks=${locks.length} ${summary}${same ? "" : " (layer on disk differs: rerun node tools/town/locks.mjs " + place + ")"}`);
    return same && !errors.length ? 0 : 1;
  }
  writeFileSync(file, text);
  console.log(`SUMMARY locks ${errors.length ? "fail" : "ok"} place=${place} locks=${locks.length} ${summary} wrote=${LAYER_DIR}/Locks_${place}.layer`);
  return errors.length ? 1 : 0;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) process.exit(main(process.argv.slice(2)));
