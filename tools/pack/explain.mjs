// tools\cli pack explain <key> (#969): which layer set the final value, offline.
//
//   pack explain catalogue.items.pistol_m9              every field of the entry, each with its source
//   pack explain catalogue.items.pistol_m9.price        one field
//   pack explain shops.priceRules.type:64,mode:2.buyPrice
//   pack explain shops.defaultBuyPrice                  a plain section key (core, packs, owner)
//   pack explain crime.crimes                           every id a pack or the owner file changed
//
// Layers, in order (docs/config/override-model.md): the core .conf, each pack's overlays in load
// order (the packs under addons/, or `--pack <addon>`), then the owner's file (`--config config.json`).
// A field is named the way the owner file names it (`m_iPrice` is `price`). The merge is the
// server's: add, replace, patch, remove by id; a refused verb and a second pack on one id are
// printed under the entry. Fields no layer sets keep the class default and are not listed.
// Exit 1 when the key names nothing. red-proof: tools/validation/pack-check-selftest.mjs.
import { join } from "node:path";
import { jsonKey } from "../config/schema.mjs";
import { SECTIONS, fieldsOf, layerName, loadCore, loadOwner, loadPackOverlays, mergeList, metaIndex, sectionDef, sectionLayers, sectionNames, show } from "./layers.mjs";
import { packDirs, resolvePack } from "./check.mjs";

const USAGE = `usage: tools\\cli pack explain <key> [--pack <addon>] [--config <config.json>] [--json]
  key: <section>.<list>.<id>[.<field>] or <section>.<key>
  lists: ${Object.keys(SECTIONS).join(", ")}; plain keys of: ${sectionNames().join(", ")}`;

const CAP = 30;

/** The source of one value, short: core, pack <name> (<file>), owner <file>. */
const src = (layer) => (layer.kind === "core" ? "core" : layer.kind === "owner" ? `owner ${layer.file}` : `pack ${layer.label} (${layer.file})`);

function parseKey(key) {
  for (const list of Object.keys(SECTIONS).sort((a, b) => b.length - a.length)) {
    if (key === list) return { list };
    if (key.startsWith(`${list}.`)) {
      const rest = key.slice(list.length + 1);
      const def = SECTIONS[list];
      return { list, rest, def };
    }
  }
  const dot = key.indexOf(".");
  if (dot > 0 && sectionDef(key.slice(0, dot))) return { scalar: key.slice(dot + 1), section: key.slice(0, dot) };
  return null;
}

function layersFor(root, packArg, ownerPath, def) {
  const addonsDir = join(root, "addons");
  const meta = metaIndex(addonsDir);
  const names = packArg ? [resolvePack(addonsDir, packArg)].filter(Boolean) : packDirs(addonsDir);
  if (packArg && !names.length) return { error: `no pack '${packArg}' in addons/` };
  const owner = loadOwner(ownerPath);
  if (owner.error) return { error: owner.error };
  owner.file = ownerPath ? ownerPath.split(/[\\/]/).pop() : "config.json";
  const packs = names.map((p) => ({ pack: p, overlays: loadPackOverlays(addonsDir, p, meta).overlays }));
  return { addonsDir, packs, owner };
}

function explainEntry(list, id, field, merged, out) {
  const e = merged.entries.get(id);
  const touched = merged.events.filter((v) => v.id === id);
  if (!e) {
    out.push(`${list} ${id}: no layer has this id`);
    return false;
  }
  out.push(`${list} ${id}: ${e.present ? "present" : `removed by ${src(e.trail[e.trail.length - 1].layer)}`}`);
  const names = field ? [field] : [...e.fields.keys()];
  for (const n of names) {
    const f = e.fields.get(n);
    if (!f) { out.push(`  ${n}: set by no layer (class default)`); continue; }
    const was = f.was.map((w) => `${w.layer.kind === "pack" ? w.layer.label : w.layer.kind} ${show(w.value)}`).join(", ");
    out.push(`  ${n} = ${show(f.value)}  <- ${src(f.layer)}${was ? `  (overridden: ${was})` : ""}`);
  }
  out.push(`  trail: ${e.trail.map((t) => `${t.layer.kind === "pack" ? t.layer.label : t.layer.kind} ${t.op}`).join(" > ")}`);
  for (const v of touched.filter((x) => x.result === "refused")) out.push(`  refused: ${src(v.layer)} ${v.op}: ${v.why}`);
  for (const c of merged.conflicts.filter((x) => x.id === id)) out.push(`  conflict: ${c.second} changes an id ${c.first} already changed; ${c.second} wins`);
  return true;
}

export function explain(root, key, opts = {}) {
  const k = parseKey(key);
  if (!k) return { code: 2, lines: [`pack explain: '${key}' is not <section>.<list>.<id> or <section>.<key>`, USAGE] };
  const L = layersFor(root, opts.pack, opts.config, k.def);
  if (L.error) return { code: 2, lines: [`pack explain: ${L.error}`] };
  const out = [];

  if (k.scalar) {
    const def = sectionDef(k.section);
    const hits = [];
    const core = loadCore(L.addonsDir, def);
    const take = (props, layer) => {
      for (const [name, v] of Object.entries(fieldsOf(props ?? {}, Object.values(SECTIONS).map((s) => s.field)))) {
        if (name === k.scalar) hits.push({ layer, value: v });
      }
    };
    if (core) take(core.props, { kind: "core", label: "core", file: def.core });
    for (const p of L.packs) for (const o of p.overlays) if (o.root.cls === def.cls) take(o.root.props, { kind: "pack", label: p.pack, file: o.file });
    const ownerSection = L.owner.json?.[k.section];
    if (ownerSection && Object.hasOwn(ownerSection, k.scalar)) hits.push({ layer: { kind: "owner", file: L.owner.file }, value: ownerSection[k.scalar] });
    if (!hits.length) return { code: 1, lines: [`${key}: no layer sets it (class default)`] };
    const last = hits[hits.length - 1];
    out.push(`${key} = ${show(last.value)}  <- ${src(last.layer)}`);
    for (const h of hits.slice(0, -1)) out.push(`  overridden: ${show(h.value)} from ${src(h.layer)}`);
    return { code: 0, lines: out, data: { key, value: last.value, source: src(last.layer), layers: hits.map((h) => ({ source: src(h.layer), value: h.value })) } };
  }

  const def = SECTIONS[k.list];
  const merged = mergeList(sectionLayers(L.addonsDir, def, L.packs, L.owner));
  if (!k.rest) {
    const changed = [...merged.entries.values()].filter((e) => e.trail.some((t) => t.layer.kind !== "core"));
    out.push(`${k.list}: ${merged.entries.size} id(s), ${changed.length} changed by a pack or the owner`);
    for (const e of changed.slice(0, CAP)) out.push(`  ${e.id}  ${e.present ? "" : "(removed) "}${e.trail.map((t) => `${t.layer.kind === "pack" ? t.layer.label : t.layer.kind} ${t.op}`).join(" > ")}`);
    if (changed.length > CAP) out.push(`  ... ${changed.length - CAP} more`);
    for (const v of merged.events.filter((x) => x.result === "refused" && x.layer.kind !== "core").slice(0, CAP)) out.push(`  refused: ${src(v.layer)} ${v.id || "(no id)"} ${v.op}: ${v.why}`);
    return { code: 0, lines: out };
  }
  // <id> or <id>.<field>: the id may itself hold dots (a prefab path), so the whole rest is tried first.
  const cut = merged.entries.has(k.rest) ? -1 : k.rest.lastIndexOf(".");
  const id = cut < 0 ? k.rest : k.rest.slice(0, cut);
  const field = cut < 0 ? "" : jsonKey(k.rest.slice(cut + 1));
  const found = explainEntry(k.list, id, field, merged, out);
  if (!found) {
    const near = [...merged.entries.keys()].filter((i) => i.toLowerCase().includes(id.toLowerCase().slice(0, 4))).slice(0, 5);
    if (near.length) out.push(`  similar ids: ${near.join(", ")}`);
  }
  return { code: found ? 0 : 1, lines: out };
}

export async function main(args, ctx) {
  const val = (f) => { const i = args.indexOf(f); return i >= 0 ? args[i + 1] : undefined; };
  const skip = new Set(["--pack", "--config"].flatMap((f) => (args.includes(f) ? [args.indexOf(f) + 1] : [])));
  const key = args.find((a, i) => !a.startsWith("--") && !skip.has(i));
  if (!key) { console.log(USAGE); return 2; }
  const r = explain(ctx.root, key, { pack: val("--pack"), config: val("--config") });
  if (args.includes("--json")) console.log(JSON.stringify({ key, code: r.code, lines: r.lines, ...(r.data ?? {}) }, null, 2));
  else for (const l of r.lines) console.log(l);
  return r.code;
}
