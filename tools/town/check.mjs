// tools/town/check.mjs - `tools\cli town check <place>`: the static checks of one place (docs/architecture/everon-world.md 3.5, card #2206).
//
//   town check <place> [--json] [--template-file f] [--catalog f] [--no-perf]
//
// Reads  docs/everon/places/<place>/dossier.json and mapping.json, tools/town/templates/<kind>.json, tools/town/claims/<place>-*.json,
//        addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_*.layer, addons/LifeFramework/Configs/Systems/LF_SystemCatalog.conf
// Reports (an ERROR fails; a WARN does not):
//   - every required slot of the template is mapped (a mapped optional slot is checked like a required one);
//   - every mapped slot has its layer and its claim, and the claim agrees with the mapping (place, slot, building or plot);
//   - every layer registers the LF_Places kinds its slot promises (`LF_PlaceComponent` m_sKind, id `<place>.<slot>`); a promised
//     kind that no system lists in `m_aPlaces` of LF_SystemCatalog.conf is a WARN (the layer is right, nothing reads the kind yet);
//   - every entity of a layer stands inside its claimed building's footprint (or plot) plus the slot's margin;
//   - no two claims name one building or plot (a street slot marked `shared` may stand on a building another slot holds);
//   - a claim whose `destruction.mode` is "needs-hook" is a WARN until the core's indestructible hook lands (D15);
//   - the performance budget (docs/architecture/performance.md): each slot's layer against the building budget of its size
//     (the same counts as `tools\cli perf building <slug> --size <size> --json`), and the per-place limits within 60 m;
//   - the map POI-kind slot check: TODO(#2245), `tools/town/lib/poi-check.mjs` (card M17) has not landed.
// The pure functions are what tools/validation/island-zones-selftest.mjs runs; `validate-island-zones` shares the claim and footprint half.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { BUDGET, countTree, makeResolver, parseEntities, verdict } from "../perf/building-count.mjs";
import { topLevelCoords } from "../validation/validate-world-zones.mjs";
import { loadTemplate } from "./map.mjs";

export const help = "check <place> [--json] [--no-perf]   one place: required slots mapped, layers and claims, kinds against m_aPlaces, footprints, perf budget; exit 1 on an error";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const CLAIMS_DIR = "tools/town/claims";
export const CATALOG = "addons/LifeFramework/Configs/Systems/LF_SystemCatalog.conf";
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;

// Margins in metres around a footprint (or plot) that a slot's entities may stand in. A building slot is a kit inside the walls
// plus a doorstep; a street slot (the board, the taxi rank) stands on the pavement in front of its building, so it gets a kerb.
export const MARGIN = { building: 2, plot: 2, street: 12 };
export const SLOT_MARGIN = { taxi: 8, board: 10 }; // a template slot's own `margin` beats these
// Building-budget size per slot (docs/architecture/performance.md "The budget": a bank, the town hall and the police station are large).
export const SLOT_SIZE = { arrival: "large", bank: "large", police: "large", board: "small", taxi: "small", parcel: "small", fence: "small" };
// Per place: a slot set within this radius shares one view (docs/architecture/performance.md "Per place").
export const PLACE_RADIUS = 60;
export const PLACE_LIMITS = { lights: 24, shadow: 12, entities: 1500 };

export const layerNameOf = (place, slot) => `Place_${place}_${slot}.layer`;
export const claimNameOf = (place, slot) => `${place}-${slot}.json`;
const r1 = (n) => Math.round(n * 10) / 10;

// ------------------------------------------------------------------ catalog and layer text

// The kinds the systems look for: every string inside an `m_aPlaces { ... }` list of LF_SystemCatalog.conf.
export function catalogPlaces(text) {
  const kinds = new Set();
  for (const m of String(text).matchAll(/m_aPlaces\s*\{([^}]*)\}/g)) for (const k of m[1].matchAll(/"([^"]+)"/g)) kinds.add(k[1]);
  return kinds;
}

// The LF_PlaceComponent markers of a layer: [{ kind, id }].
export function layerMarkers(text) {
  const out = [];
  for (const m of String(text).matchAll(/LF_PlaceComponent\s+"\{[0-9A-Fa-f]{16}\}"\s*\{([^}]*)\}/g)) {
    const kind = /m_sKind\s+"([^"]*)"/.exec(m[1])?.[1];
    const id = /m_sId\s+"([^"]*)"/.exec(m[1])?.[1];
    if (kind) out.push({ kind, id: id ?? "" });
  }
  return out;
}

// ------------------------------------------------------------------ geometry

// Distance in metres from a point to a polygon (0 inside), on the XZ plane.
export function distToPolygon(px, pz, poly) {
  let inside = false;
  let best = Infinity;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, zi] = poly[i];
    const [xj, zj] = poly[j];
    if (zi > pz !== zj > pz && px < ((xj - xi) * (pz - zi)) / (zj - zi) + xi) inside = !inside;
    const dx = xj - xi;
    const dz = zj - zi;
    const len2 = dx * dx + dz * dz;
    const t = len2 ? Math.max(0, Math.min(1, ((px - xi) * dx + (pz - zi) * dz) / len2)) : 0;
    best = Math.min(best, Math.hypot(px - (xi + t * dx), pz - (zi + t * dz)));
  }
  return inside ? 0 : best;
}

// The ground a slot may use: the mapped building's footprint corners, or the mapped free plot's rectangle. null when unknown.
export function slotGround(entry, dossier) {
  if (entry?.building) {
    const b = (dossier.buildings ?? []).find((x) => x.id === entry.building);
    const f = b?.footprint;
    if (!b) return null;
    if (f?.corners?.length >= 3) return { poly: f.corners, centre: [f.cx ?? b.x, f.cz ?? b.z], what: `building ${entry.building}` };
    const [cx, cz] = [f?.cx ?? b.x, f?.cz ?? b.z];
    const [hw, hd] = [(f?.w ?? 8) / 2, (f?.d ?? 8) / 2];
    return { poly: [[cx - hw, cz - hd], [cx + hw, cz - hd], [cx + hw, cz + hd], [cx - hw, cz + hd]], centre: [cx, cz], what: `building ${entry.building}` };
  }
  if (entry?.plot) {
    const p = (dossier.freePlots?.plots ?? dossier.plots ?? []).find((x) => x.id === entry.plot);
    if (!p) return null;
    const [hw, hd] = [(p.width ?? 10) / 2, (p.depth ?? 10) / 2];
    return { poly: [[p.x - hw, p.z - hd], [p.x + hw, p.z - hd], [p.x + hw, p.z + hd], [p.x - hw, p.z + hd]], centre: [p.x, p.z], what: `plot ${entry.plot}` };
  }
  return null;
}

// The slot's margin: the template's own, else the per-slot table, else the host's.
export function marginOf(slot, entry) {
  if (typeof slot?.margin === "number") return slot.margin;
  if (slot && SLOT_MARGIN[slot.id] !== undefined) return SLOT_MARGIN[slot.id];
  if (slot?.host === "street") return MARGIN.street;
  return entry?.plot ? MARGIN.plot : MARGIN.building;
}

// ------------------------------------------------------------------ claims and footprints (shared with validate-island-zones)

// data: { place, dossier, mapping, template, claims: { <file name>: claim }, layers: { <layer file name>: text } }
// Returns { errors, warnings, slots: [{ slot, entry, ground, claim, layer }] }.
export function checkClaimsAndFootprints(data) {
  const { place, dossier, mapping, template } = data;
  const errors = [];
  const warnings = [];
  const slots = [];
  const slotsById = new Map(template.slots.map((s) => [s.id, s]));
  const claimsByName = data.claims ?? {};
  const layers = data.layers ?? {};

  // A layer or claim of this place that no mapped slot accounts for.
  for (const name of Object.keys(layers)) {
    const id = name.slice(`Place_${place}_`.length, -".layer".length);
    const e = mapping.slots?.[id];
    if (!e || e.none) errors.push(`${LAYER_DIR}/${name}: no mapped slot '${id}' in the mapping of ${place} (an island layer needs its slot mapped first)`);
  }

  const holders = new Map(); // building or plot -> [slot ids that hold it exclusively]
  for (const [id, entry] of Object.entries(mapping.slots ?? {})) {
    if (!entry || entry.none) continue;
    const slot = slotsById.get(id);
    if (!slot) continue; // checkMapping reports a slot the template lacks
    const layerName = layerNameOf(place, id);
    const claimName = claimNameOf(place, id);
    const claim = claimsByName[claimName] ?? null;
    const text = layers[layerName];
    const ground = slotGround(entry, dossier);
    const row = { slot: id, entry, ground, claim, layer: text ?? null, layerName };
    slots.push(row);
    if (!claim) errors.push(`slot ${id}: no claim ${CLAIMS_DIR}/${claimName}`);
    if (text === undefined) errors.push(`slot ${id}: no layer ${LAYER_DIR}/${layerName}`);
    if (claim) {
      if (claim.place !== place || claim.slot !== id) errors.push(`slot ${id}: ${claimName} names ${claim.place}.${claim.slot}`);
      if ((entry.building && claim.building !== entry.building) || (entry.plot && claim.plot !== entry.plot)) {
        errors.push(`slot ${id}: the claim names ${claim.building ?? claim.plot ?? "nothing"}, the mapping says ${entry.building ?? entry.plot}`);
      }
      if (claim.layer && !String(claim.layer).endsWith(`/${layerName}`)) errors.push(`slot ${id}: the claim's layer is ${claim.layer}, expected ${layerName}`);
      if (claim.destruction?.mode === "needs-hook") {
        warnings.push(`slot ${id}: destruction is "needs-hook" (${claim.destruction.reason ?? "a vanilla building cannot be switched off by a layer"}); fine until the indestructible hook lands, then fit it again`);
      }
    }
    const key = entry.building ? `building ${entry.building}` : `plot ${entry.plot}`;
    if (!entry.shared) {
      if (!holders.has(key)) holders.set(key, []);
      holders.get(key).push(id);
    }
    if (!ground) {
      errors.push(`slot ${id}: ${key} is not in the dossier of ${place}`);
      continue;
    }
    if (text !== undefined) {
      const margin = marginOf(slot, entry);
      for (const c of topLevelCoords(text)) {
        const d = distToPolygon(c.x, c.z, ground.poly);
        if (d > margin) errors.push(`${LAYER_DIR}/${layerName}: ${c.name} stands ${r1(d)} m outside ${ground.what}, over the ${margin} m margin of slot ${id}`);
      }
    }
  }
  // No two claims name one building (or plot): two slots mapped to it, or two claim files carrying it.
  for (const [key, ids] of holders) if (ids.length > 1) errors.push(`${key} is named by more than one claim: slots ${ids.join(", ")} (a street slot may share it with \`shared: true\`)`);
  const byClaim = new Map();
  for (const [name, c] of Object.entries(claimsByName)) {
    if (!c || c.shared) continue;
    const key = c.building ? `building ${c.building}` : c.plot ? `plot ${c.plot}` : null;
    if (!key) continue;
    if (!byClaim.has(key)) byClaim.set(key, []);
    byClaim.get(key).push(name);
  }
  for (const [key, names] of byClaim) if (names.length > 1) errors.push(`${key} is named by claims ${names.join(", ")}`);
  return { errors: [...new Set(errors)], warnings, slots };
}

// ------------------------------------------------------------------ the full place check

// Pure but for `resolve` (a `{GUID}path` -> { text } reader; null skips the perf count). opts: { resolve, perf }.
export function checkPlace(data, opts = {}) {
  const { place, template } = data;
  const base = checkClaimsAndFootprints(data);
  const errors = [...base.errors];
  const warnings = [...base.warnings];
  const notes = [];
  const slotsById = new Map(template.slots.map((s) => [s.id, s]));
  const mapping = data.mapping;

  // Required slots are mapped.
  for (const slot of template.slots) {
    const e = mapping.slots?.[slot.id];
    if (slot.required && (!e || e.none)) errors.push(`slot ${slot.id}: required slot is not mapped${e?.reason ? ` (${e.reason})` : ""}`);
  }

  // Kinds: each promised kind is in the layer; each is known to a system's m_aPlaces.
  const known = data.catalogKinds ?? null;
  for (const row of base.slots) {
    const slot = slotsById.get(row.slot);
    const promised = slot.places ?? [];
    if (row.layer !== null) {
      const markers = layerMarkers(row.layer);
      const have = new Set(markers.map((m) => m.kind));
      for (const kind of promised) if (!have.has(kind)) errors.push(`slot ${row.slot}: the layer registers no LF_PlaceComponent of kind ${kind}`);
      for (const m of markers) if (promised.includes(m.kind) && m.id !== `${place}.${row.slot}`) errors.push(`slot ${row.slot}: ${m.kind} marker has id '${m.id}', expected '${place}.${row.slot}'`);
    }
    if (known) for (const kind of promised) if (!known.has(kind)) warnings.push(`slot ${row.slot}: kind ${kind} is in no system's m_aPlaces (LF_SystemCatalog.conf); the layer is right but nothing looks for it yet`);
  }

  // TODO(#2245): the map POI-kind slot check (card M17, tools/town/lib/poi-check.mjs) is not in yet. When it lands, call it here:
  // a slot whose front kind has no POI row, and no explicit `none`, is an error.
  notes.push("map POI kinds: not checked yet (waiting on #2245, tools/town/lib/poi-check.mjs)");

  // Performance budget.
  const perf = [];
  if (opts.resolve) {
    const seen = new Map();
    for (const row of base.slots) {
      if (row.layer === null) continue;
      const size = row.claim?.size ?? SLOT_SIZE[row.slot] ?? "medium";
      if (!BUDGET[size]) {
        errors.push(`slot ${row.slot}: claim size '${size}' is not small, medium or large`);
        continue;
      }
      const t = countTree(parseEntities(row.layer), opts.resolve, seen);
      const over = verdict(t, size);
      perf.push({ slot: row.slot, size, centre: row.ground?.centre ?? null, entities: t.entities, lights: t.lights, shadow: t.shadow, unset: t.unset, point: t.point, over, unresolved: [...t.unresolved] });
      for (const o of over) errors.push(`slot ${row.slot} (${size}): over the building budget: ${o}`);
      if (t.unresolved.size) warnings.push(`slot ${row.slot}: ${t.unresolved.size} prefab(s) could not be read, so their insides are not counted (e.g. ${[...t.unresolved][0]})`);
    }
    for (const o of placeOver(perf)) errors.push(o);
  }
  return { ok: errors.length === 0, errors: [...new Set(errors)], warnings, notes, slots: base.slots.map((s) => s.slot), perf };
}

// Pure: the per-place limits. For each slot as the centre, the slots whose centres lie within 60 m share one view; their added
// entities, lights and shadow casters are summed. Returns the OVER lines (the worst centre of each limit, once).
export function placeOver(perf) {
  const out = [];
  const worst = {};
  for (const c of perf) {
    if (!c.centre) continue;
    const near = perf.filter((p) => p.centre && Math.hypot(p.centre[0] - c.centre[0], p.centre[1] - c.centre[1]) <= PLACE_RADIUS);
    for (const [key, limit] of Object.entries(PLACE_LIMITS)) {
      const sum = near.reduce((n, p) => n + p[key], 0);
      if (sum > limit && (!worst[key] || sum > worst[key].sum)) worst[key] = { sum, limit, at: c.slot, n: near.length };
    }
  }
  const label = { lights: "lights switched on at night", shadow: "shadow casters", entities: "entities added" };
  for (const [key, w] of Object.entries(worst)) out.push(`place limit: ${label[key]} ${w.sum} > ${w.limit} within ${PLACE_RADIUS} m of slot ${w.at} (${w.n} slot(s))`);
  return out;
}

// ------------------------------------------------------------------ guard of the island layers (shared with validate-island-zones)

// Pure: only a `ws/town-draft-<place>-<slot>` branch may change a Place_<place>_<slot>.layer, and only its own. `files` are repo paths
// changed on the branch. main, a detached HEAD and a missing branch are not checked.
export function guardIslandLayers({ branch, files }) {
  const errors = [];
  const touched = (files ?? []).map((f) => String(f).replace(/\\/g, "/")).filter((f) => f.startsWith(`${LAYER_DIR}/`) && /\/Place_[^/]+\.layer$/.test(f));
  if (!touched.length) return { errors, touched };
  const name = String(branch ?? "");
  if (!name || name === "main" || name === "HEAD") return { errors, touched, skipped: `branch '${name || "unknown"}' is not checked` };
  const slug = name.replace(/^ws\//, "");
  const draft = /^town-draft-([a-z0-9-]+)$/.exec(slug);
  for (const f of touched) {
    const base = f.slice(f.lastIndexOf("/") + 1, -".layer".length); // Place_<place>_<slot>
    if (!draft) {
      errors.push(`${f}: only a town-draft-<place>-<slot> branch may change an island layer (this is ${name}); create the worktree with tools\\cli wt new town-draft-<place>-<slot>`);
      continue;
    }
    // The branch joins place and slot with '-'; the layer joins them with '_' (slugs hold no '_'), so the layer is this branch's own
    // when its two parts joined with '-' give the branch's suffix.
    const parts = base.replace(/^Place_/, "").split("_");
    if (parts.length !== 2 || parts.join("-") !== draft[1]) errors.push(`${f}: branch ${name} may change only its own Place_<place>_<slot>.layer, not ${base}.layer`);
  }
  return { errors, touched };
}

// ------------------------------------------------------------------ repo reads

const readJson = (f) => JSON.parse(readFileSync(f, "utf8"));

// Everything about one place from a repo root. Throws on a missing dossier or mapping (a place with neither is not a place).
export function loadPlace(root, place, { templateFile = null, catalogFile = null, dossierFile = null, mappingFile = null, layerDir = LAYER_DIR, claimsDir = CLAIMS_DIR } = {}) {
  const placeDir = join(root, "docs", "everon", "places", place);
  const dFile = dossierFile ?? join(placeDir, "dossier.json");
  const mFile = mappingFile ?? join(placeDir, "mapping.json");
  if (!existsSync(dFile)) throw new Error(`no dossier: ${dFile} does not exist (tools\\cli scout ${place})`);
  if (!existsSync(mFile)) throw new Error(`no mapping: ${mFile} does not exist (tools\\cli town map ${place})`);
  const dossier = readJson(dFile);
  const mapping = readJson(mFile);
  const template = templateFile ? readJson(templateFile) : loadTemplate(mapping.template, root);
  const claims = {};
  const cDir = join(root, claimsDir);
  if (existsSync(cDir)) {
    for (const f of readdirSync(cDir)) {
      if (!f.endsWith(".json")) continue;
      const c = readJson(join(cDir, f));
      if (c && c.place === place) claims[f] = c; // the file name is <place>-<slot>, ambiguous when both hold a hyphen: the claim says which place it is
    }
  }
  const layers = {};
  const lDir = join(root, layerDir);
  if (existsSync(lDir)) for (const f of readdirSync(lDir)) if (f.startsWith(`Place_${place}_`) && f.endsWith(".layer")) layers[f] = readFileSync(join(lDir, f), "utf8");
  const cat = catalogFile ?? join(root, CATALOG);
  const catalogKinds = existsSync(cat) ? catalogPlaces(readFileSync(cat, "utf8")) : null;
  return { place, dossier, mapping, template, claims, layers, catalogKinds };
}

// ------------------------------------------------------------------ CLI

export async function main(args, { root = ROOT } = {}) {
  const flags = new Set(args.filter((a) => a.startsWith("--") && !["--template-file", "--catalog"].includes(a)));
  const val = (k) => {
    const i = args.indexOf(k);
    return i === -1 ? null : args[i + 1];
  };
  const skip = new Set([val("--template-file"), val("--catalog")].filter(Boolean));
  const place = args.find((a) => !a.startsWith("--") && !skip.has(a));
  if (!place || !SLUG_RX.test(place)) {
    console.log("usage: tools\\cli town check <place> [--json] [--no-perf] [--template-file f] [--catalog f]");
    return 2;
  }
  let data;
  try {
    data = loadPlace(root, place, { templateFile: val("--template-file") ? resolve(root, val("--template-file")) : null, catalogFile: val("--catalog") ? resolve(root, val("--catalog")) : null });
  } catch (e) {
    console.log(`SUMMARY town-check fail ${place}: ${e.message}`);
    return 1;
  }
  const result = checkPlace(data, { resolve: flags.has("--no-perf") ? null : await makeResolver(root) });
  if (flags.has("--json")) {
    console.log(JSON.stringify({ place, ...result }, null, 2));
  } else {
    for (const n of result.notes) console.log(`note  ${n}`);
    for (const w of result.warnings) console.log(`WARN  ${w}`);
    for (const e of result.errors) console.log(`ERROR ${e}`);
    const lights = result.perf.reduce((n, p) => n + p.lights, 0);
    const ents = result.perf.reduce((n, p) => n + p.entities, 0);
    console.log(`SUMMARY town-check ${result.ok ? "ok" : "fail"} ${place}: ${result.slots.length} mapped slot(s), ${result.errors.length} error(s), ${result.warnings.length} warning(s)${result.perf.length ? `, ${ents} entities and ${lights} lights added` : ""}`);
  }
  return result.ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
