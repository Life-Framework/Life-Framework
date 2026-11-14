// tools\cli pack check <addon> (#969): what a pack does to a server, checked offline before it ships.
//
// Reports each finding with the pack file and the id:
//   manifest   the version range, the manifest, the public surface   (validate-pack-manifest)
//   boundary   no script in the pack, the core never names it        (validate-addon-boundary)
//   header     the mission header names the manifest and loads real overlays; an overlay no header loads
//   ids        add of an id the lower layers have; replace, patch or remove of one they lack; an unknown
//              verb; a record with no id; an offer for an item the merged catalogue lacks
//   config     a section the pack adds (#553) that the server ignores: a core name, outside the pack
//              namespace, a bad field type, default or range (tools/config/pack-sections.mjs)
//   packs      a second pack changing an id an earlier pack changed (a warning naming both)
//   guids      a {GUID}path in an overlay that names another path, or a stale GUID of a file in this repo
//   strings    a #Key in an overlay that no string table of the core or the pack defines
// Base-game {GUID}path references cannot be resolved offline; they are counted, not listed.
// Not covered yet (own issues): dialogue #957, terms #958, map independence #961, profit loops
// over pack prices #686. Exit 1 on any error; --strict fails on warnings too.
//
// Output is quiet: one summary line, then findings (20 at most; --all lists the rest, --json for tools).
// red-proof: tools/validation/pack-check-selftest.mjs plants one error of each kind.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { checkPackManifest } from "../validation/validate-pack-manifest.mjs";
import { checkBoundary } from "../validation/validate-addon-boundary.mjs";
import { coreKeysOf, readScripts } from "../config/schema.mjs";
import { loadPackSections } from "../config/pack-sections.mjs";
import { CORE, SECTIONS, guidOf, loadPackOverlays, metaIndex, mergeList, pathOfRes, rel, sectionLayers, strings, walk } from "./layers.mjs";

const CAP = 20;

/** String-table ids of every .st under an addon. */
function stringIds(addonDir) {
  const ids = new Set();
  for (const f of walk(join(addonDir, "Language")).filter((p) => p.endsWith(".st"))) {
    for (const m of readFileSync(f, "utf8").matchAll(/^\s*Id\s+"([^"]+)"/gm)) ids.add(m[1]);
  }
  return ids;
}

/** The core's own top-level owner keys, read from its config classes; just schemaVersion without scripts. */
export function coreKeysFor(addonsDir) {
  const scripts = join(addonsDir, CORE, "Scripts", "Game");
  return existsSync(scripts) ? coreKeysOf(readScripts(scripts)) : new Set(["schemaVersion"]);
}

export function packDirs(addonsDir) {
  return readdirSync(addonsDir).filter((n) => n.startsWith("LF_") && existsSync(join(addonsDir, n, `${n}.gproj`))).sort();
}

/** `Everon1989`, `LF_Everon1989` or addons/LF_Everon1989 to the folder name, or null. */
export function resolvePack(addonsDir, arg) {
  const name = String(arg ?? "").replace(/[\\/]+$/, "").split(/[\\/]/).pop();
  for (const c of [name, `LF_${name}`]) if (c && existsSync(join(addonsDir, c, `${c}.gproj`)) && c.startsWith("LF_")) return c;
  return null;
}

/** Runs every check on one pack. Returns { pack, findings: [{level, area, file, id, msg}], unchecked }. */
export function checkPack(root, pack) {
  const addonsDir = join(root, "addons");
  const dir = join(addonsDir, pack);
  const findings = [];
  const add = (level, area, file, id, msg) => findings.push({ level, area, file, id, msg });

  for (const e of checkPackManifest(root)) {
    if (!e.startsWith(`${pack}/`) && !e.startsWith(`${pack} `)) continue;
    const m = new RegExp(`^${pack}/(\\S+?):?\\s+(.*)$`).exec(e);
    if (m) add("error", "manifest", m[1], "", m[2]);
    else add("error", "manifest", "", "", e);
  }
  for (const e of checkBoundary(addonsDir)) if (e.includes(pack)) add("error", "boundary", "", "", e);

  const meta = metaIndex(addonsDir);
  const loaded = loadPackOverlays(addonsDir, pack, meta);
  for (const p of loaded.problems) add("error", "header", p.file, p.id, p.msg);
  for (const f of loaded.unlisted) add("warn", "header", f, "", `overlay is not in m_aLF_ConfigOverlays of ${loaded.header ?? "the mission header"}; the server will not load it`);
  const manifestGuid = guidOf(loaded.manifestRef);
  if (loaded.header) {
    const hit = manifestGuid ? meta.get(manifestGuid) : null;
    if (!hit || hit.addon !== pack) add("error", "header", loaded.header, "m_LF_PackManifest", `the header names ${loaded.manifestRef ?? "no manifest"}, which is not a resource of this pack`);
  }

  // config: sections this pack adds to the owner file; the server ignores one with a problem.
  const coreKeys = coreKeysFor(addonsDir);
  const mine0 = loadPackSections(addonsDir, coreKeys).filter((p) => p.pack === pack);
  const claimed = new Map(); // section name -> the other pack that declares it
  for (const other of loadPackSections(addonsDir, coreKeys)) {
    if (other.pack !== pack) for (const s of other.sections) if (!s.problem) claimed.set(s.name, other.pack);
  }
  for (const pk of mine0) {
    for (const s of pk.sections) {
      if (s.problem) add("error", "config", pk.file, s.name || "(no name)", `${s.problem}; the server ignores this section`);
      else if (claimed.has(s.name)) add("error", "config", pk.file, s.name, `pack ${claimed.get(s.name)} declares section ${s.name} too`);
    }
  }

  // ids: the merge of every section this pack overlays, packs after the core, as the server does it.
  const others = packDirs(addonsDir).filter((p) => p !== pack);
  const mine = { pack, overlays: loaded.overlays };
  const touched = new Set(loaded.overlays.map((o) => o.root.cls));
  let merged = null;
  for (const [list, def] of Object.entries(SECTIONS)) {
    if (!touched.has(def.cls)) continue;
    const layers = sectionLayers(addonsDir, def, [...others.map((p) => ({ pack: p, overlays: loadPackOverlays(addonsDir, p, meta).overlays })), mine], null);
    const r = mergeList(layers);
    if (list === "catalogue.items") merged = r;
    for (const ev of r.events) {
      if (ev.layer.kind !== "pack" || ev.layer.label !== pack || ev.result !== "refused") continue;
      add("error", "ids", ev.layer.file, `${list} ${ev.id || "(no id)"}`, ev.why);
    }
    for (const c of r.conflicts) {
      if (c.second === pack || c.first === pack) add("warn", "packs", "", `${list} ${c.id}`, `${c.second} changes an id ${c.first} already changed; ${c.second} wins`);
    }
    // Duplicate ids inside one overlay are the same refusal; offers must name a catalogue item.
    if (list === "catalogue.offers") {
      const items = (merged ?? mergeList(sectionLayers(addonsDir, SECTIONS["catalogue.items"], [mine], null))).entries;
      for (const ev of r.events.filter((e) => e.layer.kind === "pack" && e.layer.label === pack && (e.result === "added" || e.result === "replaced"))) {
        const item = ev.id.split("/").slice(1).join("/");
        if (item && !items.get(item)?.present) add("error", "ids", ev.layer.file, `${list} ${ev.id}`, `the offer names item "${item}", which the merged catalogue.items does not have`);
      }
    }
  }

  // guids and strings, over every string value in the pack's overlays.
  const known = new Set([...stringIds(join(addonsDir, CORE)), ...stringIds(dir)]);
  let unchecked = 0;
  for (const o of loaded.overlays) {
    for (const s of new Set(strings(o.root))) {
      const g = /^\{([0-9A-Fa-f]{16})\}(.*)$/.exec(s);
      if (g) {
        const hit = meta.get(g[1].toUpperCase());
        if (hit) {
          if (hit.path !== g[2]) add("error", "guids", o.file, s, `GUID {${g[1]}} is ${hit.addon}/${hit.path}, the reference says ${g[2]}`);
        } else if (g[2] && [CORE, pack].some((a) => existsSync(join(addonsDir, a, g[2])))) {
          add("error", "guids", o.file, s, `GUID {${g[1]}} is in no .meta, but ${g[2]} exists in this repo; the reference is stale`);
        } else unchecked++;
      } else if (s.startsWith("#") && /^#[A-Za-z]/.test(s) && !known.has(s.slice(1))) {
        add("error", "strings", o.file, s, `string key ${s.slice(1)} is in no string table of ${CORE} or ${pack}`);
      }
    }
  }
  return { pack, findings, unchecked };
}

export function summary(r) {
  const errors = r.findings.filter((f) => f.level === "error").length;
  const warns = r.findings.length - errors;
  return { errors, warns, line: `pack check ${r.pack}: ${errors ? `${errors} error(s)` : "OK"}${warns ? `, ${warns} warning(s)` : ""}${r.unchecked ? `, ${r.unchecked} base-game GUID(s) not checkable offline` : ""}` };
}

const USAGE = "usage: tools\\cli pack check <addon> [--strict] [--all] [--json]   (addon: Everon1989, LF_Everon1989 or its folder)";

export async function main(args, ctx) {
  const flags = new Set(args.filter((a) => a.startsWith("--")));
  const target = args.find((a) => !a.startsWith("--"));
  if (!target) { console.log(USAGE); return 2; }
  const root = ctx?.root;
  const pack = resolvePack(join(root, "addons"), target);
  if (!pack) {
    console.log(`pack check: no pack '${target}' in addons/ (found: ${packDirs(join(root, "addons")).join(", ") || "none"})`);
    return 2;
  }
  const r = checkPack(root, pack);
  const s = summary(r);
  const full = flags.has("--all") || process.env.LF_OUTPUT === "full";
  if (flags.has("--json")) console.log(JSON.stringify({ pack, errors: s.errors, warnings: s.warns, unchecked: r.unchecked, findings: r.findings }, null, 2));
  else {
    console.log(s.line);
    const order = [...r.findings].sort((a, b) => (a.level === b.level ? 0 : a.level === "error" ? -1 : 1));
    for (const f of full ? order : order.slice(0, CAP)) {
      console.log(`${f.level === "error" ? "ERROR" : "WARN "} ${f.area.padEnd(8)} ${[f.file, f.id].filter(Boolean).join("  ")}: ${f.msg}`);
    }
    if (!full && order.length > CAP) console.log(`... ${order.length - CAP} more (--all)`);
  }
  return s.errors || (flags.has("--strict") && s.warns) ? 1 : 0;
}
