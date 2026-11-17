// The map area rules in node: the twin of LF_MapAreas (Feature/Map/LF_MapAreas.c).
// The emulator (tools/map) calls these to draw areas; both sides run the one case table,
// addons/LifeFramework/Configs/Map/Rules/areas.cases.json, so a drift fails a test:
//   - Enforce: LF_Test_MapAreas
//   - node:    tools/map/rules/index.mjs runCases('areas', evaluate), run by validation/map-areas-selftest.mjs
// Coordinates are world metres, flat: a point is [x, z]. Keep every rule below in step with LF_MapAreas.

import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const CASES_JSON = resolve(ROOT, 'addons/LifeFramework/Configs/Map/Rules/areas.cases.json');
const GENERATED_C = resolve(ROOT, 'addons/LifeFramework/Scripts/Game/Feature/Map/LF_MapAreaCases.generated.c');

export const DEFAULT_SEGMENTS = 32;
export const MIN_SEGMENTS = 3;
// A bend sharper than this (1 + cos of the turn) falls back to the next segment's normal, no spike.
const MITER_MIN_DENOM = 0.2;

// Style per kind. Tokens are LF_UITheme map-* names (card M6 adds the rows); a pack restyles them.
export const STYLES = {
  SAFE: { fillToken: 'map-area-safe-fill', fillAlpha: 15, hatch: 'none', lineToken: 'map-area-safe-line', lineWidth: 2, dash: 'solid', labelKey: '#LF-Map_AreaSafe' },
  LAWLESS: { fillToken: 'map-area-lawless-fill', fillAlpha: 25, hatch: 'diagonal', lineToken: 'map-area-lawless-line', lineWidth: 4, dash: 'heavy-dash', labelKey: '#LF-Map_AreaLawless' },
  TOWN: { fillToken: 'map-area-town-fill', fillAlpha: 0, hatch: 'none', lineToken: 'map-area-town-line', lineWidth: 1, dash: 'dash', labelKey: '' },
  GATED: { fillToken: 'map-area-gated-fill', fillAlpha: 20, hatch: 'cross', lineToken: 'map-area-gated-line', lineWidth: 2, dash: 'solid', labelKey: '#LF-Map_AreaGated' },
  EVENT: { fillToken: 'map-area-event-fill', fillAlpha: 20, hatch: 'none', lineToken: 'map-area-event-line', lineWidth: 2, dash: 'dash', labelKey: '' },
  CUSTOM: { fillToken: 'map-area-custom-fill', fillAlpha: 0, hatch: 'none', lineToken: 'map-area-custom-line', lineWidth: 1, dash: 'solid', labelKey: '' },
};

// Which style a zone rule draws with. NORMAL zones draw nothing (the countryside and the policed band need no fill).
const RULE_KIND = { SAFE: 'SAFE', LAWLESS: 'LAWLESS' };

export function styleOf(kind) {
  const style = STYLES[kind];
  return style ? { kind, ...style } : null;
}

export function styleForRule(rule) {
  const kind = RULE_KIND[rule];
  return kind ? styleOf(kind) : null;
}

const rad = (deg) => (deg * Math.PI) / 180;

// A circle as a polygon: `segments` points, counter-clockwise from +x.
export function circleOutline(center, radius, segments = DEFAULT_SEGMENTS) {
  if (!(radius > 0)) return null;
  const n = Math.max(segments, MIN_SEGMENTS);
  const out = [];
  for (let i = 0; i < n; i++) {
    const a = (2 * Math.PI * i) / n;
    out.push([center[0] + radius * Math.cos(a), center[1] + radius * Math.sin(a)]);
  }
  return out;
}

// A box turned by yaw degrees, the way LF_ZoneArea.Contains turns it: world = rotate(local, yaw).
// Corners run (-hx,-hz), (hx,-hz), (hx,hz), (-hx,hz).
export function boxOutline(center, half, yawDeg) {
  if (!(half[0] > 0) || !(half[1] > 0)) return null;
  const c = Math.cos(rad(yawDeg));
  const s = Math.sin(rad(yawDeg));
  const corners = [[-half[0], -half[1]], [half[0], -half[1]], [half[0], half[1]], [-half[0], half[1]]];
  return corners.map(([lx, lz]) => [center[0] + lx * c - lz * s, center[1] + lx * s + lz * c]);
}

// A corridor (LF_ZoneShape.POLYLINE): the left side forward, the right side back, mitred bends,
// flat caps. One point is a circle.
export function corridorOutline(points, halfWidth, segments = DEFAULT_SEGMENTS) {
  if (!(halfWidth > 0) || points.length === 0) return null;
  if (points.length === 1) return circleOutline(points[0], halfWidth, segments);

  const normals = [];
  for (let i = 0; i < points.length - 1; i++) {
    const dx = points[i + 1][0] - points[i][0];
    const dz = points[i + 1][1] - points[i][1];
    const len = Math.hypot(dx, dz);
    // A zero-length segment keeps the last good normal.
    if (len > 0) normals.push([-dz / len, dx / len]);
    else normals.push(normals.length > 0 ? normals[normals.length - 1] : [0, 1]);
  }

  const offsets = [];
  for (let i = 0; i < points.length; i++) {
    if (i === 0) offsets.push([normals[0][0] * halfWidth, normals[0][1] * halfWidth]);
    else if (i === points.length - 1) offsets.push([normals[i - 1][0] * halfWidth, normals[i - 1][1] * halfWidth]);
    else {
      const a = normals[i - 1];
      const b = normals[i];
      const denom = 1 + a[0] * b[0] + a[1] * b[1];
      if (denom < MITER_MIN_DENOM) offsets.push([b[0] * halfWidth, b[1] * halfWidth]);
      else offsets.push([((a[0] + b[0]) / denom) * halfWidth, ((a[1] + b[1]) / denom) * halfWidth]);
    }
  }

  const left = points.map((p, i) => [p[0] + offsets[i][0], p[1] + offsets[i][1]]);
  const right = points.map((p, i) => [p[0] - offsets[i][0], p[1] - offsets[i][1]]).reverse();
  return left.concat(right);
}

// Area centroid of a polygon (shoelace); a zero-area polygon falls back to the mean of its points.
export function centroid(points) {
  if (!points || points.length === 0) return [0, 0];
  // Worked relative to the first point: the Enforce twin uses 32-bit floats, and products of
  // coordinates in the thousands lose their difference.
  const ox = points[0][0];
  const oz = points[0][1];
  let area2 = 0;
  let cx = 0;
  let cz = 0;
  for (let i = 0; i < points.length; i++) {
    const a = [points[i][0] - ox, points[i][1] - oz];
    const next = points[(i + 1) % points.length];
    const b = [next[0] - ox, next[1] - oz];
    const cross = a[0] * b[1] - b[0] * a[1];
    area2 += cross;
    cx += (a[0] + b[0]) * cross;
    cz += (a[1] + b[1]) * cross;
  }
  if (Math.abs(area2) < 1e-3) {
    const sum = points.reduce((acc, p) => [acc[0] + p[0], acc[1] + p[1]], [0, 0]);
    return [sum[0] / points.length, sum[1] / points.length];
  }
  return [ox + cx / (3 * area2), oz + cz / (3 * area2)];
}

// The outline of one table case or one emulator area. Null when refused (too few points, no radius, no extent).
//   polygon:  { points }                   world points, in order (the component already turned them)
//   circle:   { center, radius, segments? }
//   box:      { center, half, yaw }
//   corridor: { points, radius, segments? }
// Numbers arrive as arrays here; the case table spells lists as strings (parseList).
export function outlineOf(shape) {
  switch (shape.shape) {
    case 'polygon':
      return shape.points.length >= 3 ? shape.points.map((p) => [p[0], p[1]]) : null;
    case 'circle':
      return circleOutline(shape.center, shape.radius, shape.segments ?? DEFAULT_SEGMENTS);
    case 'box':
      return boxOutline(shape.center, shape.half, shape.yaw ?? 0);
    case 'corridor':
      return corridorOutline(shape.points, shape.radius, shape.segments ?? DEFAULT_SEGMENTS);
    default:
      return null;
  }
}

// "x z x z ..." to [[x, z], ...]
export function parseList(text) {
  const nums = String(text ?? '').trim().split(/\s+/).filter(Boolean).map(Number);
  const out = [];
  for (let i = 0; i + 1 < nums.length; i += 2) out.push([nums[i], nums[i + 1]]);
  return out;
}

export function shapeOfInput(c) {
  return {
    shape: c.shape,
    points: parseList(c.points),
    center: parseList(c.center)[0],
    half: parseList(c.half)[0],
    radius: c.radius,
    yaw: c.yaw,
    segments: c.segments,
  };
}

// Numbers leave the rule rounded to 0.01 m and spelled as text, so the shared case table compares
// whole values and the Enforce twin (32-bit floats) matches within 0.011.
export const fmt = (v) => String(Math.round(v * 100) / 100 + 0);
export const listText = (pts) => (pts ? pts.map((p) => `${fmt(p[0])} ${fmt(p[1])}`).join(' ') : '');

// The rule the emulator and the case table call. input.op picks the question:
//   { op: "outline", shape, points|center|half|radius|yaw|segments }  ->  { outline, centroid } ("" when refused)
//   { op: "style", kind }                                              ->  the look, or null for an unknown kind
//   { op: "rule", rule }                                               ->  { kind } the style a zone rule draws with ("" for none)
export function evaluate(input) {
  if (input.op === 'outline') {
    const pts = outlineOf(shapeOfInput(input));
    return { outline: listText(pts), centroid: pts ? listText([centroid(pts)]) : '' };
  }
  if (input.op === 'style') return styleOf(input.kind);
  if (input.op === 'rule') {
    const s = styleForRule(input.rule);
    return { kind: s ? s.kind : '' };
  }
  throw new Error(`unknown op ${input.op}`);
}

// The Enforce copy of the case table: the engine cannot read a loose .json from an addon, so
// LF_Test_MapAreas reads LF_MapAreaCasesData.Json(), generated from the table by `node tools/map/rules/areas.mjs --gen`.
export function generatedSource() {
  const lines = readFileSync(CASES_JSON, 'utf8').replace(/\n$/, '').split('\n');
  const esc = (s) => s.replaceAll("\\", "\\\\").replaceAll('"', '\\"');
  return [
    '// GENERATED by tools/map/rules/areas.mjs --gen from Configs/Map/Rules/areas.cases.json. Do not edit.',
    '//! The area case table as lines, for LF_Test_MapAreas (the engine cannot read the loose .json).',
    'class LF_MapAreaCasesData',
    '{',
    '\t//------------------------------------------------------------------------------------------------',
    '\tstatic string Json()',
    '\t{',
    '\t\tstring text;',
    ...lines.map((l) => `\t\ttext += "${esc(l)}\\n";`),
    '\t\treturn text;',
    '\t}',
    '}',
    '',
  ].join('\n');
}

export function generatedIsCurrent() {
  return existsSync(GENERATED_C) && readFileSync(GENERATED_C, 'utf8') === generatedSource();
}

if (process.argv.includes('--gen') && process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  writeFileSync(GENERATED_C, generatedSource());
  console.log('wrote LF_MapAreaCases.generated.c');
}
