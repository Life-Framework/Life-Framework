// tools/town/map.mjs - `tools\cli town map <place>`: propose a mapping of a place template's slots onto a scout dossier's
// buildings and free plots, and check one (docs/architecture/everon-world.md 3.2).
//
//   town map <slug> [--template town|village] [--dossier file] [--out file] [--force] [--dry]
//   town map <slug> --check [--template kind] [--dossier file] [--out file]
//
// Reads  docs/everon/places/<slug>/dossier.json   (the scout's schema 1, tools/scout/scout.mjs buildDossier)
//        tools/town/templates/<kind>.json         (slots and what a building must offer; no coordinate, no place name)
// Writes docs/everon/places/<slug>/mapping.json   (slot -> { building } | { plot, prefab } | { none, reason })
//
// The proposal is a start for a human or the lead: it scores, it does not decide (D10). A mapping that already exists is
// never overwritten without --force, because the reviewed one is what the fit-out agents build from. Pure functions
// (loadTemplate, checkTemplate, proposeMapping, checkMapping) are what tools/validation/town-map-selftest.mjs runs on
// a fixture; the CLI part at the bottom only reads and writes files.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { typeIndex, unknownTypes } from "../scout/types.mjs";
import { checkSlotPois, loadPoiKinds } from "./lib/poi-check.mjs";

export const help = "map <slug> [--template town|village] [--dossier f] [--out f] [--force] | --check   propose a slot -> building/plot mapping from the scout dossier; --check refuses an incomplete one or a slot with no POI kind";

export const DOSSIER_SCHEMA = 1;
export const TEMPLATE_SCHEMA = 1;
export const MAPPING_SCHEMA = 1;
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
const r3 = (n) => Math.round(n * 1000) / 1000;
const clamp01 = (n) => Math.max(0, Math.min(1, n));

// ------------------------------------------------------------------ templates

export function loadTemplate(kind, root) {
  if (!SLUG_RX.test(String(kind))) throw new Error(`template kind '${kind}' is not a plain name`);
  const file = join(root, "tools", "town", "templates", `${kind}.json`);
  if (!existsSync(file)) throw new Error(`no template ${kind}: ${relative(root, file)} does not exist`);
  return JSON.parse(readFileSync(file, "utf8"));
}

const COORD_KEYS = /^(x|y|z|pos|position|coord|coords|coordinate|coordinates|origin|centre|center|lat|lon|lng|yaw|angles?)$/i;
const COORD_PAIR = /-?\d+(\.\d+)?\s*[,;]\s*-?\d+(\.\d+)?/;
// Names of the island's places the tooling must not carry; the template is for any place of its kind.
const PLACE_NAMES = /\b(saint[- ]?philippe|st\.? philippe|meaux|tyrone|gravette|montignac|provins|entre-deux|chotain|figari|laruns|levie|morton|le moule|villeneuve|lamentin|saint[- ]?pierre|durras|vernon|regina|erquy)\b/i;

// Pure: the problems of a template, [] when it is sound. Tooling data only: no coordinate, no place name.
export function checkTemplate(t) {
  const out = [];
  if (!t || typeof t !== "object") return ["the template is not an object"];
  if (t.schema !== TEMPLATE_SCHEMA) out.push(`schema is ${t.schema}, expected ${TEMPLATE_SCHEMA}`);
  if (typeof t.kind !== "string" || !SLUG_RX.test(t.kind)) out.push("kind is missing or not a plain name");
  if (!Array.isArray(t.slots) || !t.slots.length) out.push("slots is missing or empty");
  const ids = new Set();
  for (const s of Array.isArray(t.slots) ? t.slots : []) {
    const at = `slot ${s?.id ?? "?"}`;
    if (typeof s?.id !== "string" || !SLUG_RX.test(s.id)) out.push(`${at}: id is missing or not a plain name`);
    else if (ids.has(s.id)) out.push(`${at}: duplicate slot id`);
    else ids.add(s.id);
    if (typeof s?.required !== "boolean") out.push(`${at}: required must be true or false`);
    if (typeof s?.kit !== "string" || !s.kit) out.push(`${at}: kit is missing`);
    if (s?.host !== "building" && s?.host !== "street" && s?.host !== "plot") out.push(`${at}: host must be building, street or plot`);
    if (!s?.needs || typeof s.needs !== "object") out.push(`${at}: needs is missing`);
    else if (!["required", "optional"].includes(s.needs.interior)) out.push(`${at}: needs.interior must be required or optional`);
    if (!Array.isArray(s?.places)) out.push(`${at}: places must be a list of LF_Places kinds`);
    if (Array.isArray(s?.needs?.types)) for (const u of unknownTypes(s.needs.types)) out.push(`${at}: type '${u}' is not a scout building type or alias (tools/scout/types.mjs)`);
    if (s?.host === "plot" && !s?.plot) out.push(`${at}: a plot slot needs a plot block`);
  }
  walk(t, "", (key, value, path) => {
    if (key !== null && COORD_KEYS.test(key)) out.push(`${path}: '${key}' is a coordinate key; a template holds none`);
    if (Array.isArray(value) && value.length >= 2 && value.length <= 4 && value.every((v) => typeof v === "number")) out.push(`${path}: a list of ${value.length} numbers looks like a coordinate`);
    if (typeof value === "string") {
      if (COORD_PAIR.test(value)) out.push(`${path}: '${value}' holds a coordinate pair`);
      if (PLACE_NAMES.test(value)) out.push(`${path}: '${value}' names a place; a template holds no place names`);
    }
  });
  return out;
}

function walk(v, path, fn, key = null) {
  fn(key, v, path || "$");
  if (Array.isArray(v)) v.forEach((x, i) => walk(x, `${path}[${i}]`, fn, null));
  else if (v && typeof v === "object") for (const [k, x] of Object.entries(v)) walk(x, `${path}.${k}`, fn, k);
}

// ------------------------------------------------------------------ dossier

function readDossier(d) {
  if (!d || typeof d !== "object") throw new Error("the dossier is not an object");
  if (d.schema !== DOSSIER_SCHEMA) throw new Error(`dossier schema is ${d.schema}; this tool reads schema ${DOSSIER_SCHEMA}`);
  if (!Array.isArray(d.buildings)) throw new Error("the dossier has no buildings list");
  if (!d.place || typeof d.place.slug !== "string") throw new Error("the dossier has no place.slug");
  const plots = d.freePlots && Array.isArray(d.freePlots.plots) ? d.freePlots.plots : [];
  return { buildings: d.buildings, plots, roads: Array.isArray(d.roads) ? d.roads : [], place: d.place };
}

// Pure: metres from a point to a road polyline of the dossier.
function roadDistance(road, x, z) {
  let best = Infinity;
  for (let i = 1; i < road.points.length; i++) {
    const [ax, az] = road.points[i - 1];
    const [bx, bz] = road.points[i];
    const dx = bx - ax;
    const dz = bz - az;
    const l2 = dx * dx + dz * dz;
    const t = l2 ? Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / l2)) : 0;
    best = Math.min(best, Math.hypot(x - (ax + t * dx), z - (az + t * dz)));
  }
  return best;
}

// The index of the first wanted type the building satisfies, or -1. The names resolve through the scout's one vocabulary
// (tools/scout/types.mjs TYPE_ALIASES), so a template and a dossier meet whichever form each uses.
const typeMatch = (types, b) => typeIndex(types, b.type, b.stem);

// Pure: does a door at ground level exist? A building with no door entity passes (its interior is unverified anyway).
const groundDoor = (b) => !b.entrances.length || b.entrances.some((e) => e.y - b.y <= 1.5);

// Pure: why a building cannot serve a slot, or null when it can. The interior gate lives here.
function eligible(b, slot) {
  const n = slot.needs;
  if (b.ruin) return "a ruin";
  if (n.interior === "required" && b.interior.value === "no") return "no interior";
  if (n.groundFloor && !groundDoor(b)) return "no ground-level door";
  if (n.types && typeMatch(n.types, b) < 0) return `type ${b.type} is not one of ${n.types.join(", ")}`;
  if (n.minArea && b.footprint.area < n.minArea) return `footprint ${b.footprint.area} m2 is under ${n.minArea}`;
  if (n.maxDistance && b.distance > n.maxDistance) return `${b.distance} m from the centre, over ${n.maxDistance}`;
  if (n.minDistance && b.distance < n.minDistance) return `${b.distance} m from the centre, under ${n.minDistance}`;
  return null;
}

// Pure: { score 0..1, why: [...] } for a building that is eligible.
function score(b, slot, ctx) {
  const n = slot.needs;
  const why = [];
  let type = 0.5;
  if (n.types) {
    const i = typeMatch(n.types, b);
    type = 1 - (0.6 * i) / Math.max(1, n.types.length);
    why.push(`type ${b.type}${i === 0 ? " (first choice)" : ""}`);
  }
  let prefer = 0.5;
  if (n.prefer === "large") prefer = clamp01(b.footprint.area / ctx.maxArea);
  else if (n.prefer === "near") prefer = clamp01(1 - b.distance / ctx.reach);
  else if (n.prefer === "far") prefer = clamp01(b.distance / ctx.reach);
  if (n.prefer) why.push(`${n.prefer}: ${b.footprint.area} m2, ${b.distance} m out`);
  let interior = 0.5;
  if (n.interior === "required") interior = b.interior.value === "yes" ? 1 : 0.3;
  else if (b.interior.value === "yes") interior = 0.6;
  if (n.interior === "required") why.push(`interior ${b.interior.value}`);
  const frontage = ctx.roads.length ? clamp01(1 - Math.min(...ctx.roads.map((r) => roadDistance(r, b.footprint.cx, b.footprint.cz))) / 60) : 0.5;
  return { score: r3(0.45 * type + 0.25 * prefer + 0.15 * interior + 0.15 * frontage), why };
}

const plotFits = (p, plot) => p.width >= plot.minWidth && p.depth >= plot.minDepth;

// ------------------------------------------------------------------ proposal

// Pure: the proposed mapping for a template on a dossier object (as read from dossier.json).
export function proposeMapping(dossierJson, template, opts = {}) {
  const probs = checkTemplate(template);
  if (probs.length) throw new Error(`template ${template?.kind} is not sound: ${probs[0]}`);
  const d = readDossier(dossierJson);
  const live = d.buildings.filter((b) => !b.ruin);
  const ctx = {
    roads: d.roads.filter((r) => Array.isArray(r.points) && r.points.length >= 2),
    maxArea: Math.max(1, ...live.map((b) => b.footprint.area)),
    reach: Math.max(60, ...live.map((b) => b.distance)),
  };

  // Candidates per building slot, best first.
  const cand = {};
  const why = {};
  for (const slot of template.slots) {
    if (slot.host !== "building") continue;
    const list = [];
    const rejected = [];
    for (const b of d.buildings) {
      const no = eligible(b, slot);
      if (no) {
        rejected.push(no);
        continue;
      }
      const s = score(b, slot, ctx);
      list.push({ b, ...s });
    }
    list.sort((a, b) => b.score - a.score || a.b.id.localeCompare(b.b.id));
    cand[slot.id] = list;
    why[slot.id] = rejected;
  }

  // Greedy by tightness (fewest candidates first, required first, then template order), with one augmenting step:
  // a slot with no free candidate asks the holder of an eligible building to move to another one.
  const order = template.slots
    .map((s, i) => ({ s, i }))
    .filter(({ s }) => s.host === "building")
    .sort((a, b) => Number(b.s.required) - Number(a.s.required) || cand[a.s.id].length - cand[b.s.id].length || a.i - b.i)
    .map(({ s }) => s);
  const holder = new Map(); // building id -> slot id
  const pick = {}; // slot id -> candidate entry
  const place = (slot) => {
    const free = cand[slot.id].find((c) => !holder.has(c.b.id));
    if (free) {
      holder.set(free.b.id, slot.id);
      pick[slot.id] = free;
      return true;
    }
    if (!slot.required) return false;
    for (const c of cand[slot.id]) {
      const other = holder.get(c.b.id);
      const alt = cand[other].find((x) => !holder.has(x.b.id));
      if (alt) {
        holder.set(alt.b.id, other);
        pick[other] = alt;
        holder.set(c.b.id, slot.id);
        pick[slot.id] = c;
        return true;
      }
    }
    return false;
  };
  for (const slot of order) place(slot);

  const usedPlots = new Set();
  const slots = {};
  const plotFor = (slot) => {
    if (!slot.plot) return null;
    const fits = d.plots.filter((p) => !usedPlots.has(p.id) && plotFits(p, slot.plot));
    if (!fits.length) return null;
    const far = slot.needs.prefer === "far";
    fits.sort((a, b) => (far ? b.distance - a.distance : a.distance - b.distance) || a.id.localeCompare(b.id));
    return fits[0];
  };
  const noneWhy = (slot) => {
    const parts = [`no building fits (${summarise(why[slot.id] ?? [])})`];
    parts.push(slot.plot ? "and no free plot is big enough" : "and the slot has no plot fallback");
    return parts.join(" ");
  };

  // Slots in template order for the output; plots are handed out in the same order.
  for (const slot of template.slots) {
    if (slot.host === "building") {
      const c = pick[slot.id];
      if (c) {
        const e = { building: c.b.id, score: c.score, why: c.why };
        if (c.b.interior.value === "unknown" && slot.needs.interior === "required") {
          e.verify = ["interior"];
          e.why = [...e.why, "interior unverified: run `look --interior` before fitting"];
        }
        const alts = cand[slot.id].filter((x) => x.b.id !== c.b.id).slice(0, 3).map((x) => ({ building: x.b.id, score: x.score }));
        if (alts.length) e.alternatives = alts;
        slots[slot.id] = e;
        continue;
      }
      const p = plotFor(slot);
      if (p) {
        usedPlots.add(p.id);
        const pre = slot.plot.prefabs[0];
        if (pre) {
          slots[slot.id] = { plot: p.id, prefab: { type: pre.type, stem: pre.stem }, why: [`no building fits (${summarise(why[slot.id])}); free plot ${p.id}, ${p.width} x ${p.depth} m, ${p.distance} m out`] };
          continue;
        }
      }
      slots[slot.id] = { none: true, reason: noneWhy(slot) };
    } else if (slot.host === "plot") {
      const p = plotFor(slot);
      if (p) {
        usedPlots.add(p.id);
        const pre = slot.plot.prefabs[0];
        slots[slot.id] = { plot: p.id, ...(pre ? { prefab: { type: pre.type, stem: pre.stem } } : {}), why: [`free plot ${p.id}, ${p.width} x ${p.depth} m, ${p.distance} m out`] };
      } else slots[slot.id] = { none: true, reason: "no free plot is big enough" };
    }
  }

  // Street slots: a shared frontage, the nearest building to the centre inside the slot's reach, else the nearest plot.
  for (const slot of template.slots) {
    if (slot.host !== "street") continue;
    const reach = slot.needs.maxDistance ?? Infinity;
    const b = live
      .filter((x) => x.distance <= reach)
      .sort((x, y) => x.distance - y.distance || x.id.localeCompare(y.id))[0];
    if (b) slots[slot.id] = { building: b.id, shared: true, why: [`street furniture on the frontage of ${b.id} (${b.type}), ${b.distance} m from the centre; the kit sets it against the kerb`] };
    else {
      const p = d.plots.filter((x) => x.distance <= reach).sort((x, y) => x.distance - y.distance)[0];
      if (p) slots[slot.id] = { plot: p.id, shared: true, prefab: null, why: [`no building within ${reach} m; open ground at plot ${p.id}`] };
      else slots[slot.id] = { none: true, reason: `no building or plot within ${reach} m of the centre` };
    }
  }

  // Output keys in template order.
  const ordered = {};
  for (const slot of template.slots) ordered[slot.id] = slots[slot.id];
  const missing = template.slots.filter((s) => s.required && (!ordered[s.id] || ordered[s.id].none)).map((s) => s.id);
  return {
    schema: MAPPING_SCHEMA,
    template: template.kind,
    place: d.place.slug,
    status: missing.length ? "incomplete" : "proposed",
    note: "A proposal. A human or the lead edits it; approval is a reviewed PR holding only this file (label town-mapping). `tools\\cli town map <place> --check` must pass first.",
    dossier: { schema: DOSSIER_SCHEMA, generated: dossierJson.generated?.at ?? null, buildings: d.buildings.length, freePlots: d.plots.length },
    ...(missing.length ? { missing } : {}),
    slots: ordered,
  };
}

function summarise(reasons) {
  const count = {};
  for (const r of reasons) {
    const k = r.replace(/\d+(\.\d+)?/g, "N");
    count[k] = (count[k] ?? 0) + 1;
  }
  const top = Object.entries(count).sort((a, b) => b[1] - a[1]).slice(0, 2).map(([k, n]) => `${n} x ${k}`);
  return top.length ? top.join(", ") : "none in the dossier";
}

// ------------------------------------------------------------------ check

// Pure: the problems that keep a mapping from being approved, [] when it can be.
export function checkMapping(mapping, dossierJson, template) {
  const out = [];
  if (!mapping || typeof mapping !== "object" || typeof mapping.slots !== "object" || !mapping.slots) return ["the mapping has no slots object"];
  if (mapping.schema !== MAPPING_SCHEMA) out.push(`mapping schema is ${mapping.schema}, expected ${MAPPING_SCHEMA}`);
  if (mapping.template !== template.kind) out.push(`mapping is for template '${mapping.template}', not '${template.kind}'`);
  const d = readDossier(dossierJson);
  if (mapping.place !== d.place.slug) out.push(`mapping is for place '${mapping.place}', the dossier is '${d.place.slug}'`);
  const byBuilding = new Map(d.buildings.map((b) => [b.id, b]));
  const byPlot = new Map(d.plots.map((p) => [p.id, p]));
  const slotsById = new Map(template.slots.map((s) => [s.id, s]));
  for (const id of Object.keys(mapping.slots)) if (!slotsById.has(id)) out.push(`slot ${id}: the ${template.kind} template has no such slot`);
  const claimed = new Map();
  for (const slot of template.slots) {
    const e = mapping.slots[slot.id];
    if (e === undefined) {
      if (slot.required) out.push(`slot ${slot.id}: required slot is missing from the mapping`);
      continue;
    }
    if (e.none) {
      if (slot.required) out.push(`slot ${slot.id}: required slot is none${e.reason ? ` (${e.reason})` : ""}`);
      else if (!e.reason) out.push(`slot ${slot.id}: none needs a reason`);
      continue;
    }
    if (e.building && e.plot) out.push(`slot ${slot.id}: names both a building and a plot`);
    if (!e.building && !e.plot) {
      out.push(`slot ${slot.id}: names neither a building nor a plot`);
      continue;
    }
    if (e.shared && slot.host !== "street") out.push(`slot ${slot.id}: only a street slot may share a building`);
    if (e.building) {
      const b = byBuilding.get(e.building);
      if (!b) {
        out.push(`slot ${slot.id}: building ${e.building} is not in the dossier`);
        continue;
      }
      if (b.ruin) out.push(`slot ${slot.id}: ${b.id} is a ruin`);
      if (slot.needs.interior === "required" && b.interior.value === "no") out.push(`slot ${slot.id}: ${b.id} has no interior and the slot needs one`);
      if (slot.needs.interior === "required" && b.interior.value === "unknown" && !(e.verify ?? []).includes("interior")) out.push(`slot ${slot.id}: ${b.id} has an unverified interior; the entry needs "verify": ["interior"]`);
      if (!e.shared) {
        if (claimed.has(b.id)) out.push(`slot ${slot.id}: building ${b.id} is already claimed by slot ${claimed.get(b.id)}`);
        else claimed.set(b.id, slot.id);
      }
    }
    if (e.plot) {
      const p = byPlot.get(e.plot);
      if (!p) out.push(`slot ${slot.id}: plot ${e.plot} is not in the dossier`);
      else if (slot.plot && !plotFits(p, slot.plot)) out.push(`slot ${slot.id}: plot ${p.id} is ${p.width} x ${p.depth} m, under the ${slot.plot.minWidth} x ${slot.plot.minDepth} the slot needs`);
      if (!e.shared && !(e.prefab && typeof e.prefab.stem === "string" && e.prefab.stem)) out.push(`slot ${slot.id}: a plot needs a prefab to place on it`);
      if (!e.shared) {
        const k = `plot:${e.plot}`;
        if (claimed.has(k)) out.push(`slot ${slot.id}: plot ${e.plot} is already claimed by slot ${claimed.get(k)}`);
        else claimed.set(k, slot.id);
      }
    }
  }
  return out;
}

// ------------------------------------------------------------------ CLI

const USAGE = "usage: tools\\cli town map <slug> [--template town|village] [--dossier file] [--out file] [--force] [--dry]\n       tools\\cli town map <slug> --check [--template kind] [--dossier file] [--out file]";

function parseArgs(args) {
  const flags = {};
  const pos = [];
  const valued = new Set(["template", "dossier", "out"]);
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      if (valued.has(k)) flags[k] = args[++i];
      else flags[k] = true;
    } else pos.push(a);
  }
  return { flags, pos };
}

// Pure: which template a place uses, and why. The flag wins; then the template an existing mapping was made for (the
// reviewed mapping is what --check must hold the place to, whatever the gazetteer called the place); then the template
// the dossier was scouted for (`scout --template`); then the gazetteer kind when it is a template name; else town.
export function chooseTemplate({ flag, mapping, dossier } = {}) {
  const isKind = (k) => typeof k === "string" && SLUG_RX.test(k);
  if (isKind(flag)) return { kind: flag, source: "--template" };
  if (isKind(mapping?.template)) return { kind: mapping.template, source: "the mapping's template" };
  if (isKind(dossier?.place?.template)) return { kind: dossier.place.template, source: "the dossier's template" };
  const k = dossier?.place?.kind;
  if (k === "town" || k === "village") return { kind: k, source: "the dossier's place kind" };
  return { kind: "town", source: "the default" };
}

const readJson = (file, what) => {
  if (!existsSync(file)) throw new Error(`${what} not found: ${file}`);
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch (e) {
    throw new Error(`${what} is not valid JSON (${file}): ${e.message}`);
  }
};

export async function main(args, { root }) {
  const { flags, pos } = parseArgs(args);
  const slug = pos[0];
  if (!slug || flags.help) {
    console.log(USAGE);
    return flags.help ? 0 : 1;
  }
  if (!SLUG_RX.test(slug)) {
    console.log(`REFUSE  '${slug}' is not a place slug (lowercase letters, digits and hyphens)\n${USAGE}`);
    return 1;
  }
  const placeDir = join(root, "docs", "everon", "places", slug);
  const dossierFile = resolve(root, flags.dossier || join(placeDir, "dossier.json"));
  const out = resolve(root, flags.out || join(placeDir, "mapping.json"));
  try {
    const dossier = readJson(dossierFile, "dossier");
    // An existing mapping names its template; a --check, or a proposal that re-reads one, follows it (--template still wins).
    let existing = null;
    if (existsSync(out)) {
      try {
        existing = JSON.parse(readFileSync(out, "utf8"));
      } catch {
        existing = null;
      }
    }
    const { kind, source } = chooseTemplate({ flag: flags.template, mapping: existing, dossier });
    const template = loadTemplate(kind, root);
    if (flags.check) {
      const mapping = readJson(out, "mapping");
      // The template's POI naming (card M17): every slot names a front kind with a POI row, or "none". `town check` (T4) calls the same.
      const probs = [...checkMapping(mapping, dossier, template), ...checkSlotPois(template, loadPoiKinds(root).kinds)];
      for (const p of probs) console.log(`FAIL  ${p}`);
      if (probs.length) {
        console.log(`SUMMARY town-map check fail ${slug}: ${probs.length} problem(s); ${relative(root, out)}`);
        return 1;
      }
      const n = Object.values(mapping.slots).filter((e) => e.building || e.plot).length;
      console.log(`SUMMARY town-map check ok ${slug}: ${n} slot(s) mapped, every required slot of ${kind} (${source}) has a home; ${relative(root, out)}`);
      return 0;
    }
    if (existsSync(out) && !flags.force && !flags.dry) {
      console.log(`REFUSE  ${relative(root, out)} exists and may be the reviewed mapping. Edit it by hand, or pass --force to replace it with a new proposal.`);
      return 1;
    }
    const mapping = proposeMapping(dossier, template);
    const text = `${JSON.stringify(mapping, null, 2)}\n`;
    if (flags.dry) {
      console.log(text);
    } else {
      mkdirSync(dirname(out), { recursive: true });
      writeFileSync(out, text);
    }
    for (const [id, e] of Object.entries(mapping.slots)) {
      const to = e.building ? `${e.building}${e.shared ? " (shared)" : ""}${e.verify ? " [verify interior]" : ""}` : e.plot ? `plot ${e.plot} + ${e.prefab?.stem ?? "no prefab"}` : `none: ${e.reason}`;
      console.log(`  ${id.padEnd(14)} ${to}`);
    }
    const missing = mapping.missing ?? [];
    console.log(`SUMMARY town-map ok ${slug}: template ${kind} (${source}), ${Object.values(mapping.slots).filter((e) => e.building || e.plot).length}/${template.slots.length} slots mapped, ${missing.length ? `INCOMPLETE (${missing.join(", ")} unmapped; --check will fail)` : "complete"}${flags.dry ? "" : `; ${relative(root, out)}`}`);
    return 0;
  } catch (e) {
    console.log(`SUMMARY town-map fail ${slug}: ${e.message}`);
    return 1;
  }
}

