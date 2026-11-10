// tools/actions/action-anchor.mjs
//
// Put a UserActionContext at a sensible anchor on the entity's mesh bounds, and
// check that every placed context is on its mesh and within a standing
// player's reach. Entry point is place-action-contexts.mjs (it dispatches here
// when it gets arguments). Docs: docs/action-anchors.md.
//
//   --anchor center|front|top|custom [--height 1.0] [--front +z|-z]
//            [--offset x y z] [--radius r] [--context name] [--dry] <target>
//   --batch [--dry]        apply every entry of tools/actions/anchors.json
//   --check [--tolerance 0.5] [--fail-gap 3]
//
// <target> is a prefab (Prefabs/.../X.et), a layer entity (Name, or
// File.layer#Name), or a child entity of a prefab (X.et#Name). Bounds come from
// the .xob header (the same source place-action-contexts.mjs uses), in the
// entity's local space, which is the space a PointInfo Offset is in.

import { existsSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  ROOT, addonOf, ancestorIds, applyEntity, boundsOf, bracesBalanced, directComponents,
  entitySpans, isDoorDefault, isHandheld, lineIndent, matchBlock, newlineOf, num,
  parseContext, readResource, resolveChain, textCache, walk,
} from "./place-action-contexts.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const CONFIG = join(HERE, "anchors.json");
const STAND = 0.5; // a player stops about this far off the mesh
const CHEST = 1.0; // and the context reaches the point from about this height

// ---- geometry ---------------------------------------------------------------

export function anchorPoint(min, max, kind, opts = {}) {
  const cx = (min[0] + max[0]) / 2;
  const cz = (min[2] + max[2]) / 2;
  if (kind === "center") return [cx, (min[1] + max[1]) / 2, cz];
  if (kind === "top") return [cx, max[1], cz];
  if (kind === "front") {
    const sign = opts.front === "-z" ? -1 : 1;
    const wanted = opts.height == null ? 1.0 : opts.height;
    const y = Math.min(Math.max(wanted, min[1]), max[1]); // never floats above the mesh
    return [cx, y, sign > 0 ? max[2] : min[2]];
  }
  throw new Error(`unknown anchor '${kind}' (center, front, top, custom)`);
}

// A player stands 1.0 to 1.5 m from the point. A front anchor is on the face;
// center/top sit behind it by half the footprint.
export function suggestRadius(min, max, kind) {
  if (kind === "front") return 1.5;
  const half = Math.max(max[0] - min[0], max[2] - min[2]) / 2;
  return Math.min(4, Math.max(1.5, half + 1.25));
}

export function gapToBounds(point, min, max) {
  const d = point.map((v, i) => Math.max(min[i] - v, 0, v - max[i]));
  return Math.hypot(...d);
}

// Distance from the point to the nearest spot a player can stand (outside the
// footprint grown by STAND, at chest height).
export function reachDistance(point, min, max, height = 0) {
  const lo = [min[0] - STAND, min[2] - STAND];
  const hi = [max[0] + STAND, max[2] + STAND];
  const p = [point[0], point[2]];
  const inside = p[0] > lo[0] && p[0] < hi[0] && p[1] > lo[1] && p[1] < hi[1];
  let dh;
  if (inside) dh = Math.min(p[0] - lo[0], hi[0] - p[0], p[1] - lo[1], hi[1] - p[1]);
  else dh = Math.hypot(Math.max(lo[0] - p[0], 0, p[0] - hi[0]), Math.max(lo[1] - p[1], 0, p[1] - hi[1]));
  const dv = height > 0 ? 0 : Math.abs(point[1] - CHEST);
  return Math.hypot(dh, dv);
}

// ---- entities ---------------------------------------------------------------

function nameOf(spanText) {
  const first = spanText.split(/\r?\n/, 1)[0];
  const m = first.match(/^[ \t]*\w+\s+([A-Za-z0-9_]+)\s*(?::|\{)/);
  return m ? m[1] : null;
}

function spansOf(text) {
  return entitySpans(text, true);
}

function resolveTarget(root, target) {
  let file = null;
  let name = null;
  const hash = target.indexOf("#");
  const base = hash < 0 ? target : target.slice(0, hash);
  if (hash >= 0) name = target.slice(hash + 1);
  const candidates = [resolve(base), join(root, base), join(addonOf(root), base)];
  for (const c of candidates) {
    if (/\.(et|layer)$/.test(c) && existsSync(c) && statSync(c).isFile()) { file = c; break; }
  }
  if (file) {
    const text = readFileSync(file, "utf8");
    const spans = spansOf(text);
    const span = name ? spans.find((s) => nameOf(s.text) === name) : spans[0];
    if (!span) throw new Error(`${target}: no such entity`);
    return { file, span };
  }
  name = base;
  const hits = [];
  for (const f of walk(addonOf(root))) {
    const text = readFileSync(f, "utf8");
    for (const span of spansOf(text)) if (nameOf(span.text) === name) hits.push({ file: f, span });
  }
  if (hits.length !== 1) throw new Error(`${target}: ${hits.length ? "ambiguous, " + hits.length + " entities named that" : "not found"}; use File#Name`);
  return hits[0];
}

function relOf(root, file) {
  return relative(addonOf(root), file).replace(/\\/g, "/");
}

// ---- writing ----------------------------------------------------------------

function setVolume(spanText, offset, radius, contextName, nl) {
  const comps = directComponents(spanText);
  if (!comps) return null;
  let ctx = null;
  const re = /UserActionContext\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/g;
  let m;
  while ((m = re.exec(comps.body))) {
    const b = matchBlock(comps.body.slice(m.index), /^UserActionContext\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/);
    if (!b) continue;
    const abs = { start: m.index, end: m.index + b.end, body: comps.body.slice(m.index, m.index + b.end) };
    if (!contextName || new RegExp(`ContextName\\s+"${contextName}"`).test(abs.body)) { ctx = abs; break; }
  }
  if (!ctx) return null;
  const ind = lineIndent(comps.body, ctx.start) + " ";
  let body = ctx.body;
  const off = `Offset ${offset.map(num).join(" ")}`;
  const point = matchBlock(body, /PointInfo\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/);
  if (point) {
    if (/\bOffset\s/.test(point.body)) {
      body = body.slice(0, point.start) + point.body.replace(/Offset\s+[^\r\n]+/, off) + body.slice(point.end);
    } else {
      const pind = lineIndent(body, point.index) + " ";
      body = body.slice(0, point.start + 1) + nl + pind + off + body.slice(point.start + 1);
    }
  } else {
    const ptr = "50888D37F0EB77E0";
    const first = body.indexOf("{") + 1;
    body = body.slice(0, first) + nl + ind + `Position PointInfo "{${ptr}}" {` + nl + ind + ` ${off}` + nl + ind + "}" + body.slice(first);
  }
  if (/\bRadius\s/.test(body)) body = body.replace(/\bRadius\s+[^\r\n]+/, `Radius ${num(radius)}`);
  else body = body.slice(0, body.lastIndexOf("}")).replace(/[ \t]*$/, "") + ind + `Radius ${num(radius)}` + nl + lineIndent(comps.body, ctx.start) + "}";
  // Height would make a capsule that a mesh anchor does not want.
  body = body.replace(/\r?\n[ \t]*Height\s+[^\r\n]+/, "");
  const nextComps = comps.body.slice(0, ctx.start) + body + comps.body.slice(ctx.end);
  return spanText.slice(0, comps.start) + nextComps + spanText.slice(comps.end);
}

// ---- anchor mode ------------------------------------------------------------

function fmt(v) {
  return v.map(num).join(" ");
}

export function anchorEntity(root, target, opts) {
  textCache.clear();
  const { file, span } = resolveTarget(root, target);
  const rel = relOf(root, file);
  const resolved = resolveChain(root, span.text);
  const before = resolved.found ? { offset: resolved.offset, radius: resolved.radius } : null;
  let offset;
  let radius;
  let kind = opts.anchor;
  if (kind === "custom") {
    if (!opts.offset) throw new Error("--anchor custom needs --offset x y z");
    offset = opts.offset;
    radius = opts.radius ?? 1.5;
  } else {
    if (!resolved.mesh) throw new Error(`${target}: no mesh to take bounds from; use --anchor custom --offset x y z`);
    const bounds = boundsOf(root, resolved.mesh);
    if (!bounds) throw new Error(`${target}: no bounds in ${resolved.mesh}`);
    offset = opts.offset ?? anchorPoint(bounds.min, bounds.max, kind, opts);
    radius = opts.radius ?? suggestRadius(bounds.min, bounds.max, kind);
  }
  radius = Math.round(radius * 100) / 100;
  const nl = newlineOf(span.text);
  let next = setVolume(span.text, offset, radius, opts.context, nl);
  if (!next) {
    const own = parseContext(directComponents(span.text)?.body ?? "");
    const ids = ancestorIds(root, span.text);
    next = applyEntity(span.text, {
      volume: { offset, radius, height: 0 },
      manager: (own?.manager || ids.manager).toUpperCase(),
      context: (own?.contextGuid || ids.context).toUpperCase(),
      point: (own?.pointGuid || ids.point).toUpperCase(),
    }, nl);
  }
  if (next === span.text) {
    console.log(`unchanged ${rel}#${nameOf(span.text) ?? "(root)"} [${kind}] offset ${fmt(offset)} r${num(radius)}`);
    return { rel, offset, radius };
  }
  const whole = readFileSync(file, "utf8");
  const text = whole.slice(0, span.start) + next + whole.slice(span.end);
  if (!bracesBalanced(text)) throw new Error(`${target}: edit unbalanced the file`);
  if (!opts.dry) writeFileSync(file, text);
  const was = before ? `${fmt(before.offset)} r${before.radius == null ? "?" : num(before.radius)}` : "none";
  console.log(`${opts.dry ? "would set" : "set"} ${rel}#${nameOf(span.text) ?? "(root)"} [${kind}] offset ${was} -> ${fmt(offset)} r${num(radius)}`);
  return { rel, offset, radius };
}

// ---- check mode -------------------------------------------------------------

export function checkContexts(root = ROOT, opts = {}) {
  const tolerance = opts.tolerance ?? 0.5;
  const failGap = opts.failGap ?? 3;
  textCache.clear();
  const warnings = [];
  const errors = [];
  let checked = 0;
  let skipped = 0;
  for (const file of walk(addonOf(root))) {
    const rel = relOf(root, file);
    const text = readFileSync(file, "utf8");
    for (const span of spansOf(text)) {
      if (isHandheld(rel, span.text)) continue;
      const own = parseContext(directComponents(span.text)?.body ?? "");
      if (!own) continue; // only contexts written on this entity; inherited ones are checked at their source
      const resolved = resolveChain(root, span.text);
      if (!resolved.found || isDoorDefault(resolved)) continue;
      if (!resolved.mesh) { skipped++; continue; }
      const bounds = boundsOf(root, resolved.mesh);
      if (!bounds) { skipped++; continue; }
      checked++;
      const label = `${rel}#${nameOf(span.text) ?? "(root)"}`;
      const point = resolved.offset;
      const gap = gapToBounds(point, bounds.min, bounds.max);
      const mesh = resolved.mesh.split("/").pop();
      if (gap > failGap) {
        errors.push(`${label}: context point ${fmt(point)} is ${num(gap)} m outside ${mesh} bounds (${fmt(bounds.min)} .. ${fmt(bounds.max)})`);
        continue;
      }
      if (gap > tolerance) {
        warnings.push(`${label}: context point ${fmt(point)} is ${num(gap)} m outside ${mesh} bounds (${fmt(bounds.min)} .. ${fmt(bounds.max)}); try --anchor front`);
        continue;
      }
      if (resolved.radius != null) {
        const reach = reachDistance(point, bounds.min, bounds.max, resolved.height ?? 0);
        if (reach > resolved.radius) {
          warnings.push(`${label}: radius ${num(resolved.radius)} does not reach a player standing ${STAND} m off ${mesh} (needs ${num(reach)} m)`);
        }
      }
    }
  }
  return { warnings, errors, checked, skipped };
}

// ---- cli --------------------------------------------------------------------

function parseArgs(argv) {
  const o = { positional: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const num1 = () => Number(argv[++i]);
    if (a === "--anchor") o.anchor = argv[++i];
    else if (a === "--height") o.height = num1();
    else if (a === "--front") o.front = argv[++i];
    else if (a === "--radius") o.radius = num1();
    else if (a === "--context") o.context = argv[++i];
    else if (a === "--offset") o.offset = [num1(), num1(), num1()];
    else if (a === "--tolerance") o.tolerance = num1();
    else if (a === "--fail-gap") o.failGap = num1();
    else if (a === "--dry") o.dry = true;
    else if (a === "--batch") o.batch = true;
    else if (a === "--check") o.check = true;
    else if (a.startsWith("--")) throw new Error(`unknown option ${a}`);
    else o.positional.push(a);
  }
  return o;
}

export function runAnchorCli(argv) {
  let o;
  try {
    o = parseArgs(argv);
    if (o.check) {
      const r = checkContexts(ROOT, o);
      for (const w of r.warnings) console.log(`WARN ${w}`);
      for (const e of r.errors) console.log(`ERROR ${e}`);
      console.log(`action-anchor check: ${r.errors.length} error(s), ${r.warnings.length} warning(s), ${r.checked} checked, ${r.skipped} without bounds`);
      return r.errors.length ? 1 : 0;
    }
    if (o.batch) {
      const cfg = JSON.parse(readFileSync(CONFIG, "utf8"));
      for (const [target, entry] of Object.entries(cfg)) {
        if (target.startsWith("_")) continue;
        anchorEntity(ROOT, target, { ...entry, dry: o.dry });
      }
      return 0;
    }
    if (!o.anchor) throw new Error("usage: place-action-contexts.mjs --anchor center|front|top|custom [--height h] [--front +z|-z] [--offset x y z] [--radius r] [--dry] <entity|prefab> | --batch | --check");
    if (o.positional.length !== 1) throw new Error("give exactly one <entity|prefab> target");
    anchorEntity(ROOT, o.positional[0], o);
    return 0;
  } catch (e) {
    console.log(`ERROR ${e.message}`);
    return 1;
  }
}
