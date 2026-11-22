// tools/actions/counter-npc.mjs
//
// A clerk, teller or desk officer stands behind a counter; the customer stands in front of it. The
// talk context on the NPC is a sphere on its Spine5 bone, so its Radius decides how far across the
// counter the prompt shows. 0.6 m (the old default) put the prompt behind the counter: "you couldn't
// really talk to the clerk across the counter" (live play, 2026-11-22). This check finds every NPC
// placed with a counter in front of it and fails when the talk sphere does not reach the spot where a
// customer stands: the counter's far face plus 0.5 m (the same STAND distance validate-action-reach
// uses for a prop).
//
// Two sources of NPCs:
//  - a layer entity whose prefab chain has a Talk, Quest or RoleTalk context (placed directly);
//  - a resident anchor `LF_ResidentAnchorComponent m_sAnchorId "counter"`, where a resident with a
//    counter work kind (the Residents conf) stands at runtime. Its radius is the smallest talk
//    radius among those residents' prefabs.
//
// Facing: Enfusion yaw 0 faces +Z, positive yaw turns toward +X, so forward = (sin a, cos a).

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import {
  ROOT, boundsOf, braceBlock, directComponents, entitySpans, matchBlock, parentOf, readResource,
  resolveChain, textCache, walk,
} from "./place-action-contexts.mjs";

export const CUSTOMER_STAND = 0.35; // the customer leans to the counter: this far from its far face
export const CHEST_REACH_MIN = 2.0; // a counter NPC's talk sphere is never smaller than this
const COUNTER_NAME = /(counter|till|desk|stall|kiosk|window|teller)/i;
const COUNTER_LOOKAHEAD = 2.6; // a counter origin this far in front of the NPC (m)
const COUNTER_LATERAL = 2.0; // and this far to either side (counters are long)
const DEFAULT_HALF = [0.6, 0.45]; // x, z half extents when the prop has no readable bounds
const TALK_CONTEXTS = new Set(["Talk", "Quest"]);

function num(text, re, fallback) {
  const m = text.match(re);
  return m ? Number(m[1]) : fallback;
}

// The radius of the context a customer uses to talk to this NPC prefab: the largest Talk or Quest
// context in its chain, or RoleTalk when the chain has no authored one. null: not an NPC.
export function talkRadiusOf(root, prefabRel, seen = new Set()) {
  let talk = null;
  let role = null;
  let rel = prefabRel;
  while (rel && !seen.has(rel)) {
    seen.add(rel);
    const text = readResource(root, rel);
    if (!text) break;
    const comps = directComponents(text)?.body ?? "";
    const re = /UserActionContext\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/g;
    let m;
    while ((m = re.exec(comps))) {
      const block = braceBlock(comps, comps.indexOf("{", m.index + m[0].length - 1));
      if (!block) continue;
      const body = comps.slice(block.start, block.end);
      const name = (body.match(/ContextName\s+"([^"]+)"/) || [])[1];
      const radius = num(body, /\bRadius\s+(-?\d+(?:\.\d+)?)/, null);
      if (radius == null) continue;
      if (TALK_CONTEXTS.has(name)) talk = Math.max(talk ?? 0, radius);
      else if (name === "RoleTalk") role = Math.max(role ?? 0, radius);
    }
    rel = parentOf(text);
  }
  return talk ?? role;
}

function headerOf(span) {
  const first = span.text.split(/\r?\n/, 1)[0];
  const m = first.match(/^(\w+)\s*([A-Za-z0-9_]*)\s*(?::\s*"\{[0-9A-Fa-f]{16}\}([^"]+)")?\s*\{/);
  return m ? { cls: m[1], name: m[2] || "", prefab: m[3] || null } : null;
}

function transformOf(spanText) {
  // Only the entity's own lines (depth 1); a child entity's coords are in its parent's space.
  let depth = 0;
  let coords = null;
  let yaw = 0;
  for (const line of spanText.split(/\r?\n/)) {
    const opens = (line.match(/\{/g) || []).length;
    const closes = (line.match(/\}/g) || []).length;
    if (depth === 1) {
      const c = line.match(/^\s*coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/);
      if (c) coords = [Number(c[1]), Number(c[2]), Number(c[3])];
      const y = line.match(/^\s*angleY\s+(-?[\d.]+)/);
      if (y) yaw = Number(y[1]);
      const a = line.match(/^\s*angles\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/);
      if (a) yaw = Number(a[2]);
    }
    depth += opens - closes;
  }
  return coords ? { x: coords[0], z: coords[2], yaw } : null;
}

const rad = (deg) => (deg * Math.PI) / 180;
const fwd = (yaw) => [Math.sin(rad(yaw)), Math.cos(rad(yaw))];

// The prop's footprint corners in world space, from its mesh bounds (entity-local), turned by its yaw.
function footprint(root, prefabRel, t) {
  let half = null;
  let centre = [0, 0];
  if (prefabRel) {
    const text = readResource(root, prefabRel);
    const mesh = text ? resolveChain(root, text).mesh : null;
    const b = mesh ? boundsOf(root, mesh) : null;
    if (b) {
      half = [(b.max[0] - b.min[0]) / 2, (b.max[2] - b.min[2]) / 2];
      centre = [(b.max[0] + b.min[0]) / 2, (b.max[2] + b.min[2]) / 2];
    }
  }
  if (!half) half = DEFAULT_HALF;
  const f = fwd(t.yaw);
  const r = [f[1], -f[0]]; // local +x in world
  const cx = t.x + r[0] * centre[0] + f[0] * centre[1];
  const cz = t.z + r[1] * centre[0] + f[1] * centre[1];
  const out = [];
  for (const sx of [-1, 1]) {
    for (const sz of [-1, 1]) {
      out.push([cx + r[0] * half[0] * sx + f[0] * half[1] * sz, cz + r[1] * half[0] * sx + f[1] * half[1] * sz]);
    }
  }
  return out;
}

// Where a customer stands across `counter` from an NPC at `npc` facing `yaw`, and how far that is
// from the NPC. null when the prop is not in front of the NPC.
export function customerDistance(npc, counterCorners, counterCentre) {
  const f = fwd(npc.yaw);
  const r = [f[1], -f[0]];
  const rel = [counterCentre[0] - npc.x, counterCentre[1] - npc.z];
  const ahead = rel[0] * f[0] + rel[1] * f[1];
  const side = rel[0] * r[0] + rel[1] * r[1];
  if (ahead < 0.2 || ahead > COUNTER_LOOKAHEAD || Math.abs(side) > COUNTER_LATERAL) return null;
  let far = -Infinity;
  for (const [x, z] of counterCorners) far = Math.max(far, (x - npc.x) * f[0] + (z - npc.z) * f[1]);
  const stand = far + CUSTOMER_STAND;
  const lateral = Math.max(0, Math.abs(side) - 1.0); // the counter is long: the customer stands opposite the NPC
  return { stand, distance: Math.hypot(stand, Math.min(lateral, 0)) };
}

function residentCounterRadius(root) {
  // The smallest talk radius among residents whose work kind is a counter, desk or till.
  const confDir = join(root, "addons", "LF_Everon1989", "Configs", "Residents");
  const conf = join(confDir, "LF_Residents_Everon1989.conf");
  if (!existsSync(conf)) return null;
  const text = readFileSync(conf, "utf8");
  let min = null;
  const re = /m_sWorkKind\s+"([^"]+)"[\s\S]*?m_sPrefab\s+"\{[0-9A-Fa-f]{16}\}([^"]+)"/g;
  let m;
  while ((m = re.exec(text))) {
    if (!/(Counter|Till|Desk|Window|Teller)/i.test(m[1])) continue;
    const r = talkRadiusOf(root, m[2]);
    if (r != null) min = min == null ? r : Math.min(min, r);
  }
  return min;
}

export function checkCounterNpcs(root = ROOT) {
  textCache.clear();
  const errors = [];
  const warnings = [];
  let checked = 0;
  const residentRadius = residentCounterRadius(root);
  for (const file of walk(join(root, "addons"))) {
    if (!file.endsWith(".layer")) continue;
    const rel = file.slice(join(root, "addons").length + 1).replace(/\\/g, "/");
    const text = readFileSync(file, "utf8");
    const npcs = [];
    const counters = [];
    for (const span of entitySpans(text, true)) {
      if (/^\s/.test(span.text)) continue; // a nested child, in its parent's space
      const h = headerOf(span);
      const t = h ? transformOf(span.text) : null;
      if (!h || !t) continue;
      const label = `${rel}#${h.name || h.cls}`;
      const isAnchor = /LF_ResidentAnchorComponent/.test(span.text) && /m_sAnchorId\s+"counter"/.test(span.text);
      if (isAnchor) {
        if (residentRadius != null) npcs.push({ label: `${label} (resident counter anchor)`, t, radius: residentRadius });
        continue;
      }
      if (h.prefab) {
        const radius = talkRadiusOf(root, h.prefab);
        if (radius != null) {
          npcs.push({ label, t, radius });
          continue;
        }
      }
      if (h.prefab && (COUNTER_NAME.test(h.name) || COUNTER_NAME.test(h.prefab))) counters.push({ label, t, prefab: h.prefab });
    }
    for (const npc of npcs) {
      let worst = null;
      for (const c of counters) {
        const corners = footprint(root, c.prefab, c.t);
        const r = customerDistance(npc.t, corners, [c.t.x, c.t.z]);
        if (r && (!worst || r.distance > worst.distance)) worst = { ...r, counter: c.label };
      }
      if (!worst) continue;
      checked++;
      if (worst.distance > npc.radius) {
        errors.push(
          `${npc.label}: talk radius ${npc.radius} m does not reach a customer across ${worst.counter} `
          + `(stands ${worst.distance.toFixed(2)} m from the clerk); raise the Talk/Quest context Radius to at least ${Math.max(CHEST_REACH_MIN, Math.ceil(worst.distance * 4) / 4)} (docs/action-anchors.md "Counter NPCs")`,
        );
      }
    }
  }
  return { errors, warnings, checked, residentRadius };
}

// Rule A: the prefabs the residents conf puts at a counter or till, and the dedicated counter-staff
// prefabs, keep a talk sphere a customer can use across a counter, wherever they are placed.
export function checkCounterStaffPrefabs(root = ROOT) {
  textCache.clear();
  const errors = [];
  const conf = join(root, "addons", "LF_Everon1989", "Configs", "Residents", "LF_Residents_Everon1989.conf");
  const staff = new Set([
    "Prefabs/Characters/Npcs/LF_Shopkeeper.et",
    "Prefabs/Characters/Npcs/LF_FuelClerk.et",
    "Prefabs/Characters/Quests/QuestGiver_Clerk.et",
  ]);
  if (existsSync(conf)) {
    const text = readFileSync(conf, "utf8");
    const re = /m_sWorkKind\s+"([^"]+)"[\s\S]*?m_sPrefab\s+"\{[0-9A-Fa-f]{16}\}([^"]+)"/g;
    let m;
    while ((m = re.exec(text))) if (/(Counter|Till|Desk|Window|Teller)/i.test(m[1])) staff.add(m[2]);
  }
  for (const rel of staff) {
    const radius = talkRadiusOf(root, rel);
    if (radius == null) errors.push(`${rel}: counter staff with no Talk, Quest or RoleTalk context`);
    else if (radius < CHEST_REACH_MIN) errors.push(`${rel}: counter staff talk radius ${radius} m is under ${CHEST_REACH_MIN} m, a customer across the counter cannot reach it`);
  }
  return { errors, checked: staff.size };
}
