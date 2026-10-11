// tools/actions/place-action-contexts.mjs
//
// Place UserActionContext Offset, Radius, and Height from the mesh for a world
// interactable that is still on the door default. Apple and ore are the
// pattern: the point sits on the part the player uses (the fruit, the rock),
// not on an empty pivot. The door failure is Offset 0 0 0, Radius 0.650,
// Height 0, which is the hinge. A mesh taller than about 1.5 m gets a Height
// so the volume is a capsule along the mesh. Handheld pickups are left as they
// are.
//
// The door override reuses ActionsManagerComponent {F32B667C3C823800}. A second
// ActionsManagerComponent drops the lock. Meshes are not renamed or swapped.
//
//   node tools/actions/place-action-contexts.mjs

import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { PakVirtualFS } from "../mcp/enfusion-mcp-bk/dist/pak/vfs.js";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const DOOR_RADIUS = 0.65;
const TALL_METERS = 1.5;
const FALLBACK_CONTEXT = "50888D37F529F720";
const FALLBACK_POINT = "50888D37F0EB77E0";
const FALLBACK_MANAGER = "F32B667C3C823800";

let vfs;

function gamePath() {
  return process.env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
}

function pak() {
  if (!vfs) vfs = PakVirtualFS.get(gamePath());
  return vfs;
}

function addonOf(root) {
  return join(root, "addons", "LifeFramework");
}

function bracesBalanced(text) {
  let depth = 0;
  for (const ch of text) {
    if (ch === "{") depth++;
    else if (ch === "}") {
      depth--;
      if (depth < 0) return false;
    }
  }
  return depth === 0;
}

function newlineOf(text) {
  return text.includes("\r\n") ? "\r\n" : "\n";
}

function num(value) {
  let text = (Math.round(value * 10000) / 10000).toFixed(4);
  text = text.replace(/0+$/, "").replace(/\.$/, "");
  if (text === "-0") return "0";
  return text;
}

function walk(dir, out = []) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) walk(path, out);
    else if (entry.name.endsWith(".et") || entry.name.endsWith(".layer")) out.push(path);
  }
  return out;
}

const textCache = new Map();

function readResource(root, rel) {
  const key = rel.replace(/\\/g, "/");
  if (textCache.has(key)) return textCache.get(key);
  const local = join(addonOf(root), ...key.split("/"));
  let text = null;
  if (existsSync(local) && statSync(local).isFile()) text = readFileSync(local, "utf8");
  else if (pak()?.exists(key)) text = pak().readTextFile(key);
  textCache.set(key, text);
  return text;
}

function headerLine(text) {
  const end = text.indexOf("\n");
  return end < 0 ? text : text.slice(0, end).replace(/\r$/, "");
}

function parentOf(text) {
  const match = headerLine(text).match(/:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/);
  return match ? match[1] : null;
}

function braceBlock(text, openIndex) {
  let depth = 0;
  for (let i = openIndex; i < text.length; i++) {
    const ch = text[i];
    if (ch === "{") depth++;
    else if (ch === "}") {
      depth--;
      if (depth === 0) return { start: openIndex, end: i + 1 };
    }
  }
  return null;
}

function matchBlock(text, pattern) {
  const match = pattern.exec(text);
  if (!match) return null;
  const open = match[0].includes("{")
    ? match.index + match[0].lastIndexOf("{")
    : text.indexOf("{", match.index);
  if (open < 0) return null;
  const block = braceBlock(text, open);
  if (!block) return null;
  return { ...block, index: match.index, header: match[0], body: text.slice(block.start, block.end) };
}

function lineIndent(text, index) {
  const lineStart = text.lastIndexOf("\n", Math.max(0, index - 1)) + 1;
  const lineEnd = text.indexOf("\n", lineStart);
  const line = text.slice(lineStart, lineEnd < 0 ? text.length : lineEnd);
  return (line.match(/^[ \t]*/) || [""])[0];
}

function directComponents(entityText) {
  return matchBlock(entityText, /(?:^|\n)[ \t]*components\s*\{/);
}

function parseMesh(components) {
  if (!components) return null;
  const match = components.match(/Object\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.xob)"/);
  return match ? match[1] : null;
}

function parseContext(components) {
  if (!components) return null;
  const user = matchBlock(components, /UserActionContext\s*"\{([0-9A-Fa-f]{16})\}"\s*\{/);
  if (!user) return null;
  const point = matchBlock(user.body, /PointInfo\s*"\{([0-9A-Fa-f]{16})\}"\s*\{/);
  let offset = null;
  if (point) {
    const off = point.body.match(/Offset\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)/);
    offset = off ? off.slice(1).map(Number) : [0, 0, 0];
  }
  const radius = user.body.match(/\bRadius\s+(-?\d+(?:\.\d+)?)/);
  const height = user.body.match(/\bHeight\s+(-?\d+(?:\.\d+)?)/);
  const manager = components.match(/ActionsManagerComponent\s*"\{([0-9A-Fa-f]{16})\}"/);
  return {
    manager: manager ? manager[1].toUpperCase() : null,
    contextGuid: user.header.match(/\{([0-9A-Fa-f]{16})\}/)[1].toUpperCase(),
    pointGuid: point ? point.header.match(/\{([0-9A-Fa-f]{16})\}/)[1].toUpperCase() : null,
    offset,
    radius: radius ? Number(radius[1]) : null,
    height: height ? Number(height[1]) : null,
  };
}

function resolveChain(root, text, seen = new Set()) {
  const parent = parentOf(text);
  let inherited = { offset: [0, 0, 0], radius: null, height: 0, mesh: null, found: false };
  if (parent && !seen.has(parent)) {
    seen.add(parent);
    const body = readResource(root, parent);
    if (body) inherited = resolveChain(root, body, seen);
    else if (/Door_Base\.et$/.test(parent)) {
      inherited = { offset: [0, 0, 0], radius: DOOR_RADIUS, height: 0, mesh: null, found: true };
    }
  }
  const components = directComponents(text)?.body ?? "";
  const mesh = parseMesh(components) || inherited.mesh;
  const own = parseContext(components);
  if (!own) return { ...inherited, mesh };
  return {
    offset: own.offset ?? inherited.offset,
    radius: own.radius ?? inherited.radius,
    height: own.height ?? inherited.height,
    mesh,
    found: true,
  };
}

function ancestorIds(root, text, seen = new Set()) {
  const parent = parentOf(text);
  if (!parent || seen.has(parent)) {
    return { manager: FALLBACK_MANAGER, context: FALLBACK_CONTEXT, point: FALLBACK_POINT };
  }
  seen.add(parent);
  const body = readResource(root, parent);
  if (!body) return { manager: FALLBACK_MANAGER, context: FALLBACK_CONTEXT, point: FALLBACK_POINT };
  const own = parseContext(directComponents(body)?.body ?? "");
  const manager = (body.match(/ActionsManagerComponent\s*"\{([0-9A-Fa-f]{16})\}"/) || [])[1];
  if (own?.contextGuid) {
    return {
      manager: (own.manager || manager || FALLBACK_MANAGER).toUpperCase(),
      context: own.contextGuid,
      point: own.pointGuid || FALLBACK_POINT,
    };
  }
  const up = ancestorIds(root, body, seen);
  if (manager) up.manager = manager.toUpperCase();
  return up;
}

export function isDoorDefault(volume) {
  if (!volume || volume.radius == null) return false;
  const [x, y, z] = volume.offset ?? [0, 0, 0];
  return Math.abs(x) < 0.0001
    && Math.abs(y) < 0.0001
    && Math.abs(z) < 0.0001
    && Math.abs(volume.radius - DOOR_RADIUS) < 0.001
    && Math.abs(volume.height ?? 0) < 0.0001;
}

function isHandheld(rel, entityText) {
  const path = rel.replace(/\\/g, "/");
  if (/\/(?:Items|Tools|Currencies)\//.test(path)) return true;
  return entityText.includes("SCR_PickUpItemAction");
}

function boundsOf(root, rel) {
  const key = rel.replace(/\\/g, "/");
  const local = join(addonOf(root), ...key.split("/"));
  let buf = null;
  if (existsSync(local)) buf = readFileSync(local);
  else if (pak()?.exists(key)) buf = pak().readFile(key);
  if (!buf || buf.length < 48 || buf.toString("ascii", 0, 4) !== "FORM") return null;
  const min = [buf.readFloatLE(24), buf.readFloatLE(28), buf.readFloatLE(32)];
  const max = [buf.readFloatLE(36), buf.readFloatLE(40), buf.readFloatLE(44)];
  if ([...min, ...max].some((value) => !Number.isFinite(value))) return null;
  if (max[0] < min[0] || max[1] < min[1] || max[2] < min[2]) return null;
  return { min, max };
}

function hingeAxis(min, max) {
  const size = min.map((value, index) => max[index] - value);
  if (size[1] < 1.2) return null;
  const nearOrigin = (value) => value > -0.25 && value < 0.25;
  if (size[2] < 0.4 && size[0] > 0.7 && (nearOrigin(min[0]) || nearOrigin(max[0]))) return 0;
  if (size[0] < 0.4 && size[2] > 0.7 && (nearOrigin(min[2]) || nearOrigin(max[2]))) return 2;
  return null;
}

// Point on the part the player uses. A hinged slab's origin is the hinge, so
// the point moves out along the leaf. Anything else uses the mesh body: the
// center of a small object (an apple), or a reachable point on a larger one
// (ore). A mesh taller than about 1.5 m gets a Height.
export function volumeFromBounds(min, max) {
  const size = min.map((value, index) => max[index] - value);
  const center = min.map((value, index) => (value + max[index]) / 2);
  const offset = center.slice();
  const axis = hingeAxis(min, max);
  if (axis != null) {
    const hingeAtMin = Math.abs(min[axis]) <= Math.abs(max[axis]);
    const hinge = hingeAtMin ? min[axis] : max[axis];
    const far = hingeAtMin ? max[axis] : min[axis];
    offset[axis] = hinge + (far - hinge) * 0.9;
  } else if (size[1] > 0.25 && size[1] <= TALL_METERS) {
    const lo = Math.max(min[1], 0);
    const hi = max[1];
    if (hi > lo) offset[1] = lo + (hi - lo) * 0.7;
  }
  return {
    offset,
    radius: Math.max(size[0], size[2]) / 2,
    height: size[1] > TALL_METERS ? size[1] : 0,
  };
}

function isEntityHeader(line) {
  if (/\.et"\s*\{/.test(line)) return true;
  if (/\{[0-9A-Fa-f]{16}\}/.test(line)) return false;
  return /^[ \t]*(GenericEntity|GameEntity|Tree|SCR_[A-Za-z0-9_]+|EL_[A-Za-z0-9_]+|LF_[A-Za-z0-9_]+)\b.*\{\s*$/.test(line);
}

function entitySpans(text, isLayer) {
  if (!isLayer) return [{ start: 0, end: text.length, text }];
  const spans = [];
  let offset = 0;
  for (const line of text.split(/\r?\n/)) {
    if (isEntityHeader(line)) {
      const lineEnd = text.indexOf("\n", offset);
      const header = text.slice(offset, lineEnd < 0 ? text.length : lineEnd);
      const brace = header.lastIndexOf("{");
      const open = brace < 0 ? -1 : offset + brace;
      const block = open < 0 ? null : braceBlock(text, open);
      if (block) spans.push({ start: offset, end: block.end, text: text.slice(offset, block.end) });
    }
    offset += line.length;
    if (text[offset] === "\r") offset += 1;
    if (text[offset] === "\n") offset += 1;
  }
  return spans;
}

function alreadyPlaced(own, resolved) {
  if (!own?.offset) return false;
  const volume = {
    offset: own.offset,
    radius: own.radius ?? resolved.radius,
    height: own.height ?? resolved.height,
  };
  return !isDoorDefault(volume);
}

function targetOf(root, rel, entityText) {
  if (isHandheld(rel, entityText)) return null;
  const components = directComponents(entityText)?.body ?? "";
  const localMesh = parseMesh(components);
  if (!localMesh) return null;
  const resolved = resolveChain(root, entityText);
  const own = parseContext(components);
  const parentRel = parentOf(entityText);
  const parentBody = parentRel ? readResource(root, parentRel) : null;
  const parentMesh = parentBody ? resolveChain(root, parentBody).mesh : null;
  const meshOverride = Boolean(parentMesh && localMesh !== parentMesh && isDoorResource(parentRel));
  if (alreadyPlaced(own, resolved)) return null;
  if (!isDoorDefault(resolved) && !meshOverride) return null;
  const bounds = boundsOf(root, localMesh);
  if (!bounds) return { error: `no mesh bounds for ${localMesh}` };
  const ids = ancestorIds(root, entityText);
  return {
    mesh: localMesh,
    volume: volumeFromBounds(bounds.min, bounds.max),
    manager: (own?.manager || ids.manager).toUpperCase(),
    context: (own?.contextGuid || ids.context).toUpperCase(),
    point: (own?.pointGuid || ids.point).toUpperCase(),
  };
}

function contextLines(indent, step, spec) {
  const i2 = indent + step;
  const i3 = i2 + step;
  const i4 = i3 + step;
  const i5 = i4 + step;
  const [x, y, z] = spec.volume.offset;
  const lines = [
    `${i2}ActionContexts {`,
    `${i3}UserActionContext "{${spec.context}}" {`,
    `${i4}Position PointInfo "{${spec.point}}" {`,
    `${i5}Offset ${num(x)} ${num(y)} ${num(z)}`,
    `${i4}}`,
    `${i4}Radius ${num(spec.volume.radius)}`,
  ];
  if (spec.volume.height > 0) lines.push(`${i4}Height ${num(spec.volume.height)}`);
  lines.push(`${i3}}`, `${i2}}`);
  return lines;
}

function isDoorResource(rel) {
  if (!rel) return false;
  return /(?:^|\/)(?:EL_Door_Base|Door_Base)\.et$/.test(rel) || /\/Doors\//.test(rel);
}

function applyEntity(entityText, spec, nl) {
  const components = directComponents(entityText);
  if (!components) return entityText;
  const compIndent = lineIndent(entityText, components.start);
  const innerLine = components.body.split(/\r?\n/).slice(1).find((line) => line.trim());
  const innerIndent = innerLine ? (innerLine.match(/^[ \t]*/) || [""])[0] : `${compIndent} `;
  const step = innerIndent.startsWith(compIndent) ? innerIndent.slice(compIndent.length) || " " : " ";
  const manager = matchBlock(components.body, new RegExp(`ActionsManagerComponent\\s*"\\{${spec.manager}\\}"\\s*\\{`, "i"));
  if (manager) {
    if (manager.body.includes("ActionContexts")) return entityText;
    const managerIndent = lineIndent(components.body, manager.start);
    const lines = contextLines(managerIndent, step, spec);
    const piece = nl + lines.join(nl);
    const replaced = components.body.slice(0, manager.start + 1) + piece + components.body.slice(manager.start + 1);
    return entityText.slice(0, components.start) + replaced + entityText.slice(components.end);
  }
  const lines = [
    `${innerIndent}ActionsManagerComponent "{${spec.manager}}" {`,
    ...contextLines(innerIndent, step, spec),
    `${innerIndent}}`,
  ];
  const close = components.body.lastIndexOf("}");
  let cut = components.body.lastIndexOf("\n", close);
  if (cut > 0 && components.body[cut - 1] === "\r") cut -= 1;
  if (cut < 0) cut = close;
  const piece = nl + lines.join(nl);
  const replaced = components.body.slice(0, cut) + piece + components.body.slice(cut);
  return entityText.slice(0, components.start) + replaced + entityText.slice(components.end);
}

function placeFile(root, abs) {
  const rel = relative(addonOf(root), abs).replace(/\\/g, "/");
  let text = readFileSync(abs, "utf8");
  const nl = newlineOf(text);
  const updated = [];
  for (let pass = 0; pass < 20; pass++) {
    const span = entitySpans(text, abs.endsWith(".layer")).find((entity) => {
      const target = targetOf(root, rel, entity.text);
      return target && !target.error;
    });
    if (!span) {
      const broken = entitySpans(text, abs.endsWith(".layer")).map((entity) => targetOf(root, rel, entity.text)).find((target) => target?.error);
      if (broken) return { updated, error: `${rel}: ${broken.error}` };
      break;
    }
    const target = targetOf(root, rel, span.text);
    const next = applyEntity(span.text, target, nl);
    if (next === span.text) return { updated, error: `${rel}: could not write an action context` };
    text = text.slice(0, span.start) + next + text.slice(span.end);
    if (!bracesBalanced(text)) return { updated, error: `${rel}: action context insert unbalanced the prefab` };
    textCache.set(rel, text);
    updated.push({
      rel,
      mesh: target.mesh,
      offset: target.volume.offset.map(num).join(" "),
      radius: num(target.volume.radius),
      height: num(target.volume.height),
    });
  }
  if (updated.length) writeFileSync(abs, text);
  return { updated };
}

export function placeActionContexts(root = ROOT) {
  textCache.clear();
  const updated = [];
  const errors = [];
  for (const file of walk(addonOf(root))) {
    const result = placeFile(root, file);
    updated.push(...result.updated);
    if (result.error) errors.push(result.error);
  }
  return { updated, errors };
}

export function auditActionContexts(root = ROOT) {
  textCache.clear();
  const failures = [];
  let checked = 0;
  for (const file of walk(addonOf(root))) {
    const rel = relative(addonOf(root), file).replace(/\\/g, "/");
    const text = readFileSync(file, "utf8");
    for (const entity of entitySpans(text, file.endsWith(".layer"))) {
      if (isHandheld(rel, entity.text)) continue;
      const resolved = resolveChain(root, entity.text);
      if (!resolved.found) continue;
      checked++;
      if (!isDoorDefault(resolved)) continue;
      const name = entity.text.split(/\r?\n/, 1)[0].trim();
      failures.push(`${rel} (${name}) is still the door default: Offset 0 0 0, Radius 0.650, Height 0`);
    }
  }
  return { failures, checked };
}

function isMain() {
  const self = resolve(fileURLToPath(import.meta.url));
  const invoked = process.argv[1] ? resolve(process.argv[1]) : "";
  return self === invoked;
}

if (isMain()) {
  const { updated, errors } = placeActionContexts(ROOT);
  for (const item of updated) {
    console.log(`placed ${item.rel} offset ${item.offset} radius ${item.radius} height ${item.height}`);
  }
  for (const error of errors) console.log(`ERROR ${error}`);
  console.log(`place-action-contexts: ${updated.length} placed, ${errors.length} error(s)`);
  if (errors.length) process.exit(1);
}
