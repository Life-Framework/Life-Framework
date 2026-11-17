// tools/validation/validate-map-poi-visibility.mjs
// inputs: addons/*/Configs/Map/LF_MapPois*.conf
//
// An underworld place is never public (everon-map.md decision D-M5): the server never puts a fence or a
// dealer on every player's map, an org shares one by hand. LF_MapPoiKind's `m_eVisibility` defaults to
// PUBLIC (0), so a row that says `m_sCategory "underworld"` and leaves the visibility out, or sets
// anything but HIDDEN, would publish it. This fails such a row. A pack overlay row that patches a kind
// the core (or another file) calls underworld, and states a visibility other than HIDDEN, fails too.
// Do not change the attribute default: it would hide every public row that omits it.
// red-proof: tools/validation/map-poi-visibility-selftest.mjs plants an underworld PUBLIC row, an
// underworld row with no visibility and a patch row, and passes the real core and pack confs.
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

// [{ kind, category, visibility, line }] for each LF_MapPoiKind block; a field a row leaves out is "".
export function poiRows(confText) {
  const rows = [];
  const lines = confText.split("\n");
  let row = null;
  let depth = 0;
  lines.forEach((text, i) => {
    const open = /^\s*LF_MapPoiKind\b.*\{\s*$/.exec(text);
    if (!row && open) {
      row = { kind: "", category: "", visibility: "", line: i + 1 };
      depth = 1;
      return;
    }
    if (!row) return;
    const str = (name) => new RegExp(String.raw`^\s*${name}\s+"([^"]*)"`).exec(text)?.[1];
    const kind = str("m_sKind");
    if (kind !== undefined) row.kind = kind;
    const category = str("m_sCategory");
    if (category !== undefined) row.category = category;
    const vis = /^\s*m_eVisibility\s+(\S+)/.exec(text)?.[1];
    if (vis !== undefined) row.visibility = vis;
    depth += (text.match(/\{/g) || []).length - (text.match(/\}/g) || []).length;
    if (depth <= 0) {
      rows.push(row);
      row = null;
    }
  });
  return rows;
}

// [{ file, line, kind, why }] for every underworld row that is not HIDDEN. `files` is [{ name, text }].
export function underworldViolations(files) {
  const parsed = files.map((f) => ({ name: f.name, rows: poiRows(f.text) }));
  const underworldKinds = new Set();
  for (const f of parsed) for (const r of f.rows) if (r.category === "underworld" && r.kind) underworldKinds.add(r.kind);
  const bad = [];
  for (const f of parsed) {
    for (const r of f.rows) {
      const underworld = r.category === "underworld" || (r.category === "" && underworldKinds.has(r.kind));
      if (!underworld) continue;
      if (r.category === "underworld" && r.visibility === "")
        bad.push({ file: f.name, line: r.line, kind: r.kind, why: "category underworld with no m_eVisibility (the default is PUBLIC): set m_eVisibility HIDDEN" });
      else if (r.visibility !== "" && r.visibility !== "HIDDEN")
        bad.push({ file: f.name, line: r.line, kind: r.kind, why: `underworld kind with m_eVisibility ${r.visibility}: it must be HIDDEN` });
    }
  }
  return bad;
}

// Every POI conf under addons/*/Configs/Map/.
export function poiConfFiles(root) {
  const files = [];
  const addons = join(root, "addons");
  for (const a of readdirSync(addons, { withFileTypes: true })) {
    const dir = join(addons, a.name, "Configs", "Map");
    if (!a.isDirectory() || !existsSync(dir)) continue;
    for (const n of readdirSync(dir)) {
      if (/^LF_MapPois.*\.conf$/.test(n)) files.push({ name: `addons/${a.name}/Configs/Map/${n}`, text: readFileSync(join(dir, n), "utf8") });
    }
  }
  return files;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-map-poi-visibility.mjs")) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const bad = underworldViolations(poiConfFiles(root));
  for (const b of bad) console.log(`FAIL ${b.file}:${b.line}: POI kind ${b.kind}: ${b.why}`);
  if (bad.length) process.exit(1);
  console.log("validate-map-poi-visibility: OK");
}
