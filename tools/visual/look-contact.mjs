// tools/visual/look-contact.mjs - `tools\cli look contact <area>`: verify placements by eye (placement spike).
//
//   tools\cli look contact <area>                        the area's layer props (closest first, --close N, default 8)
//   tools\cli look contact <area> --entities A,B          those layer entities by name
//   tools\cli look contact <area> --stock <store_key>     a store's shelf stock, from the audit's rows
//                                                        (--from tmp/place-audit/out.txt, the last `audit placement`)
//   --items A@0,B@3   only those stock copies, in that order
//   --max N   items in the batch (default 12)   --worst   the largest |cgap| first instead of shelf order
//
// Each item gets five pictures in one row of the batch's contact sheet: four low side views with the camera 2 cm
// above the item's measured bottom, and one top-down. The plugin draws the measured bottom face (red), the traced
// support under it (green) and the nine contact rays (yellow) through the mesh, and writes the gap it measured
// (LF_PlaceSnap, the same numbers as the audit's cgap) under the tile. Never framed on the pivot: the target is the
// bottom-face centre of the item's world bounds. One Workbench run (or the look server) shoots the whole batch.

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const VIEWS = ["0", "1", "2", "3", "top"];
const VIEW_LABEL = { 0: "front", 1: "right", 2: "back", 3: "left", top: "top" };
const safe = (s) => String(s).replace(/[^A-Za-z0-9_-]+/g, "_");
const r3 = (v) => Math.round(v * 1000) / 1000;

// The audit's shelf rows of one store: [{ label, x, y, z, cgap }] with x, y, z the bounds centre.
export function stockTargets(outText, store) {
  const rows = [];
  for (const line of String(outText).split(/\r?\n/)) {
    const m = /^shelf (\S+)\/(\S+)@(\d+) (.*)$/.exec(line);
    if (!m || (store && m[1] !== store)) continue;
    const kv = Object.fromEntries(m[4].split(" ").map((p) => p.split("=")).filter((p) => p.length === 2));
    if (!kv.c) continue;
    const [x, y, z] = kv.c.split(",").map(Number);
    rows.push({ label: `${m[2]}@${m[3]}`, entity: `@${m[2]}`, x, y, z, cgap: kv.cgap === undefined || kv.cgap === "none" ? null : Number(kv.cgap), gap: Number(kv.gap) });
  }
  return rows;
}

// The batch: { targets } from the flags. world = agent-box/layers.mjs entities; closeSet from look.mjs.
export function contactTargets({ area, world, flags, root, closeSet }) {
  const max = Number(flags.max) > 0 ? Number(flags.max) : 12;
  let targets;
  if (flags.stock) {
    const from = flags.from || join(root, "tmp", "place-audit", "out.txt");
    if (!existsSync(from)) throw new Error(`--stock reads the audit's rows: ${from} is missing; run tools\\cli audit placement first`);
    targets = stockTargets(readFileSync(from, "utf8"), flags.stock === true ? null : String(flags.stock));
    if (flags.items) {
      const want = String(flags.items).split(",");
      targets = want.map((w) => targets.find((t) => t.label === w)).filter(Boolean);
    }
    if (!targets.length) throw new Error(`no shelf rows for store ${flags.stock} in ${from}`);
  } else if (flags.entities) {
    const names = String(flags.entities).split(",").filter(Boolean);
    targets = names.map((n) => {
      const e = world.find((w) => w.name === n);
      if (!e) throw new Error(`no layer entity named ${n}`);
      return { label: n, entity: n, x: e.x, y: e.y, z: e.z };
    });
  } else {
    targets = closeSet(world, area, Number(flags.close) > 0 ? Number(flags.close) : 8).map((e) => ({ label: e.name || String(e.prefab || "").split("/").pop().replace(/\.et$/, ""), entity: e.name || "-", x: e.x, y: e.y, z: e.z }));
  }
  if (flags.worst) targets.sort((a, b) => Math.abs(b.cgap ?? b.gap ?? 0) - Math.abs(a.cgap ?? a.gap ?? 0));
  return targets.slice(0, max);
}

// The plugin lines of one batch: five shots per target, one sheet row each.
export function contactShots(area, targets, { az = area.front ?? 180 } = {}) {
  const slug = safe(area.name);
  const out = [];
  targets.forEach((t, i) => {
    for (const v of VIEWS) {
      const kind = `k${String(i + 1).padStart(2, "0")}_${safe(t.label).slice(0, 32)}_${v}`;
      const name = `${slug}__${kind}`;
      out.push({ name, area: area.name, kind, label: `${t.label} ${VIEW_LABEL[v]}`, line: `contact ${name} ${t.entity} ${r3(t.x)} ${r3(t.y)} ${r3(t.z)} ${v} ${az}`, entity: { name: /^[-@]/.test(t.entity) ? null : t.entity, x: t.x, y: t.y, z: t.z }, contact: true });
    }
  });
  return out;
}

// Tile text: the measured gap in centimetres, + hovers and - sinks, and a mark past the 1 cm verify bar.
export function contactLabel(shot, st) {
  if (!st?.contact) return shot.label;
  const g = st.contact.gap;
  if (g === null || !Number.isFinite(g) || Math.abs(g) > 100) return `${shot.label}  NO SUPPORT`;
  const cm = Math.round(g * 1000) / 10;
  return `${shot.label}  gap ${cm > 0 ? "+" : ""}${cm} cm${Math.abs(g) > 0.01 ? "  [OFF]" : ""}`;
}

export function selftest() {
  const fails = [];
  const rows = stockTargets("shelf general_store/AppleJuice@3 bmin=1,2,3 gap=0.2 c=177.6,1.1,237.3 cgap=0.0005 flags=none\nshelf other/X@0 c=1,1,1 cgap=none\nfoo", "general_store");
  if (rows.length !== 1 || rows[0].x !== 177.6 || rows[0].cgap !== 0.0005) fails.push(`stockTargets: ${JSON.stringify(rows)}`);
  const shots = contactShots({ name: "general-store", front: 90 }, rows);
  if (shots.length !== 5 || shots[4].line !== "contact general-store__k01_AppleJuice_3_top @AppleJuice 177.6 1.1 237.3 top 90") fails.push(`contactShots: ${shots[4]?.line}`);
  if (contactLabel({ label: "x" }, { contact: { gap: 0.0123 } }) !== "x  gap +1.2 cm  [OFF]") fails.push(`contactLabel: ${contactLabel({ label: "x" }, { contact: { gap: 0.0123 } })}`);
  if (contactLabel({ label: "x" }, { contact: { gap: -0.004 } }) !== "x  gap -0.4 cm") fails.push("contactLabel sink");
  return fails;
}
