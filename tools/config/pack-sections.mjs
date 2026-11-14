// Pack config sections (#553), read offline: a pack lists the owner-config sections it adds in its
// manifest `.conf` (LF_PackManifest.m_aConfigSections, a pack has no script). This file reads them for
// the schema generator (tools/config/schema.mjs), `pack check` and `pack explain`, and mirrors the
// runtime's rules (LF_PackConfigSections.Problem in Feature/Packs/LF_PackConfigSection.c):
//   - the name is the pack namespace (`LF_Everon1989` is `everon1989`), alone or continued by an
//     upper-case word (`everon1989Chains`);
//   - a core section name is refused, as is a name declared twice; a refused section is ignored;
//   - fields are int, float, bool or string, each with a default of that type; min and max bound a number.
// Red-proof: tools/validation/config-schema-selftest.mjs and pack-check-selftest.mjs.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf } from "../economy/profit-loops.mjs";

export const FIELD_TYPES = { int: "integer", float: "number", bool: "boolean", string: "string" };

/** `LF_Everon1989` is `everon1989`. */
export const namespaceOf = (packId) => String(packId ?? "").replace(/^LF_/, "").toLowerCase();

const isNum = (s) => /^-?\d+(\.\d+)?$/.test(s);
const isWhole = (s) => /^-?\d+$/.test(s);
const text = (v) => (v === undefined || v === null ? "" : String(v));

/** Why a declared section cannot be used, or "" when it holds. `coreKeys` are the core's own top-level keys. */
export function sectionProblem(section, ns, coreKeys, seen = new Set()) {
  const name = text(section.name);
  if (!name) return "a section has no name";
  if (coreKeys.has(name)) return `section ${name} is a core section name`;
  if (!ns || !name.startsWith(ns)) return `section ${name} does not start with the pack namespace ${ns}`;
  if (name.length > ns.length && !/^[A-Z]/.test(name.slice(ns.length))) return `section ${name} must be the namespace ${ns} or continue it with an upper-case word`;
  if (seen.has(name)) return `section ${name} is declared twice`;
  if (!section.fields.length) return `section ${name} declares no fields`;
  const keys = new Set();
  for (const f of section.fields) {
    if (!f.key) return `section ${name} has a field with no key`;
    if (keys.has(f.key)) return `section ${name} declares field ${f.key} twice`;
    keys.add(f.key);
    if (!FIELD_TYPES[f.type]) return `${name}.${f.key} has type ${f.type}; use int, float, bool or string`;
    const numeric = f.type === "int" || f.type === "float";
    if (f.type === "int" && !isWhole(f.def)) return `${name}.${f.key} default ${f.def} is not a whole number`;
    if (f.type === "float" && !isNum(f.def)) return `${name}.${f.key} default ${f.def} is not a number`;
    if (f.type === "bool" && f.def !== "true" && f.def !== "false") return `${name}.${f.key} default ${f.def} is not true or false`;
    if (!numeric && (f.min !== "" || f.max !== "")) return `${name}.${f.key} has a min or max but is not a number`;
    if (numeric && [f.min, f.max].some((b) => b !== "" && !isNum(b))) return `${name}.${f.key} has a min or max that is not a number`;
    if (numeric && f.min !== "" && f.max !== "" && Number(f.min) > Number(f.max)) return `${name}.${f.key} has a min above its max`;
    if (numeric && f.min !== "" && Number(f.def) < Number(f.min)) return `${name}.${f.key} default ${f.def} is below its min`;
    if (numeric && f.max !== "" && Number(f.def) > Number(f.max)) return `${name}.${f.key} default ${f.def} is above its max`;
  }
  return "";
}

/** The sections a manifest root node declares, raw: [{ name, desc, fields: [{ key, type, def, min, max, desc }] }]. */
export function sectionsOf(manifestRoot) {
  const list = manifestRoot?.props?.m_aConfigSections?.list ?? [];
  return list.map((n) => ({
    name: text(n.props?.m_sName),
    desc: text(n.props?.m_sDesc),
    fields: (n.props?.m_aFields?.list ?? []).map((f) => ({
      key: text(f.props?.m_sKey), type: text(f.props?.m_sType) || "int", def: text(f.props?.m_sDefault),
      min: text(f.props?.m_sMin), max: text(f.props?.m_sMax), desc: text(f.props?.m_sDesc),
    })),
  }));
}

function* confs(dir) {
  if (!existsSync(dir)) return;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    if (e.name === "log" || e.name === ".git") continue;
    const p = join(dir, e.name);
    if (e.isDirectory()) yield* confs(p);
    else if (e.name.endsWith(".conf")) yield p;
  }
}

/**
 * Every pack under addonsDir (an `LF_*` addon with a .gproj) with its manifest's sections:
 * [{ pack, ns, file, sections: [{ name, desc, fields, problem }] }]. A pack without a manifest or
 * sections gives an empty list. `coreKeys` is a Set of the core's top-level keys (section names and
 * "schemaVersion"); the problem of each section is computed against it.
 */
export function loadPackSections(addonsDir, coreKeys) {
  const out = [];
  if (!existsSync(addonsDir)) return out;
  for (const pack of readdirSync(addonsDir).filter((n) => n.startsWith("LF_") && existsSync(join(addonsDir, n, `${n}.gproj`))).sort()) {
    for (const f of confs(join(addonsDir, pack))) {
      const raw = readFileSync(f, "utf8");
      if (!/^\s*LF_PackManifest\b/m.test(raw)) continue;
      const root = parseConf(raw);
      if (root?.cls !== "LF_PackManifest") continue;
      const ns = namespaceOf(root.props.m_sPackId || pack);
      const seen = new Set();
      const sections = sectionsOf(root).map((s) => {
        const problem = sectionProblem(s, ns, coreKeys, seen);
        if (!problem) seen.add(s.name);
        return { ...s, problem };
      });
      out.push({ pack, ns, file: f.slice(join(addonsDir, pack).length + 1).split(/[\\/]/).join("/"), sections });
    }
  }
  return out;
}

const typed = (f) => (f.type === "bool" ? f.def === "true" : f.type === "string" ? f.def : Number(f.def));

/** The JSON-schema node of one valid pack section. */
export function sectionSchema(pack, section) {
  const properties = {};
  for (const f of section.fields) {
    const s = { type: FIELD_TYPES[f.type], default: typed(f) };
    if (f.desc) s.description = f.desc;
    if (f.min !== "") s.minimum = Number(f.min);
    if (f.max !== "") s.maximum = Number(f.max);
    properties[f.key] = s;
  }
  return {
    type: "object",
    description: `Pack ${pack}: ${section.desc.replace(/\.?$/, ".") === "." ? `${section.name} section.` : section.desc.replace(/\.?$/, ".")}`,
    properties, additionalProperties: false, "x-pack": pack,
  };
}
