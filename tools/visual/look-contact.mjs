// tools/visual/look-contact.mjs - `tools\cli look contact <area|place:...>`: verify placements by eye (#2955, placement spike).
//
//   tools\cli look contact <area>                        the area's layer props (closest first, --close N, default 8)
//   tools\cli look contact <area> --entities A,B          those layer entities by name
//   tools\cli look contact <area> --stock <store_key>     a store's shelf stock, named by slot: <prefab file>@<slot>
//   tools\cli look contact place:<place>/<slot> --stock <store_key>   the same on the island (MainWorld)
//   --items A@0,B@3   only those slots, in that order (needs no audit file)
//   --from <file>     the slot list of --stock: an `audit placement` out.txt (default tmp/place-audit/out.txt)
//   --max N   items in the batch (default 12)   --worst   the largest |cgap| first instead of shelf order
//
// Each item gets five pictures in one row of the batch's contact sheet: four low side views with the camera 2 cm
// above the item's measured bottom, and one top-down. The plugin draws the measured bottom face (red), the traced
// support under it (green) and the nine contact rays (yellow) through the mesh, and writes the gap it measured
// (LF_PlaceSnap, the same numbers as the audit's cgap) under the tile. Never framed on the pivot.
//
// Stock is found in the session that takes the pictures: the plugin asks the store component for the copy at
// the slot (LF_StoreComponent.GetShelfItem). The audit's rows only say WHICH slots exist (and their cgap for
// --worst); their positions are never used, because Workbench play spawns its own stock and a position from the
// test server missed the first item of every sheet (spike 2026-11-18).

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const VIEWS = ["0", "1", "2", "3", "top"];
const VIEW_LABEL = { 0: "front", 1: "right", 2: "back", 3: "left", top: "top" };
const safe = (s) => String(s).replace(/[^A-Za-z0-9_-]+/g, "_");
const r3 = (v) => Math.round(v * 1000) / 1000;

// `stock:<store>/<prefab file>@<slot>`, the plugin's name for a store slot.
export const stockRef = (store, label) => `stock:${store}/${label}`;

// The audit's shelf rows of one store (or every store): [{ label: "<file>@<slot>", store, cgap }]. Slot names only.
export function stockSlots(outText, store) {
  const rows = [];
  for (const line of String(outText).split(/\r?\n/)) {
    const m = /^shelf (\S+)\/(\S+)@(\d+) (.*)$/.exec(line);
    if (!m || (store && m[1] !== store)) continue;
    const kv = Object.fromEntries(m[4].split(" ").map((p) => p.split("=")).filter((p) => p.length === 2));
    const cgap = kv.cgap === undefined || kv.cgap === "none" ? null : Number(kv.cgap);
    rows.push({ label: `${m[2]}@${m[3]}`, store: m[1], ref: stockRef(m[1], `${m[2]}@${m[3]}`), x: 0, y: 0, z: 0, cgap, gap: Number(kv.gap) });
  }
  return rows;
}

// The batch from the flags. world = agent-box/layers.mjs entities; closeSet from look.mjs; audit = out.txt text when it is already read.
export function contactTargets({ area, world, flags, root, closeSet, audit }) {
  const max = Number(flags.max) > 0 ? Number(flags.max) : 12;
  let targets;
  if (flags.stock) {
    if (flags.stock === true) throw new Error("--stock needs a store key: --stock <store_key> (the name in the audit's `shelf <store>/...` rows)");
    const store = String(flags.stock);
    if (flags.items) {
      // Named by slot: no audit needed, the plugin reads the store.
      targets = String(flags.items).split(",").filter(Boolean).map((w) => {
        if (!/^\S+@\d+$/.test(w)) throw new Error(`--items wants <prefab file>@<slot> (e.g. AppleJuice@3), got "${w}"`);
        return { label: w, store, ref: stockRef(store, w), x: 0, y: 0, z: 0 };
      });
    } else {
      const from = flags.from || join(root, "tmp", "place-audit", "out.txt");
      let text = audit;
      if (text === undefined) {
        if (!existsSync(from)) throw new Error(`--stock lists the store's slots from the audit rows: ${from} is missing; run tools\\cli audit placement first, or name them with --items <prefab file>@<slot>`);
        text = readFileSync(from, "utf8");
      }
      targets = stockSlots(text, store);
      if (!targets.length) throw new Error(`no shelf rows for store ${store} in ${from}`);
    }
  } else if (flags.entities) {
    const names = String(flags.entities).split(",").filter(Boolean);
    targets = names.map((n) => {
      const e = world.find((w) => w.name === n);
      if (!e) throw new Error(`no layer entity named ${n}`);
      return { label: n, ref: n, x: e.x, y: e.y, z: e.z };
    });
  } else {
    targets = closeSet(world, area, Number(flags.close) > 0 ? Number(flags.close) : 8).map((e) => ({ label: e.name || String(e.prefab || e.anonPrefab || "").split("/").pop().replace(/\.et$/, "") || "prop", ref: e.name || "-", x: e.x, y: e.y, z: e.z }));
  }
  if (flags.worst) targets.sort((a, b) => Math.abs(b.cgap ?? b.gap ?? 0) - Math.abs(a.cgap ?? a.gap ?? 0));
  return targets.slice(0, max);
}

// The plugin lines of one batch: five shots per target, one sheet row each. `contact <name> <ref> <x> <y> <z> <view> <az>`.
export function contactShots(area, targets, { az = area.front ?? 180 } = {}) {
  const slug = safe(area.name);
  const out = [];
  targets.forEach((t, i) => {
    for (const v of VIEWS) {
      const kind = `k${String(i + 1).padStart(2, "0")}_${safe(t.label).slice(0, 32)}_${v}`;
      const name = `${slug}__${kind}`;
      const named = /^stock:/.test(t.ref) || !/^[-@]/.test(t.ref);
      out.push({ name, area: area.name, kind, label: `${t.label} ${VIEW_LABEL[v]}`, line: `contact ${name} ${t.ref} ${r3(t.x)} ${r3(t.y)} ${r3(t.z)} ${v} ${az}`, entity: { name: named && !/^stock:/.test(t.ref) ? t.ref : null, x: t.x, y: t.y, z: t.z }, contact: true });
    }
  });
  return out;
}

// One area's entry for look.mjs: the area (a wide sheet, five columns, same world as the target) and its shots.
export function contactPlan({ area, ...rest }) {
  const targets = contactTargets({ area, ...rest });
  const stock = rest.flags.stock && rest.flags.stock !== true ? `-${safe(rest.flags.stock)}` : "";
  const named = { ...area, name: `contact-${area.name}${stock}`, sheetCols: VIEWS.length };
  return { area: named, shots: contactShots(named, targets), targets };
}

// Tile text: the measured gap in centimetres, + hovers and - sinks, and a mark past the 1 cm verify bar.
export function contactLabel(shot, st) {
  if (!st?.contact) return shot.label;
  const g = st.contact.gap;
  if (g === null || !Number.isFinite(g) || Math.abs(g) > 100) return `${shot.label}  NO SUPPORT`;
  const cm = Math.round(g * 1000) / 10;
  return `${shot.label}  gap ${cm > 0 ? "+" : ""}${cm} cm${Math.abs(g) > 0.01 ? "  [OFF]" : ""}`;
}

// Offline proof against tools/visual/fixtures/contact-plan.json. Returns the failures (look selftest and the node selftest add them up).
export function selftest({ planText, worldOfAreas } = {}) {
  const fails = [];
  const fx = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), "fixtures", "contact-plan.json"), "utf8"));
  const root = "/none";
  const run = (flags, extra = {}) => contactPlan({ area: fx.area, flags, root, audit: fx.audit, closeSet: () => [], world: [], ...extra });

  // Stock is named by the store's slot, only the store asked for, with no position taken from the audit.
  const s = run(fx.stock.flags);
  if (JSON.stringify(s.targets.map((t) => t.label)) !== JSON.stringify(fx.stock.labels)) fails.push(`stock labels: ${s.targets.map((t) => t.label)}`);
  if (JSON.stringify(s.targets.map((t) => t.ref)) !== JSON.stringify(fx.stock.refs)) fails.push(`stock refs: ${s.targets.map((t) => t.ref)}`);
  if (s.targets.some((t) => t.x !== 0 || t.y !== 0 || t.z !== 0)) fails.push("a stock target kept a position from the audit rows");
  if (s.shots.length !== 5 * fx.stock.labels.length) fails.push(`stock shots: ${s.shots.length}`);
  if (JSON.stringify(s.shots.slice(0, 5).map((x) => x.line)) !== JSON.stringify(fx.stock.firstLines)) fails.push(`stock lines: ${s.shots.slice(0, 5).map((x) => x.line).join(" | ")}`);
  if (s.area.sheetCols !== 5 || s.area.world !== "island") fails.push(`plan area: cols=${s.area.sheetCols} world=${s.area.world}`);

  const w = run(fx.worst.flags);
  if (JSON.stringify(w.targets.map((t) => t.label)) !== JSON.stringify(fx.worst.labels)) fails.push(`--worst order: ${w.targets.map((t) => t.label)}`);
  const it = run(fx.items.flags, { audit: undefined, root: "/definitely/not/here" });
  if (JSON.stringify(it.targets.map((t) => t.label)) !== JSON.stringify(fx.items.labels)) fails.push(`--items without an audit file: ${it.targets.map((t) => t.label)}`);
  let noAudit = "";
  try {
    run({ stock: "general_store" }, { audit: undefined, root: "/definitely/not/here" });
  } catch (e) {
    noAudit = e.message;
  }
  if (!/audit placement/.test(noAudit)) fails.push(`--stock with no audit rows must say how to get them: "${noAudit}"`);
  let bare = "";
  try {
    run({ stock: true });
  } catch (e) {
    bare = e.message;
  }
  if (!/store key/.test(bare)) fails.push(`a bare --stock must name the missing key: "${bare}"`);

  const e = run(fx.entities.flags, { world: fx.entities.world });
  if (e.shots[0]?.line !== fx.entities.line) fails.push(`entity line: ${e.shots[0]?.line}`);

  // The batch runs on the island: the plan opens MainWorld (look.mjs planText / worldOfAreas, passed in by the look selftest).
  if (planText && worldOfAreas) {
    const text = planText(s.shots, [s.area]);
    if (!text.startsWith(fx.planHead)) fails.push(`plan head: ${text.slice(0, 80)}`);
    if (!text.includes(fx.stock.firstLines[0])) fails.push("plan text lost the stock line");
  }

  if (contactLabel({ label: "x" }, { contact: { gap: 0.0123 } }) !== "x  gap +1.2 cm  [OFF]") fails.push(`contactLabel: ${contactLabel({ label: "x" }, { contact: { gap: 0.0123 } })}`);
  if (contactLabel({ label: "x" }, { contact: { gap: -0.004 } }) !== "x  gap -0.4 cm") fails.push("contactLabel sink");
  return fails;
}
