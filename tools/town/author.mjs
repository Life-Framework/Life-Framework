// tools/town/author.mjs - `tools\cli town author`: make a shop in the grid around a copy of the island building, then lift it as a
// building-local kit that `town stamp` writes onto every mapped copy (card #3881, docs/architecture/placement-system.md
// "Author in the grid, stamp into the world"; docs/starter-packs/stamp.md).
//
//   town author <kit> [--from <place>/<slot>] [--slug s] [--margin m] [--dry]
//       claims an AUTHORING BAY (a 2 x 2 block at the east end of the lattice, `grid place <slug> <building prefab> --pack --bay [--margin m]`; --margin is the walk-around yard in metres;
//       tools/grid/lattice.mjs AUTHORING) holding the SAME vanilla building the island slot stands in (the dossier's prefab with
//       --from, else the kit's stamp.prefab / building.vanilla), and seeds it with the kit's rows at the building's origin and
//       yaw (`kit-grid.mjs put`) when the kit exists. Then look, walk and audit the cell. Only authoring takes a bay.
//   town author --reopen <kit> [--slug s] [--dry]
//       the kit is the source of truth, a bay is only scratch space: this builds the bay again from the extracted kit
//       (the kit's building prefab, every row put back) when someone needs to edit the shop. Extract releases it again.
//   town author extract <Grid_<slug>.layer> <group> --kit <name> [--building <EntityName>] [--clear <glob,...>] [--system s] [--out f] [--keep-bay] [--fresh]
//       writes tools/town/kits/<name>.json in the building's own frame: offsets and yaw relative to the building entity of the
//       grid layer (its height too: the same prefab has the same floors), each row's attach target, the kit's clear lanes and
//       clear list, and a `stamp` block naming the building prefab. No world coordinate is in the result. When the layer is an
//       authoring bay's, the kit is saved in tools/town/kits/ and checks, the bay is RELEASED (`grid free`: layer and claim
//       deleted) so the next shop can use it; `--keep-bay` keeps it. A row's component config (till stock id, sign def, clerk and host settings, with the
//       component GUIDs) is read from the layer row; a row the grid holds as a bare prop takes the config of the same row in the kit already on
//       disk (matched by id, a shared id suffix, or the one prefab), and that kit's source files stay the kit's `also` sources for the nested
//       blocks. `--fresh` skips it.
//       A reopened bay holds each row with its component blocks and a `KitClear_<kit>` carrier for stamp.clear (#3987); extract keeps
//       the kit's hand-tuned role, verb, wall, attach, footprint, front, counter walls and lane widths for a row that did not move
//       (inheritPrev), and reads the clear list from the carrier (`--clear <res,...>` overrides, `--clear none` empties it).
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { isAuthoringClaim } from "../grid/lattice.mjs";
import { layerOfSlug, readCells } from "../lib/world-zones.mjs";
import { bayClearName, bayRowName, checkKit, entitiesOf, extractKit, KITS_DIR, parseBlocks } from "./kit.mjs";
import { kitPrefab } from "./stamp.mjs";

export const help = "author <kit> [--from <place>/<slot>] [--margin m] | author --reopen <kit> | author extract <Grid layer> <group> --kit k   build a shop in an authoring bay (a 2x2 at the east end of the grid) around a copy of the island building, then lift it building-local for `town stamp` (extract frees the bay; --reopen rebuilds it from the kit)";

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

// The previous kit row a re-extracted row stands for: the same id, else the one whose id the new id ends with (or the reverse),
// else the only previous row of that prefab. Rows are matched once.
export function previousRow(row, prev, used) {
  const free = prev.rows.filter((p) => !used.has(p.id));
  const exact = free.find((p) => p.id === row.id);
  if (exact) return exact;
  const suffix = free.filter((p) => row.id.endsWith(`-${p.id}`) || p.id.endsWith(`-${row.id}`));
  if (suffix.length === 1) return suffix[0];
  const same = free.filter((p) => p.prefab === row.prefab);
  return same.length === 1 ? same[0] : null;
}

// Give every row the grid held as a bare prop the config of its previous kit row (config and component GUIDs), and keep the previous
// kit's source files as `source.also` so a nested block (stock lines, shop items) is still found at stamp time.
// Returns { carried: [{ id, from }], bare: [id] }: rows that took config, and rows that have none.
export function carryConfig(out, prev, used = new Set()) {
  const carried = [];
  for (const r of out.rows) {
    if (r.config?.length) continue;
    const p = previousRow(r, prev, used);
    if (!p?.config?.length) continue;
    used.add(p.id);
    r.config = JSON.parse(JSON.stringify(p.config));
    carried.push({ id: r.id, from: p.id });
  }
  if (carried.length) {
    const also = [prev.source?.file, ...(prev.source?.also ?? [])].filter((f) => f && f !== out.source.file && !out.source.also?.includes(f));
    if (also.length) out.source.also = [...new Set([...also, ...(out.source.also ?? [])])];
  }
  return { carried, bare: out.rows.filter((r) => !r.config?.length).map((r) => r.id) };
}

// A row that moved more than this since the kit was extracted re-reads its verb and wall from the new place; one that did not keeps the
// kit's (hand-tuned) values. SAME_*: the rounding a layer round trip adds (3 decimals through a rotation) is not a move.
const MOVED_M = 0.1;
const MOVED_DEG = 5;
const SAME_M = 0.002;
const SAME_DEG = 0.01;
const angDiff = (a, b) => Math.abs((((a - b) % 360) + 540) % 360 - 180);
const setIf = (row, key, value) => {
  if (value === undefined) delete row[key];
  else if (JSON.stringify(row[key]) !== JSON.stringify(value)) row[key] = value;
};

// The clear list a bay layer's carrier holds (`kit-grid.mjs put` writes it; an author may edit it there): resource names, or null when
// the layer has no carrier.
export function carrierClear(ents) {
  const carrier = ents.find((e) => e.name.startsWith("KitClear_") && e.comps.some((c) => c.head.startsWith("LF_BaseWorldClearComponent")));
  if (!carrier) return null;
  const comp = carrier.comps.find((c) => c.head.startsWith("LF_BaseWorldClearComponent"));
  const list = comp.kids.find((k) => k.head.startsWith("m_aPrefabs"));
  return (list?.props ?? []).map((p) => p.replace(/^"|"[,]?$/g, "")).filter(Boolean);
}

// Hand-tuned kit values survive an extract (#3987). A layer row stands for the kit row of the same bay name (`Kit_<kit>_<id>`, what
// `town author --reopen` writes) or of the same recorded name and prefab. That row keeps its id, name, role, verb, wall, attach and
// any field the extract does not read from the layer; the layer gives it its place, prefab class, children and config. A row that
// moved re-reads verb, wall and attach. The kit's footprint, front, counter walls, lane widths, system and scope stay. Mutates `out`;
// returns the ids of the previous rows that were matched. `--fresh` (prev null) skips all of it.
export function inheritPrev(out, prev, kitName) {
  const idMap = new Map();
  const matchedPrev = new Set();
  const pairs = [];
  out.rows.forEach((r, i) => {
    const p = prev.rows.find((o) => !matchedPrev.has(o.id) && o.prefab === r.prefab && (r.name === bayRowName(kitName, o.id) || (o.name && o.name === r.name)));
    if (!p) return;
    matchedPrev.add(p.id);
    idMap.set(r.id, p.id);
    pairs.push([i, r, p]);
  });
  const mapVerb = (v) => (v.startsWith("on ") && v !== "on floor" ? `on ${idMap.get(v.slice(3)) ?? v.slice(3)}` : v);
  for (const r of out.rows) r.verb = mapVerb(r.verb);
  for (const [i, r, p] of pairs) {
    const m = JSON.parse(JSON.stringify(p));
    const moved = Math.hypot(r.offset[0] - p.offset[0], r.offset[1] - p.offset[1], r.offset[2] - p.offset[2]) > MOVED_M || angDiff(r.yaw, p.yaw) > MOVED_DEG;
    m.offset = r.offset.map((v, k) => (Math.abs(v - p.offset[k]) <= SAME_M ? p.offset[k] : v));
    m.yaw = angDiff(r.yaw, p.yaw) <= SAME_DEG ? p.yaw : r.yaw;
    for (const k of ["pitch", "roll", "scale"]) setIf(m, k, r[k] !== undefined && p[k] !== undefined && Math.abs(r[k] - p[k]) <= 0.01 ? p[k] : r[k]);
    setIf(m, "class", r.class);
    setIf(m, "children", r.children);
    setIf(m, "floor", r.floor);
    if (r.config?.length) setIf(m, "config", r.config);
    if (moved) {
      setIf(m, "verb", r.verb);
      setIf(m, "wall", r.wall);
      delete m.attach;
    }
    out.rows[i] = m;
  }
  const a = out.anchoring;
  const pa = prev.anchoring ?? {};
  a.counters = a.counters.map((c) => {
    const id = idMap.get(c.id) ?? c.id;
    return pa.counters?.find((o) => o.id === id) ?? { ...c, id };
  });
  for (const k of ["footprint", "front", "adapt"]) if (pa[k] !== undefined) a[k] = pa[k];
  if (!out.system && prev.system) out.system = prev.system;
  if (prev.scope) out.scope = prev.scope;
  if (prev.source?.group) out.source.group = prev.source.group; // what the kit was first authored from; a reopened bay names its rows Kit_<kit>_*
  for (const k of Object.keys(prev)) if (!(k in out)) out[k] = prev[k];
  if (prev.source?.also?.length) out.source.also = [...new Set([...(out.source.also ?? []), ...prev.source.also])];
  return matchedPrev;
}

// Pure but for the file reads: the grid layer -> a building-local kit with attach targets, clear lanes and the clear list.
// prev: the kit already on disk (null for a first extract): matched rows keep their hand-tuned values (inheritPrev), bare rows take its config.
// clear: resource names the stamp hides; null reads the bay's carrier entity, else the previous kit's list ([] means none).
export function authorExtract(layerPath, group, { kit, building = null, clear = null, system = "", prev = null } = {}) {
  const ents = entitiesOf(parseBlocks(readFileSync(layerPath, "utf8")));
  const b = findBuilding(ents, building);
  if (!b.name) throw new Error("the building entity has no name; name it in the grid layer");
  const out = extractKit(layerPath, group, { kit, system, origin: b.name, exclude: `${b.name},${bayClearName(kit)}`, inside: b.prefab });
  const clearList = clear ?? carrierClear(ents) ?? (prev?.stamp?.clear ?? []).map((c) => (typeof c === "string" ? c : c?.prefab)).filter(Boolean);
  // extractKit rebases a layer kit onto its lowest row; a stamp needs the height in the building's frame (the same prefab has the
  // same floors), so every offset is put back relative to the building origin.
  const picked = new Map(ents.map((e) => [e.name, e]));
  for (const r of out.rows) {
    const e = picked.get(r.name);
    if (e) r.offset[1] = r3(e.coords[1] - b.coords[1]);
  }
  const matched = prev ? inheritPrev(out, prev, kit) : new Set();
  const floorY = Math.min(...out.rows.filter((r) => r.floor === "f0" || !r.floor).map((r) => r.offset[1]));
  out.anchoring.floorY = r3(Number.isFinite(floorY) ? floorY : 0);
  for (const f of out.anchoring.floors) f.y = r3(out.anchoring.floorY + 3 * Number(f.id.slice(1)));
  for (const r of out.rows) if (!r.attach) r.attach = attachOf(r, out.rows);
  out.source.origin = "building";
  out.source.frame = "building-local";
  const tills = out.rows.filter((r) => r.role === "till" || r.role === "counter").map((r) => r.id);
  const faces = out.rows.filter((r) => /shelf/i.test(r.prefab ?? "")).map((r) => r.id);
  out.stamp = {
    schema: 1,
    prefab: b.prefab,
    // Lanes the audit keeps free (card #3883 measures them): entrance to every till and every shelf face, 1.2 m wide.
    // A lane the kit already had keeps its (hand-tuned) width.
    lanes: [...tills, ...faces].map((to) => prev?.stamp?.lanes?.find((l) => l.from === "entrance" && l.to === to) ?? { from: "entrance", to, width: LANE_WIDTH }),
    // Vanilla clutter the building prefab brings that this shop does not want: `town stamp` writes one clear carrier per copy
    // (LF_BaseWorldClearComponent) that hides it at load (#3882, #3924).
    clear: clearList.map((prefab) => ({ prefab })),
  };
  // Not a kit field: what the carry did, for the command to print.
  const carry = prev ? carryConfig(out, prev, matched) : { carried: [], bare: out.rows.filter((r) => !r.config?.length).map((r) => r.id) };
  Object.defineProperty(out, "carry", { value: carry, enumerable: false });
  return out;
}

// The `grid place` command line that claims the bay: the island building's prefab, in the pack grid's authoring bay, with the walk-around margin when given.
export function gridPlaceArgs(plan, { margin = null, dry = false } = {}) {
  const args = ["grid", "place", plan.slug, plan.prefab, "--pack", "--bay"];
  if (margin !== null && margin !== undefined) {
    const m = Number(margin);
    if (!Number.isFinite(m) || m < 0) throw new Error(`--margin ${margin} is not a number of metres`);
    args.push("--margin", String(m));
  }
  if (dry) args.push("--dry");
  return args;
}

// The authoring claim a grid layer belongs to (by its file name), or null: a layer of a normal cell, or no claim, is not a bay.
export function bayClaimOfLayer(root, layerPath) {
  const file = basename(layerPath);
  return readCells(root).find((c) => !c.error && c.slug && isAuthoringClaim(c) && layerOfSlug(c.slug) === file) ?? null;
}

// Release the bay a layer sits in once its kit is saved. `free(slug)` is `grid free` (injected in the selftest). Returns the slug or null.
export function releaseBay(root, layerPath, free) {
  const claim = bayClaimOfLayer(root, layerPath);
  if (!claim) return null;
  return free(claim.slug) === 0 ? claim.slug : null;
}

// Where `town author` builds: the island building's prefab (--from) or the kit's own, and the cell slug.
// `reopen` rebuilds a bay from an extracted kit: the kit must exist and its prefab comes from the kit alone.
export function planAuthor(kitName, { from = null, placesDir, templatesDir, kitsDir = KITS_DIR, slug = null, reopen = false }) {
  const kitFile = join(kitsDir, `${kitName}.json`);
  const kit = existsSync(kitFile) ? JSON.parse(readFileSync(kitFile, "utf8")) : null;
  if (reopen) {
    if (!kit) throw new Error(`--reopen ${kitName}: no kit tools/town/kits/${kitName}.json to rebuild from (a first build is town author ${kitName} --from <place>/<slot>)`);
    if (from) throw new Error(`--reopen ${kitName}: the kit names its building; drop --from`);
    const prefab = kitPrefab(kit);
    if (!prefab) throw new Error(`--reopen ${kitName}: the kit has no building prefab (stamp.prefab / building.vanilla)`);
    return { kit, kitFile, prefab, source: `kit ${kitName} (reopen)`, slug: slug ?? `author-${kitName}`, seed: true, reopen: true };
  }
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

// `town author extract`: write the kit and, for a layer that sits in an authoring bay, archive the layer beside the kit
// (tools/town/kits/sources/<kit>.layer: `town fit` reads the components the kit names but does not copy from its source) and
// release the bay (`free(slug)` is `grid free`). The kit in tools/town/kits/ is the source of truth; `town author --reopen <kit>`
// builds the bay again. A kit written elsewhere (--out) or --keep-bay leaves the bay and the source where they are.
export function extractToKit(root, layerPath, group, a, { free }) {
  const kitsDir = resolve(join(root, "tools", "town", "kits"));
  const out = resolve(a.out ?? join(kitsDir, `${a.kit}.json`));
  const prevFile = join(kitsDir, `${a.kit}.json`);
  const prev = !a.fresh && existsSync(prevFile) ? JSON.parse(readFileSync(prevFile, "utf8")) : null;
  const claim = out.startsWith(kitsDir + sep) ? bayClaimOfLayer(root, layerPath) : null;
  const release = Boolean(claim) && !a["keep-bay"];
  const kit = authorExtract(layerPath, group, { kit: a.kit, building: a.building ?? null, clear: clearArg(a.clear), system: a.system ?? "", prev });
  const archive = join(kitsDir, "sources", `${kit.kit}.layer`);
  const rel = (p) => relative(root, p).split(sep).join("/");
  const had = existsSync(archive);
  kit.source.file = rel(layerPath);
  if (release) {
    // the layer is kept beside the kit, and the kit names that copy as its source
    mkdirSync(dirname(archive), { recursive: true });
    writeFileSync(archive, `${ARCHIVE_HEAD(kit.kit)}${readFileSync(layerPath, "utf8")}`);
    kit.source.file = rel(archive);
  }
  if (kit.source.also) kit.source.also = kit.source.also.filter((f) => f !== kit.source.file);
  if (kit.source.also?.length === 0) delete kit.source.also;
  for (const c of kit.carry.carried) console.log(`  config carried: ${c.id} <- previous row ${c.from}`);
  if (kit.carry.bare.length) console.log(`  no config on: ${kit.carry.bare.join(", ")} (plain props; give the grid row its component if one needs it)`);
  const errs = checkKit(kit, { root });
  if (errs.length) {
    if (release && !had) unlinkSync(archive);
    console.log(errs.join("\n"));
    return 1;
  }
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, JSON.stringify(kit, null, 2) + "\n");
  console.log(`kit ${kit.kit}: ${kit.rows.length} rows, building-local around ${kit.stamp.prefab} -> ${rel(out)}`);
  if (claim && !release) console.log(`bay ${claim.cell} (${claim.slug}) kept (--keep-bay); release it with tools\\cli grid free ${claim.slug}`);
  else if (release) {
    const freed = releaseBay(root, layerPath, free);
    console.log(freed ? `bay ${claim.cell} released (${freed}); its layer is kept as ${rel(archive)}; to edit this shop again: tools\\cli town author --reopen ${kit.kit}` : `bay ${claim.cell} (${claim.slug}) was NOT released; run tools\\cli grid free ${claim.slug}`);
  }
  console.log(`next: tools\\cli town stamp ${kit.kit} --dry`);
  return 0;
}

const ARCHIVE_HEAD = (kit) => `// Source of the \`${kit}\` kit (tools/town/kits/${kit}.json): the grid layer it was authored in, kept when its authoring bay was released (town author extract). \`town fit\` reads the components the kit names from here; \`town author --reopen ${kit}\` builds a bay again.\n`;

// --clear: a comma list of resource names; "none" is an empty list; absent is null (read the bay's carrier, else the previous kit's list).
export const clearArg = (v) => (v === undefined || v === true ? null : v === "none" ? [] : String(v).split(",").filter(Boolean));

function cli(root, args) {
  const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), ...args], { cwd: root, encoding: "utf8", timeout: 600000, windowsHide: true });
  process.stdout.write(r.stdout ?? "");
  return r.status ?? 1;
}

function parseArgs(args) {
  const o = { _: [] };
  const valued = new Set(["from", "slug", "kit", "building", "clear", "system", "out", "margin", "places-dir", "templates-dir", "reopen"]);
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
    return extractToKit(root, resolve(layer), group, a, { free: (slug) => cli(root, ["grid", "free", slug]) });
  }
  const kitName = a.reopen ? a.reopen : a._[0];
  if (!kitName || a.help) {
    console.log("usage: tools\\cli town author <kit> [--from <place>/<slot>] [--slug s] [--dry] | town author --reopen <kit> | town author extract ... (tools/town/author.mjs header)");
    return a.help ? 0 : 2;
  }
  const plan = planAuthor(kitName, { reopen: Boolean(a.reopen), from: a.from ?? null, slug: a.slug ?? null, placesDir: resolve(root, a["places-dir"] ?? "docs/everon/places"), templatesDir: resolve(root, a["templates-dir"] ?? "tools/town/templates"), kitsDir: join(root, "tools", "town", "kits") });
  console.log(`town author ${kitName}: authoring bay ${plan.slug} around ${plan.prefab} (from ${plan.source})${plan.seed ? ", seeded with the kit's rows" : ""}`);
  const open = readCells(root).find((c) => c.slug === plan.slug);
  if (open) {
    console.log(`bay ${open.cell} is already open for ${plan.slug}: work there (tools\\cli look grid:${plan.slug}), or release it with tools\\cli grid free ${plan.slug} and run this again`);
    return 1;
  }
  const placeArgs = gridPlaceArgs(plan, { margin: a.margin ?? null, dry: Boolean(a.dry) });
  if (cli(root, placeArgs)) return 1;
  if (a.dry) return 0;

  if (plan.seed) {
    const { layerFileOf } = await import("./kit-grid.mjs");
    const ents = entitiesOf(parseBlocks(readFileSync(layerFileOf(plan.slug), "utf8")));
    const b = findBuilding(ents);
    const r = spawnSync(process.execPath, [join(root, "tools", "town", "kit-grid.mjs"), "put", kitName, plan.slug, String(b.coords[0]), String(b.coords[2]), String(b.yaw), `--y=${b.coords[1]}`], { cwd: root, encoding: "utf8", windowsHide: true });
    process.stdout.write(r.stdout ?? "");
    if (r.status) return 1;
  }
  if (!plan.seed && clearArg(a.clear)?.length) {
    // a first build: the bay hides the clutter the shop will clear at stamp time, so its audit and look see the building as the stamp leaves it
    const { layerFileOf, clearCarrierBlock } = await import("./kit-grid.mjs");
    const file = layerFileOf(plan.slug);
    const b = findBuilding(entitiesOf(parseBlocks(readFileSync(file, "utf8"))));
    const block = clearCarrierBlock(kitName, plan.prefab, clearArg(a.clear), b.coords[0], b.coords[2], b.coords[1]);
    writeFileSync(file, readFileSync(file, "utf8").replace(/\n*$/, "\n") + block + "\n");
    console.log(`bay ${plan.slug}: clear carrier for ${clearArg(a.clear).length} props (extract reads the list back from it)`);
  }
  console.log(`next: tools\\cli look grid:${plan.slug} --interior auto; tools\\cli audit placement --grid --strict; then town author extract <the cell's layer> '<group>' --kit ${kitName} (it frees the bay)`);
  return 0;
}
