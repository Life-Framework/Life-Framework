// tools/validation/validate-map-icon-unique.mjs - every place type on the map has its own icon (map clean-up 2026-11-22).
// inputs: addons/*/Configs/Map/**/LF_MapPois*.conf, addons/*/Language/*.st, tools/map/icons/build-icons.mjs, tools/map/lib/icons.mjs
//
// The maintainer: "some things share the same icons, they can't be allowed, each thing needs a different icon". The map
// key column lists one row per icon and type name, so two types on one quad look like one place. A type is a POI row
// that is not HIDDEN, named by its label's English text. It fails when:
//   - two types with different names resolve to the same quad (the row's m_sIcon, else poi_<Kind>);
//   - two pack badges draw the same glyph (build-icons KIND_BADGE), so two quads look the same;
//   - a type's quad has no pack badge (it would fall back to its category's glyph, which another type draws).
// red-proof: 2026-11-22, on the rows before this change it printed six "FAIL shared quad" lines (Farm/Farm yard,
// Seed and feed/Farm supply, Fuel depot/Filling station, General store/Supermarket, Timber merchant/Sawmill, Logging
// yard/Timber yard), "FAIL same glyph pickaxe", "FAIL same glyph wheat" and "FAIL no badge poi_BusStop"; the self-check
// below also feeds it a made-up pair on one quad and fails if that is not caught.
import { readFileSync, readdirSync, statSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function confFiles() {
  const out = [];
  const walk = (d) => {
    if (!existsSync(d)) return;
    for (const f of readdirSync(d)) {
      const p = join(d, f);
      if (statSync(p).isDirectory()) walk(p);
      else if (/^LF_MapPois.*\.conf$/.test(f)) out.push(p);
    }
  };
  for (const addon of readdirSync(join(ROOT, "addons"))) walk(join(ROOT, "addons", addon, "Configs", "Map"));
  return out;
}

function strings() {
  let all = "";
  for (const addon of readdirSync(join(ROOT, "addons"))) {
    const dir = join(ROOT, "addons", addon, "Language");
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) if (f.endsWith(".st")) all += readFileSync(join(dir, f), "utf8");
  }
  return all;
}

export function readTypes() {
  const st = strings();
  const text = (key) => {
    const id = key.replace(/^#/, "");
    const at = st.indexOf(`Id "${id}"\n`);
    if (at < 0) return key;
    const m = /Target_en_us "([^"]*)"/.exec(st.slice(at, at + 400));
    return m ? m[1] : key;
  };
  // Later files override earlier rows of the same kind (the pack overlays the core), as LF_ConfigOverlay does.
  const rows = new Map();
  for (const f of confFiles()) {
    const s = readFileSync(f, "utf8");
    for (const block of s.split("LF_MapPoiKind").slice(1)) {
      const get = (k) => {
        const m = new RegExp(`${k} "([^"]*)"`).exec(block) || new RegExp(`${k} (\\w+)`).exec(block);
        return m ? m[1] : "";
      };
      const kind = get("m_sKind");
      if (!kind) continue;
      const prev = rows.get(kind) || {};
      rows.set(kind, {
        kind,
        icon: get("m_sIcon") || prev.icon || `poi_${kind}`,
        label: get("m_sLabel") ? text(get("m_sLabel")) : prev.label || kind,
        hidden: (get("m_eVisibility") || prev.vis) === "HIDDEN",
        vis: get("m_eVisibility") || prev.vis,
      });
    }
  }
  return [...rows.values()].filter((r) => !r.hidden);
}

export function readBadges() {
  const src = readFileSync(join(ROOT, "tools/map/icons/build-icons.mjs"), "utf8");
  const m = /const KIND_BADGE = \{([\s\S]*?)\n\};/.exec(src);
  if (!m) throw new Error("build-icons.mjs: KIND_BADGE not found");
  const badges = new Map();
  for (const b of m[1].matchAll(/(\w+): \["(\w+)", "(\w+)"\]/g)) badges.set(b[1], b[2]);
  return badges;
}

export function faults(types, badges) {
  const bad = [];
  const byIcon = new Map();
  for (const t of types) {
    if (!byIcon.has(t.icon)) byIcon.set(t.icon, new Set());
    byIcon.get(t.icon).add(t.label);
  }
  for (const [icon, labels] of byIcon) {
    if (labels.size > 1) bad.push(`shared quad ${icon}: ${[...labels].join(" / ")}`);
    const kind = icon.replace(/^poi_/, "");
    if (!badges.has(kind)) bad.push(`no badge ${icon} (${[...labels].join(" / ")}): add it to KIND_BADGE and PACK_KINDS`);
  }
  const byGlyph = new Map();
  for (const [kind, glyph] of badges) {
    if (!byGlyph.has(glyph)) byGlyph.set(glyph, []);
    byGlyph.get(glyph).push(kind);
  }
  for (const [glyph, kinds] of byGlyph) if (kinds.length > 1) bad.push(`same glyph ${glyph}: ${kinds.join(", ")}`);
  return bad;
}

// Self-check: two made-up types on one quad and two badges on one glyph must be caught.
const probe = faults(
  [{ icon: "poi_A", label: "Gold mine" }, { icon: "poi_A", label: "Iron mine" }],
  new Map([["A", "pickaxe"], ["B", "pickaxe"]]),
);
if (probe.length !== 2) {
  console.log(`FAIL validate-map-icon-unique: the self-check found ${probe.length} of 2 planted faults`);
  process.exit(1);
}

const types = readTypes();
const bad = faults(types, readBadges());
if (bad.length) {
  for (const b of bad) console.log(`FAIL ${b}`);
  console.log(`validate-map-icon-unique: ${bad.length} place type(s) without an icon of their own (tools/map/icons/build-icons.mjs, the POI confs).`);
  process.exit(1);
}
console.log(`validate-map-icon-unique: OK (${types.length} public type rows, each name on its own badge)`);
