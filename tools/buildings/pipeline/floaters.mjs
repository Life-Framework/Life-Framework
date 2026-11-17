// tools\cli building check <slug> [--file <prefab.et|layer>] [--json]: no small prop of a building floats, offline.
//
// For every child of the building's prefabs (addons/<addon>/<prefab_dir>/**/*.et: furniture, store, fixtures): a small prop
// (surface.mjs SMALL) must have its bottom within 2 cm of the top of the support under its footprint centre, or of a
// floor slab. A prop that hangs 2 cm or more over a top, sits in one, or has nothing under it is listed with the entity
// ID, where it is, and what to change. Hung kinds (posters, notices, clocks, ceiling lamps) are skipped. Needs only the
// paks (the same reader `grid place` sizes by), no Workbench: about a second warm.
//
// Supports outside the prefabs: the host building's colliders count, and spec.check_supports lists a counter a layer places
//   "check_supports": [{ "prefab": "Prefabs/...et", "at": [x, y, z], "yaw": 0, "why": "..." }]
// A prop that is meant to look so (a hanging pan, a prop leaning on a wall) goes in the spec:
//   "floaters_allow": [{ "match": "<entity ID or a regex on the prefab path>", "why": "<reason>" }]
// `floors` in the spec overrides the floor heights read from the host building's colliders.
//
// --fix moves each prop that floats over or sits in a top onto it (writes the prefab; a generator is the better home: use
// snapItems in it). Nothing under a prop is left for a person: a prop whose support was removed, or a hanging kind this list does not know.
// Exit: 0 clean, 1 a prop floats, 2 usage or the game files are not readable here (a runner without the game skips).
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { addonDir } from "../../lib/addons.mjs";
import { baseName, floaters, floorLevels, gameReader, prefabChildren, setHeights, specSupports } from "./surface.mjs";

// Every prefab of the building (its furniture, its store with the counters and fixtures): they share building space.
export function furnitureFiles(root, spec) {
  const dir = join(addonDir(root, spec.addon), spec.prefab_dir || "");
  if (!spec.prefab_dir || !existsSync(dir)) return [];
  const out = [];
  const walk = (d) => {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      if (e.isDirectory()) walk(join(d, e.name));
      else if (e.name.endsWith(".et")) out.push(join(d, e.name));
    }
  };
  walk(dir);
  return out;
}

const hostPath = (spec) => String(spec.vanilla?.base ?? "").replace(/^\{[0-9A-Fa-f]{16}\}/, "");

// Floors of a building: the spec's own list, else the host's large slabs, else the heights several big props stand at.
export async function floorsOf(spec, ents, reader) {
  if (Array.isArray(spec.floors) && spec.floors.length) return spec.floors;
  if (hostPath(spec)) {
    const f = await floorLevels(hostPath(spec), { reader });
    if (f.length) return f;
  }
  const counts = new Map();
  for (const e of ents) {
    const info = e.path && reader.info(e.path);
    if (!info) continue;
    const k = e.scale ?? info.scale ?? 1;
    if ((info.bounds.max[1] - info.bounds.min[1]) * k <= 0.8) continue;
    const y = Math.round((e.y + info.bounds.min[1] * k) * 100) / 100;
    counts.set(y, (counts.get(y) ?? 0) + 1);
  }
  return [...counts].filter(([, n]) => n >= 3).map(([y]) => y);
}

export async function checkBuilding(root, slug, { file = null, reader = gameReader(root) } = {}) {
  const specFile = join(root, "tools", "buildings", slug, "spec.json");
  const spec = existsSync(specFile) ? JSON.parse(readFileSync(specFile, "utf8")) : { addon: "LF_Everon1989", prefab_dir: "" };
  const files = file ? [file] : furnitureFiles(root, spec);
  if (!files.length) return { spec, files, ents: [], problems: [], floors: [], none: true };
  const ents = files.flatMap((f) => prefabChildren(readFileSync(f, "utf8")).map((e) => ({ ...e, file: f })));
  const floors = await floorsOf(spec, ents, reader);
  const problems = await floaters(ents, { reader, floors, allow: spec.floaters_allow ?? [], extra: specSupports(spec) });
  return { spec, files, ents, problems, floors };
}

export async function cmdCheck(root, args) {
  const slug = args.find((a) => !a.startsWith("--"));
  if (!slug) {
    console.log("usage: tools\\cli building check <slug> [--file <prefab.et>] [--json] [--fix]   no small prop floats: bottom within 2 cm of the top under it (offline)");
    return 2;
  }
  const at = args.indexOf("--file");
  const fix = args.includes("--fix");
  const t0 = Date.now();
  let r;
  try {
    r = await checkBuilding(root, slug, { file: at >= 0 ? args[at + 1] : null });
  } catch (e) {
    console.log(`SUMMARY building check skip ${slug}: ${e.message}`);
    return 2;
  }
  if (r.none) {
    console.log(`SUMMARY building check skip ${slug}: no prefab under ${r.spec.prefab_dir || "(no prefab_dir in the spec)"}`);
    return 2;
  }
  if (fix) {
    // Move every prop that floats over or sits in a top onto it (a prop with nothing under it needs a person). A stack settles
    // from the bottom up, so go again while something moves.
    let moved = 0;
    for (let round = 0; round < 6; round++) {
      const byFile = new Map();
      for (const p of r.problems.filter((q) => q.to != null && q.id)) byFile.set(p.file, [...(byFile.get(p.file) ?? []), p]);
      let n = 0;
      for (const [file, list] of byFile) {
        const res = setHeights(readFileSync(file, "utf8"), list);
        if (res.done) writeFileSync(file, res.text);
        n += res.done;
        for (const p of list) console.log(`  fixed ${baseName(p.path)} ${p.id} y ${p.y} -> ${p.to} (${p.support})`);
      }
      moved += n;
      if (!n) break;
      r = await checkBuilding(root, slug, { file: at >= 0 ? args[at + 1] : null });
    }
    console.log(`building check --fix: moved ${moved} prop(s) onto their surface`);
  }
  if (args.includes("--json")) console.log(JSON.stringify({ slug, floors: r.floors, problems: r.problems }, null, 1));
  for (const p of r.problems.slice(0, 60)) console.log(`  ${p.id || "(no id)"} ${baseName(p.path)} at ${p.x} ${p.y} ${p.z}: ${p.text}`);
  if (r.problems.length > 60) console.log(`  ... ${r.problems.length - 60} more`);
  const secs = ((Date.now() - t0) / 1000).toFixed(1);
  const props = r.ents.length;
  if (r.problems.length) {
    console.log(`SUMMARY building check fail ${slug}: ${r.problems.length} floating or sunk prop(s) of ${props} entities (floors ${r.floors.join(", ") || "none"}), ${secs}s. Stand a prop on its surface with surface.mjs (on: <id> or the auto snap), or list it in spec.json floaters_allow with a reason`);
    return 1;
  }
  console.log(`SUMMARY building check ok ${slug}: ${props} entities, every small prop stands on a top or a floor (floors ${r.floors.join(", ") || "none"}), ${secs}s`);
  return 0;
}
