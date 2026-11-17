// tools/map/areas-from-dossier.mjs - the Saint-Philippe map area rows of the pack, derived from the place dossier
// (docs/everon/places/saint-philippe/dossier.json) so no coordinate is typed by hand (AGENTS.md rule 9; the pack is
// the server, so it may hold places). Writes addons/LF_Everon1989/Configs/Map/LF_MapAreas_Everon1989.conf.
//   node tools/map/areas-from-dossier.mjs            write the conf
//   node tools/map/areas-from-dossier.mjs --check    exit 1 when the file on disk is not what the dossier makes
// The rows are a first reading of the dossier; the arrival slot's layer entities (M19, LF_MapAreaComponent and
// LF_ZoneComponent) take over the real lines. Same Id, same name: a zone entity of the same name wins at runtime.
// No lawless zone until M7 (decision D4).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const DOSSIER = "docs/everon/places/saint-philippe/dossier.json";
export const OUT = "addons/LF_Everon1989/Configs/Map/LF_MapAreas_Everon1989.conf";

export const TOWN_PAD = 20; // metres the town line stands off the outermost building corner
export const SIMPLIFY = 6; // metres of wobble the town line may drop
export const SQUARE_RADIUS = 40; // metres, the safe square round the town hall

const r1 = (v) => Math.round(v * 10) / 10;

/** Convex hull (monotone chain) of [x, z] points, counter-clockwise. */
export function hull(points) {
  const p = [...points].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const build = (list) => {
    const h = [];
    for (const q of list) {
      while (h.length >= 2 && cross(h[h.length - 2], h[h.length - 1], q) <= 0) h.pop();
      h.push(q);
    }
    h.pop();
    return h;
  };
  return [...build(p), ...build([...p].reverse())];
}

/** Pushes every vertex `pad` metres away from the mean of the polygon. */
export function grow(poly, pad) {
  const cx = poly.reduce((s, p) => s + p[0], 0) / poly.length;
  const cz = poly.reduce((s, p) => s + p[1], 0) / poly.length;
  return poly.map(([x, z]) => {
    const d = Math.hypot(x - cx, z - cz) || 1;
    return [x + ((x - cx) / d) * pad, z + ((z - cz) / d) * pad];
  });
}

/** Drops vertices that sit within `tol` metres of the line between their neighbours (closed polygon). */
export function simplify(poly, tol) {
  const out = [...poly];
  let again = true;
  while (again && out.length > 4) {
    again = false;
    for (let i = 0; i < out.length; i++) {
      const a = out[(i + out.length - 1) % out.length], b = out[i], c = out[(i + 1) % out.length];
      const len = Math.hypot(c[0] - a[0], c[1] - a[1]) || 1;
      const off = Math.abs((c[0] - a[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (c[1] - a[1])) / len;
      if (off < tol) { out.splice(i, 1); again = true; break; }
    }
  }
  return out;
}

/** The two rows of the pack: the town line and the safe square. Pure over a parsed dossier. */
export function rowsOf(dossier) {
  const corners = [];
  for (const b of dossier.buildings || []) {
    for (const c of b.footprint?.corners || []) corners.push(Array.isArray(c) ? c.map(Number) : String(c).split(" ").map(Number));
    if (!(b.footprint?.corners || []).length) corners.push([b.x, b.z]);
  }
  if (corners.length < 3) throw new Error("the dossier lists no buildings to draw a town line round");
  const line = simplify(grow(hull(corners), TOWN_PAD), SIMPLIFY).map(([x, z]) => [r1(x), r1(z)]);
  // The town hall: a government building that is not the school variant of the municipal office.
  const gov = (dossier.buildings || []).filter((b) => b.type === "government");
  const hall = gov.find((b) => !/school/i.test(b.stem || "")) || gov[0];
  if (!hall) throw new Error("the dossier lists no government building for the safe square");
  return [
    { id: "saint-philippe-town-line", style: "TOWN", shape: "POLYGON", points: line },
    { id: "saint-philippe-square-safe", style: "SAFE", shape: "CIRCLE", centre: [r1(hall.x), r1(hall.z)], radius: SQUARE_RADIUS },
  ];
}

// A stable GUID per row id, so a rerun rewrites the same file (a GUID is immutable once shipped).
const guidOf = (id) => `{${createHash("sha1").update(`LF_MapAreaRow:${id}`).digest("hex").slice(0, 16).toUpperCase()}}`;

export function confOf(rows) {
  const lines = ["LF_MapAreaConfig {", " m_aAreas {"];
  for (const r of rows) {
    lines.push(`  LF_MapAreaRow "${guidOf(r.id)}" {`, `   m_sId "${r.id}"`, `   m_eStyle ${r.style}`, `   m_eShape ${r.shape}`);
    if (r.shape === "CIRCLE") lines.push(`   m_vCentre ${r.centre[0]} 0 ${r.centre[1]}`, `   m_fRadius ${r.radius}`);
    else lines.push(`   m_aPoints { ${r.points.map(([x, z]) => `${x} 0 ${z}`).join(" ")} }`); // unquoted: a quoted vector loads as zeros in game
    lines.push("  }");
  }
  lines.push(" }", "}", "");
  return lines.join("\n");
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1]);
if (isMain) {
  const text = confOf(rowsOf(JSON.parse(readFileSync(join(ROOT, DOSSIER), "utf8"))));
  const file = join(ROOT, OUT);
  if (process.argv.includes("--check")) {
    const same = existsSync(file) && readFileSync(file, "utf8") === text;
    console.log(`areas-from-dossier: ${same ? "ok" : "STALE"} ${OUT}`);
    process.exit(same ? 0 : 1);
  }
  writeFileSync(file, text);
  console.log(`areas-from-dossier: wrote ${OUT}`);
}
