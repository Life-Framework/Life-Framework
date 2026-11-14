// The addon an asset is written into (#971). One place decides it for every stage of tools\cli asset:
// the spec's `addon` field (default LifeFramework, the core), overridden by `--addon <name>` on any
// command. A pack such as LF_Everon1989 gets its models, metas, prefabs, clips, sounds and strings in
// its own folder; the core never learns the pack exists (docs/architecture/core-vs-pack.md).
// Playbook: docs/asset-pipeline/README.md "Target addon".
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ADDONS, CORE, addonDir, gprojOf, resolveAddon } from "../../lib/addons.mjs";
import { PACK, checkBoundary, packSurface } from "../../validation/validate-addon-boundary.mjs";
import { usedGuids } from "./meta.mjs";

export { ADDONS, CORE };

// The value after `--addon`, or undefined.
export function addonFlag(flags) {
  const i = flags.indexOf("--addon");
  if (i < 0) return undefined;
  const v = flags[i + 1];
  if (!v || v.startsWith("--")) throw new Error(`--addon needs an addon name: ${ADDONS.map((a) => a.name).join(", ")}`);
  return v;
}

// The canonical addon name for a spec (or any record with an `addon` field). Unknown names throw:
// a typo must not quietly write into the core.
export function addonOf(spec) {
  const asked = spec?.addon || CORE;
  const a = resolveAddon(asked);
  if (!a) throw new Error(`unknown addon "${asked}"; known: ${ADDONS.map((x) => x.name).join(", ")}`);
  return a.name;
}

export const isCore = (spec) => addonOf(spec) === CORE;

// The spec with `--addon` applied. No override returns the spec itself.
export function withAddon(spec, override) {
  if (override === undefined) return spec;
  const a = resolveAddon(override);
  if (!a) throw new Error(`unknown addon "${override}"; known: ${ADDONS.map((x) => x.name).join(", ")}`);
  return { ...spec, addon: a.name };
}

// addons/<name>/ for the spec (or kit, sound record, dialogue spec).
export const addonPath = (root, spec) => addonDir(root, addonOf(spec));

// `{GUID}path` references resolve by GUID, so a new resource must not reuse a GUID from ANY repo
// addon, not only the one it lands in.
export function guidsInUse(root) {
  const used = new Set();
  for (const a of ADDONS) if (existsSync(addonDir(root, a.name))) usedGuids(addonDir(root, a.name), used);
  return used;
}

// The Workbench import plugin is a generated, gitignored file. It always lives in the core: a pack
// project depends on the core, so one copy serves both, and a second copy under the pack would
// declare the class twice.
export const pluginDir = (root) => join(addonDir(root, CORE), "Scripts", "WorkbenchGame", "ResourceManager");

// The localization table of the addon (docs/architecture/core-vs-pack.md "Strings").
export const stringTable = (root, spec) => join(addonPath(root, spec), "Language", "everonlife_localization.st");

// Everything wrong with where a spec writes, as a list. Empty means the target is usable.
export function addonProblems(root, spec) {
  const out = [];
  let name;
  try { name = addonOf(spec); } catch (e) { return [e.message]; }
  if (name !== CORE && !existsSync(gprojOf(root, name))) out.push(`addon ${name} has no ${gprojOf(root, name)}`);
  if (name === CORE) {
    out.push(...packReferences(root, JSON.stringify(spec)).map((m) => `a core asset must not reference the pack: ${m}`));
  } else {
    // A pack resource at a core path would shadow the core's (the same resource path from two addons).
    const core = addonDir(root, CORE);
    if (spec.asset_dir && existsSync(join(core, spec.asset_dir))) out.push(`asset_dir ${spec.asset_dir} already exists in ${CORE}; a pack resource at the same path would shadow it`);
    // prefab_dir is shared by many assets, so it is the files the scaffolds write that collide.
    for (const prefix of ["", "Handgun_", "Magazine_", "Ammo_"]) {
      for (const suffix of ["", "_Item"]) {
        const rel = `${spec.prefab_dir}/${prefix}${spec.name}${suffix}.et`;
        if (spec.prefab_dir && existsSync(join(core, rel))) out.push(`${rel} already exists in ${CORE}; a pack prefab at the same path would shadow it`);
      }
    }
  }
  return out;
}

// What in `text` names the pack: its GUIDs, its project GUID, its addon name, a path only the pack holds.
// The same rule validate-addon-boundary applies to every core file.
export function packReferences(root, text) {
  const packDir = addonDir(root, PACK);
  if (!existsSync(packDir)) return text.includes(PACK) ? [`names the pack addon "${PACK}"`] : [];
  const pack = packSurface(packDir);
  const coreDir = addonDir(root, CORE);
  const out = [];
  if (text.includes(PACK)) out.push(`names the pack addon "${PACK}"`);
  for (const m of text.matchAll(/\{([0-9A-Fa-f]{16})\}/g)) {
    const owner = pack.guids.get(m[1].toUpperCase());
    if (owner) out.push(`pack GUID {${m[1]}} (${PACK}/${owner})`);
  }
  for (const p of pack.paths) {
    if (existsSync(join(coreDir, p))) continue; // a core file has the same path: not pack-only
    const i = text.indexOf(p);
    if (i >= 0 && (i === 0 || /["}\s]/.test(text[i - 1]))) out.push(`pack path ${p}`);
  }
  return out;
}

// After a stage wrote into the core: the repo's own boundary check, so no stage can leave a core file
// naming the pack. A pack target cannot break it (the pack may name the core), so it returns at once.
export function assertBoundary(root, spec) {
  if (!isCore(spec) || !existsSync(addonDir(root, PACK))) return;
  const errors = checkBoundary(join(root, "addons"));
  if (errors.length) throw new Error(`the core now references the pack (docs/architecture/core-vs-pack.md):\n  ${errors.slice(0, 8).join("\n  ")}`);
}

// A resource path for a credits row: bare for the core (the existing rows), prefixed for a pack.
export const creditPath = (spec, rel) => (isCore(spec) ? rel : `${addonOf(spec)}/${rel}`);

// The row the maintainer pastes into docs/architecture/core-pack-inventory.md "Assets" for a core asset.
// A pack asset needs none: the pack is not inventoried, and it is pack by where it lives.
export function inventoryRow(spec) {
  return `| \`${spec.asset_dir}/\` | core | ${spec.name}: classify with the red line (docs/architecture/core-vs-pack.md); change to pack or split if a server in another setting would not use it |`;
}

// The dry run: where each stage will write, with nothing written. Returns lines.
export function plan(root, spec) {
  const name = addonOf(spec);
  const dir = addonDir(root, name);
  const problems = addonProblems(root, spec);
  const lines = [
    `addon      ${name}${spec.addon ? "" : " (default)"}   project ${gprojOf(root, name)}`,
    `models     ${join(dir, spec.asset_dir)}/${spec.name}.fbx -> .xob, Data/ textures and .emat, every .meta`,
    `prefabs    ${join(dir, spec.prefab_dir)}/ ({GUID}${spec.prefab_dir}/... references stay inside ${name} or point at the base game)`,
    `anim/sound ${join(dir, "Anims")}, ${join(dir, "Sounds")}, ${join(dir, "Particles")}: whatever the anim and sound stages write`,
    `strings    ${stringTable(root, spec)}`,
    `workbench  -gproj ${gprojOf(root, name)}; import plugin ${join(pluginDir(root), "LF_AssetImportPlugin.c")} (always the core's copy)`,
    `guids      fresh GUIDs are drawn against every repo addon (${ADDONS.map((a) => a.name).join(", ")})`,
    `credits    docs/credits.md row names ${creditPath(spec, `${spec.asset_dir}/${spec.name}.xob`)}`,
    name === CORE ? `inventory  ${inventoryRow(spec)}` : "inventory  no row: the asset lives in the pack, which core-pack-inventory.md does not list",
    problems.length ? `FAIL       ${problems.join("\nFAIL       ")}` : "boundary   ok: no stage writes a core file that names the pack",
  ];
  return lines;
}

// The addon folders that hold a project file: what --addon accepts today.
export function knownAddons(root) {
  return ADDONS.filter((a) => existsSync(gprojOf(root, a.name))).map((a) => a.name);
}

// A slug's spec with `--addon` applied, for the commands that read their own record (sound, kit,
// ambience) and find the target through the asset's spec. A slug with no spec is the core.
export function slugTarget(root, slug, override) {
  const file = join(root, "tools", "assets", slug, "spec.json");
  return withAddon(existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : {}, override);
}
