// tools/town/author.mjs - `tools\cli town author`: make a shop in the grid around a copy of the island building, then lift it as a
// building-local kit that `town stamp` writes onto every mapped copy (card #3881, docs/architecture/placement-system.md
// "Author in the grid, stamp into the world"; docs/starter-packs/stamp.md).
//
//   town author <kit> [--from <place>/<slot>] [--slug s] [--dry]
//       claims a pack grid cell (`grid place <slug> <building prefab> --pack`) holding the SAME vanilla building the island slot
//       stands in (the dossier's prefab with --from, else the kit's stamp.prefab / building.vanilla), and seeds it with the kit's
//       rows at the building's origin and yaw (`kit-grid.mjs put`) when the kit exists. Then look, walk and audit the cell.
//   town author extract <Grid_<slug>.layer> <group> --kit <name> [--building <EntityName>] [--clear <glob,...>] [--system s] [--out f]
//       writes tools/town/kits/<name>.json in the building's own frame: offsets and yaw relative to the building entity of the
//       grid layer (its height too: the same prefab has the same floors), each row's attach target, the kit's clear lanes and
//       clear list, and a `stamp` block naming the building prefab. No world coordinate is in the result.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { checkKit, entitiesOf, extractKit, KITS_DIR, parseBlocks } from "./kit.mjs";
import { kitPrefab } from "./stamp.mjs";

export const help = "author <kit> [--from <place>/<slot>] | author extract <Grid layer> <group> --kit k   build a shop in a pack grid cell around a copy of the island building, then lift it building-local for `town stamp`";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r3 = (n) => Math.round(n * 1000) / 1000;
// A vanilla building in a grid layer: a Structures prefab that is not the cell's fence, wall or sign.
const BUILDING_RX = /\/Prefabs\/Structures\/(?!Walls\/|Fences\/|Signs\/)/i;
export const LANE_WIDTH = 1.2;

// The building entity a grid layer's kit is relative to: the named one, else the one Structures prefab in the layer.
export function findBuilding(ents, name = null) {
  if (name) {
    const hit = ents.find((e) => e.name === name);
    if (!hit) throw new Error(`no entity ${name} in the grid layer`);
    return hit;
  }
  const hits = ents.filter((e) => e.prefab && BUILDING_RX.test("/" + e.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, "")));
  if (hits.length !== 1) throw new Error(`the grid layer holds ${hits.length} building prefabs (${hits.map((e) => e.name).join(", ") || "none"}); name one with --building`);
  return hits[0];
}

// The surface a row sits on, from its verb and the role of the row it names.
export function attachOf(row, rows) {
  if (row.verb === "on floor") return "floor";
  if (row.verb === "against wall") return "wall";
  const host = rows.find((o) => o.id === row.verb.slice(3));
  const kind = host?.role === "counter" || host?.role === "till" ? "counter" : /shelf/i.test(host?.prefab ?? "") ? "shelf" : "table";
  return `${kind}:${row.verb.slice(3)}`;
}

// Pure but for the file reads: the grid layer -> a building-local kit with attach targets, clear lanes and the clear list.
export function authorExtract(layerPath, group, { kit, building = null, clear = [], system = "" } = {}) {
  const ents = entitiesOf(parseBlocks(readFileSync(layerPath, "utf8")));
  const b = findBuilding(ents, building);
  if (!b.name) throw new Error("the building entity has no name; name it in the grid layer");
  const out = extractKit(layerPath, group, { kit, system, origin: b.name, exclude: b.name, inside: b.prefab });
  // extractKit rebases a layer kit onto its lowest row; a stamp needs the height in the building's frame (the same prefab has the
  // same floors), so every offset is put back relative to the building origin.
  const picked = new Map(ents.map((e) => [e.name, e]));
  for (const r of out.rows) {
    const e = picked.get(r.name);
    if (e) r.offset[1] = r3(e.coords[1] - b.coords[1]);
  }
  const floorY = Math.min(...out.rows.filter((r) => r.floor === "f0" || !r.floor).map((r) => r.offset[1]));
  out.anchoring.floorY = r3(Number.isFinite(floorY) ? floorY : 0);
  for (const f of out.anchoring.floors) f.y = r3(out.anchoring.floorY + 3 * Number(f.id.slice(1)));
  for (const r of out.rows) r.attach = attachOf(r, out.rows);
  out.source.origin = "building";
  out.source.frame = "building-local";
  const tills = out.rows.filter((r) => r.role === "till" || r.role === "counter").map((r) => r.id);
  const faces = out.rows.filter((r) => /shelf/i.test(r.prefab ?? "")).map((r) => r.id);
  out.stamp = {
    schema: 1,
    prefab: b.prefab,
    // Lanes the audit keeps free (card #3883 measures them): entrance to every till and every shelf face, 1.2 m wide.
    lanes: [...tills, ...faces].map((to) => ({ from: "entrance", to, width: LANE_WIDTH })),
    // Vanilla clutter the building prefab brings that this shop does not want: `town stamp` writes one clear carrier per copy
    // (LF_BaseWorldClearComponent) that hides it at load (#3882, #3924).
    clear: clear.map((prefab) => ({ prefab })),
  };
  return out;
}

// Where `town author` builds: the island building's prefab (--from) or the kit's own, and the cell slug.
export function planAuthor(kitName, { from = null, placesDir, templatesDir, kitsDir = KITS_DIR, slug = null }) {
  const kitFile = join(kitsDir, `${kitName}.json`);
  const kit = existsSync(kitFile) ? JSON.parse(readFileSync(kitFile, "utf8")) : null;
  let prefab = kit ? kitPrefab(kit) : null;
  let source = kit ? `kit ${kitName}` : null;
  if (from) {
    const [place, slot] = from.split("/");
    const mf = join(placesDir, place ?? "", "mapping.json");
    if (!place || !slot || !existsSync(mf)) throw new Error(`--from ${from}: no mapping for ${place} (want <place>/<slot>)`);
    const mapping = JSON.parse(readFileSync(mf, "utf8"));
    const entry = mapping.slots?.[slot];
    if (!entry?.building) throw new Error(`--from ${from}: the slot is not mapped to a building`);
    const dossier = JSON.parse(readFileSync(join(placesDir, place, "dossier.json"), "utf8"));
    const b = (dossier.buildings ?? []).find((x) => x.id === entry.building);
    if (!b?.prefab) throw new Error(`--from ${from}: building ${entry.building} has no prefab in the dossier`);
    if (prefab && prefab !== b.prefab) throw new Error(`--from ${from}: the island building is ${b.prefab}, the kit was authored around ${prefab}`);
    prefab = b.prefab;
    source = `${place}/${slot} (${b.id})`;
    const template = JSON.parse(readFileSync(join(templatesDir, `${mapping.template}.json`), "utf8"));
    const s = template.slots.find((x) => x.id === slot);
    if (s && s.kit !== kitName) throw new Error(`--from ${from}: that slot takes kit ${s.kit}, not ${kitName}`);
  }
  if (!prefab) throw new Error(`kit ${kitName}: no building prefab known; pass --from <place>/<slot>`);
  return { kit, kitFile, prefab, source, slug: slug ?? `author-${kitName}`, seed: Boolean(kit) };
}

function cli(root, args) {
  const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), ...args], { cwd: root, encoding: "utf8", timeout: 600000, windowsHide: true });
  process.stdout.write(r.stdout ?? "");
  return r.status ?? 1;
}

function parseArgs(args) {
  const o = { _: [] };
  const valued = new Set(["from", "slug", "kit", "building", "clear", "system", "out", "places-dir", "templates-dir"]);
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      o[k] = valued.has(k) ? args[++i] : true;
    } else o._.push(a);
  }
  return o;
}

export async function main(args, { root = ROOT } = {}) {
  const a = parseArgs(args);
  if (a._[0] === "extract") {
    const [, layer, group] = a._;
    if (!layer || !group || !a.kit) {
      console.log("usage: town author extract <Grid_<slug>.layer> <group> --kit <name> [--building Name] [--clear globs] [--out f]");
      return 2;
    }
    const kit = authorExtract(resolve(layer), group, { kit: a.kit, building: a.building ?? null, clear: String(a.clear ?? "").split(",").filter(Boolean), system: a.system ?? "" });
    const errs = checkKit(kit, { root });
    if (errs.length) {
      console.log(errs.join("\n"));
      return 1;
    }
    const out = resolve(a.out ?? join(root, "tools", "town", "kits", `${kit.kit}.json`));
    mkdirSync(dirname(out), { recursive: true });
    writeFileSync(out, JSON.stringify(kit, null, 2) + "\n");
    console.log(`kit ${kit.kit}: ${kit.rows.length} rows, building-local around ${kit.stamp.prefab} -> ${relative(root, out)}\nnext: tools\\cli town stamp ${kit.kit} --dry`);
    return 0;
  }
  if (!a._[0] || a.help) {
    console.log("usage: tools\\cli town author <kit> [--from <place>/<slot>] [--slug s] [--dry] | town author extract ... (tools/town/author.mjs header)");
    return a.help ? 0 : 2;
  }
  const plan = planAuthor(a._[0], { from: a.from ?? null, slug: a.slug ?? null, placesDir: resolve(root, a["places-dir"] ?? "docs/everon/places"), templatesDir: resolve(root, a["templates-dir"] ?? "tools/town/templates"), kitsDir: join(root, "tools", "town", "kits") });
  console.log(`town author ${a._[0]}: grid cell ${plan.slug} around ${plan.prefab} (from ${plan.source})${plan.seed ? ", seeded with the kit's rows" : ""}`);
  if (a.dry) return 0;
  if (cli(root, ["grid", "place", plan.slug, plan.prefab, "--pack"])) return 1;
  if (plan.seed) {
    const { layerFileOf } = await import("./kit-grid.mjs");
    const ents = entitiesOf(parseBlocks(readFileSync(layerFileOf(plan.slug), "utf8")));
    const b = findBuilding(ents);
    const r = spawnSync(process.execPath, [join(root, "tools", "town", "kit-grid.mjs"), "put", a._[0], plan.slug, String(b.coords[0]), String(b.coords[2]), String(b.yaw)], { cwd: root, encoding: "utf8", windowsHide: true });
    process.stdout.write(r.stdout ?? "");
    if (r.status) return 1;
  }
  console.log(`next: tools\\cli look grid:${plan.slug} --interior auto; tools\\cli audit placement --grid --strict; then town author extract <the cell's layer> '<group>' --kit ${a._[0]}`);
  return 0;
}
