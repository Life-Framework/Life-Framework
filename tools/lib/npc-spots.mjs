// tools/lib/npc-spots.mjs - every place a character can be put in the world, read from the data (#3831).
//
// Two characters on one spot stand inside each other: a pub where two residents sat on the one bench
// anchor, a scene's walkers beside the person already standing at the door. Every system that puts a
// character down reads its place from data, so the whole map can be walked offline:
//   host      a character placed in a layer (clerk, mayor, quest giver), always there;
//   resident  the timetable line a resident stands on, resolved to an anchor exactly as
//             LF_ResidentAnchors.ResolveIn does (the place by kind and id, then the anchor id);
//   walker    a scene's planned walker at its spawn marker, lined up sideways as
//             LF_EventSceneSpawn.SpawnPerson does (rank k: 0, +1, -1, +2 ... spacings).
// Two spots closer than MIN_M that can be live together are a collision. The runtime keeps the same
// rule for what the data cannot say (LF_NpcSpots.c): it moves a newcomer onto a free spot.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";

// Characters this close (metres, horizontal) share a body. A shoulder is about 0.5 m.
export const MIN_M = 0.8;
// Walkers on one spawn marker stand this far apart (LF_EventSceneSpawn.SPACING_M).
export const SPACING_M = 1.2;
// A loose resident anchor belongs to the nearest place within this many metres (LF_ResidentAnchors.OWN_RADIUS).
export const OWN_RADIUS_M = 30;

function walk(dir, ext, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) walk(path, ext, out);
    else if (name.endsWith(ext)) out.push(path);
  }
  return out;
}

// Top-level entities of a layer: { cls, name, prefab, x, y, z, yaw, body }.
export function parseLayer(text) {
  const out = [];
  const head = /^(\w+) (\S+)(?: : "(?:\{[0-9A-Fa-f]+\})?([^"]*)")? \{\r?\n([\s\S]*?)^\}/gm;
  let m;
  while ((m = head.exec(text))) {
    const body = m[4];
    const at = /^ coords (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)/m.exec(body);
    if (!at) continue;
    const yaw = /^ angleY (-?[\d.]+)/m.exec(body);
    out.push({ cls: m[1], name: m[2], prefab: m[3] || "", x: +at[1], y: +at[2], z: +at[3], yaw: yaw ? +yaw[1] : 0, body });
  }
  return out;
}

// A character standing in a layer for good. Seats, signs and props that only carry an NPC-ish name are not.
export function isHost(e) {
  if (/Seat|Chair|Armchair|Bench/.test(e.prefab)) return false;
  return e.cls === "LF_NpcBaseEntity" || e.cls === "SCR_ChimeraCharacter" || /(^|\/)(LF_Npc_|LF_Resident_|QuestGiver|LF_Shopkeeper|LF_IllicitBuyer)/.test(e.prefab);
}

export function placesOf(entities) {
  const out = [];
  for (const e of entities) {
    if (!/LF_PlaceComponent/.test(e.body)) continue;
    const kind = /m_sKind "([^"]*)"/.exec(e.body)?.[1];
    const id = /m_sId "([^"]*)"/.exec(e.body)?.[1] || "";
    if (kind) out.push({ kind, id, x: e.x, y: e.y, z: e.z, name: e.name });
  }
  return out;
}

export function anchorsOf(entities) {
  const out = [];
  for (const e of entities) {
    const id = /LF_ResidentAnchorComponent[\s\S]*?m_sAnchorId "([^"]*)"/.exec(e.body)?.[1];
    if (id) out.push({ id, x: e.x, y: e.y, z: e.z, name: e.name });
  }
  return out;
}

const dist2 = (a, b) => (a.x - b.x) ** 2 + (a.z - b.z) ** 2;
export const distXZ = (a, b) => Math.sqrt(dist2(a, b));

function nearest(list, from) {
  let best = null;
  for (const item of list) if (!best || dist2(item, from) < dist2(best, from)) best = item;
  return best;
}

// The places a (kind, placeId) can mean: the named one, else (no id, or an id nobody holds) every place of the kind.
export function candidatePlaces(places, kind, placeId) {
  const ofKind = places.filter((p) => p.kind === kind);
  if (placeId) {
    const named = ofKind.filter((p) => p.id === placeId);
    if (named.length) return named;
  }
  return ofKind;
}

// LF_ResidentAnchors.FindAnchor: a loose anchor within OWN_RADIUS_M of the place whose nearest place of the kind is
// this one; the nearest wins. No anchor id, or none found: the place's own origin.
export function spotOf(place, anchorId, anchors, places) {
  if (anchorId) {
    const ofKind = places.filter((p) => p.kind === place.kind);
    const mine = anchors.filter((a) => a.id === anchorId && distXZ(a, place) <= OWN_RADIUS_M && nearest(ofKind, a) === place);
    const found = nearest(mine, place);
    if (found) return { x: found.x, y: found.y, z: found.z, via: found.name };
  }
  return { x: place.x, y: place.y, z: place.z, via: place.name + " (place origin)" };
}

// ---- residents -------------------------------------------------------------------------------

// The text from `from` to the brace that closes the first block after it. Braces inside quotes (the GUID in
// `Class "{GUID}" {`) are not blocks.
function block(text, from) {
  let depth = 0;
  let quoted = false;
  for (let i = from; i < text.length; i++) {
    const c = text[i];
    if (c === '"') quoted = !quoted;
    else if (quoted) continue;
    else if (c === "{") depth++;
    else if (c === "}" && --depth === 0) return text.slice(from, i + 1);
  }
  return text.slice(from);
}

const minutes = (hhmm) => {
  const m = /^(\d{1,2}):(\d{2})$/.exec(hhmm || "");
  return m ? +m[1] * 60 + +m[2] : -1;
};

// Resident definitions of one config text: { id, op, homeKind, homePlaceId, workKind, workPlaceId, lines }.
export function parseResidents(text) {
  const out = [];
  const re = /LF_ResidentDef "\{[0-9A-Fa-f]+\}" \{/g;
  let m;
  while ((m = re.exec(text))) {
    const body = block(text, m.index);
    const field = (k) => new RegExp(`^\\s*${k} "([^"]*)"`, "m").exec(body.replace(/LF_ResidentEntryDef[\s\S]*$/, ""))?.[1] || "";
    const lines = [];
    const lre = /LF_ResidentEntryDef "\{[0-9A-Fa-f]+\}" \{/g;
    let l;
    while ((l = lre.exec(body))) {
      const lb = block(body, l.index);
      const get = (k, d = "") => new RegExp(`^\\s*${k} "([^"]*)"`, "m").exec(lb)?.[1] ?? d;
      const t = minutes(get("m_sTime"));
      if (t >= 0) lines.push({ t, time: get("m_sTime"), place: get("m_sPlace", "home"), anchor: get("m_sAnchor"), activity: get("m_sActivity") });
    }
    lines.sort((a, b) => a.t - b.t);
    out.push({ id: field("m_sId"), op: field("m_sOp"), homeKind: field("m_sHomeKind"), homePlaceId: field("m_sHomePlaceId"), workKind: field("m_sWorkKind"), workPlaceId: field("m_sWorkPlaceId"), lines });
  }
  return out;
}

// Core residents first, then each pack overlay: `remove` drops an id, `add`/`replace` sets it, `patch` keeps the
// core timetable unless the patch brings its own.
export function mergeResidents(sources) {
  const map = new Map();
  for (const defs of sources) {
    for (const d of defs) {
      if (d.op === "remove") map.delete(d.id);
      else if (d.op === "patch" && map.has(d.id)) {
        const old = map.get(d.id);
        map.set(d.id, { ...old, ...Object.fromEntries(Object.entries(d).filter(([k, v]) => v && !(Array.isArray(v) && !v.length))) });
      } else map.set(d.id, d);
    }
  }
  return [...map.values()].filter((d) => d.lines.length);
}

// The line a resident stands on at game minute `minute` (LF_ResidentTimetable.At: the last line started, wrapping).
export function lineAt(def, minute) {
  let found = def.lines[def.lines.length - 1];
  for (const l of def.lines) if (l.t <= minute) found = l;
  return found;
}

// Per resident, per distinct line: where it stands and which minutes of the day it is there.
export function residentSpots(defs, places, anchors) {
  const out = [];
  for (const d of defs) {
    const kinds = new Map();
    for (let minute = 0; minute < 1440; minute++) {
      const line = lineAt(d, minute);
      const kind = line.place === "work" ? d.workKind : d.homeKind;
      const placeId = line.place === "work" ? d.workPlaceId : d.homePlaceId;
      if (!kind) continue;
      const key = `${line.time}|${line.place}|${line.anchor}`;
      if (!kinds.has(key)) kinds.set(key, { line, kind, placeId, minutes: new Set() });
      kinds.get(key).minutes.add(minute);
    }
    for (const { line, kind, placeId, minutes: set } of kinds.values()) {
      for (const place of candidatePlaces(places, kind, placeId)) {
        const spot = spotOf(place, line.anchor, anchors, places);
        out.push({ type: "resident", who: d.id, what: `${line.place}/${line.anchor || "origin"} ${line.activity || "idle"} from ${line.time}`, activity: (line.activity || "").trim().toLowerCase(), ...spot, minutes: set });
      }
    }
  }
  return out;
}

// ---- scenes ----------------------------------------------------------------------------------

export function parseScenes(text) {
  const out = [];
  const re = /LF_EventScene \{/g;
  let m;
  while ((m = re.exec(text))) {
    const body = block(text, m.index);
    const id = /m_sSceneId "([^"]*)"/.exec(body)?.[1];
    const count = /m_iNpcCount (\d+)/.exec(body)?.[1];
    if (id && count) out.push({ id, count: +count });
  }
  return out;
}

export function markersOf(entities) {
  const out = [];
  for (const e of entities) {
    if (!/LF_EventWaypointComponent/.test(e.body)) continue;
    const scene = /m_sSceneId "([^"]*)"/.exec(e.body)?.[1];
    if (!scene) continue;
    const order = +(/m_iOrder (\d+)/.exec(e.body)?.[1] || 0);
    let kind = "waypoint";
    if (/EventMarker_Spawn/.test(e.prefab)) kind = "spawn";
    else if (/EventMarker_Target/.test(e.prefab)) kind = "target";
    else if (/EventMarker_Item/.test(e.prefab)) kind = "item";
    out.push({ scene, order, kind, x: e.x, y: e.y, z: e.z, yaw: e.yaw, name: e.name });
  }
  return out;
}

// LF_EventSceneSpawn.SpawnPerson: walker `index` of `count` on marker index % markers, rank index / markers, lined up
// along the marker's right axis (0, +1, -1, +2 ... spacings; the first goes to the left, as in the script).
export function walkerSpots(scene, spawns) {
  const sorted = [...spawns].sort((a, b) => a.order - b.order);
  const out = [];
  if (!sorted.length) return out;
  for (let i = 0; i < scene.count; i++) {
    const marker = sorted[i % sorted.length];
    const rank = Math.floor(i / sorted.length);
    let side = Math.ceil(rank / 2) * SPACING_M;
    if (rank % 2 === 0) side = -side;
    const rad = (marker.yaw * Math.PI) / 180;
    out.push({ type: "walker", who: `${scene.id}#${i}`, what: `scene ${scene.id} walker ${i} at ${marker.name}`, x: marker.x + Math.cos(rad) * side, y: marker.y, z: marker.z - Math.sin(rad) * side, scene: scene.id });
  }
  return out;
}

// ---- seats ----------------------------------------------------------------------------------

// How far from the anchor a seat may be for `sit` (LF_ResidentActivity.SEAT_RANGE).
export const SEAT_RANGE_M = 2.5;

// The sit places of a layer: one per LF_SitAction in the seat prefab (a two-seat bench is two), at the prop's position.
export function seatsOf(entities, prefabText) {
  const out = [];
  for (const e of entities) {
    if (!/Seat/.test(e.prefab)) continue;
    const places = (prefabText(e.prefab).match(/LF_SitAction "{/g) || []).length;
    for (let i = 0; i < places; i++) out.push({ x: e.x, y: e.y, z: e.z, name: e.name });
  }
  return out;
}

// Residents who sit at one anchor at the same minute need a seat each: LF_NpcLoiter hands out one sit place per body,
// and a sitter with none left stands on the anchor inside the others (the cafe bench, three men and two places).
export function sitCollisions(spots, seats, min = MIN_M) {
  const sitters = spots.filter((s) => s.type === "resident" && s.activity === "sit");
  const bad = [];
  const seen = new Set();
  for (let minute = 0; minute < 1440; minute++) {
    const live = sitters.filter((s) => s.minutes.has(minute));
    for (const s of live) {
      const group = live.filter((o) => distXZ(o, s) < min && Math.abs(o.y - s.y) <= 3);
      const capacity = seats.filter((p) => distXZ(p, s) <= SEAT_RANGE_M).length;
      if (group.length <= Math.max(capacity, 1)) continue; // a lone sitter with no seat stands alone, which is not a collision
      const names = [...new Set(group.map((g) => g.who))].sort();
      const key = names.join("|") + "@" + s.x.toFixed(1) + "," + s.z.toFixed(1);
      if (seen.has(key)) continue;
      seen.add(key);
      const other = group.find((g) => g.who !== s.who) || group[0];
      const clock = (m) => `${String(Math.floor(m / 60)).padStart(2, "0")}:${String(m % 60).padStart(2, "0")}`;
      bad.push({ a: s, b: other, d: 0, shared: `${clock(minute)} (${names.join(", ")} sit on ${capacity} place(s))` });
    }
  }
  return bad;
}

// ---- the check -------------------------------------------------------------------------------

// Every pair of spots that stand closer than `min` and can be live at once. Two residents need a minute they
// share; a host is always live; walkers of different scenes never share a stage (one scene runs at a time), but
// a walker meets every host and resident.
export function collisions(spots, min = MIN_M) {
  const bad = [];
  for (let i = 0; i < spots.length; i++) {
    for (let j = i + 1; j < spots.length; j++) {
      const a = spots[i];
      const b = spots[j];
      if (a.type === "walker" && b.type === "walker" && a.scene !== b.scene) continue;
      if (a.type === "resident" && b.type === "resident" && a.who === b.who) continue;
      if (a.type === "resident" && b.type === "resident" && a.activity === "sit" && b.activity === "sit") continue; // sitCollisions counts the seats
      if (Math.abs(a.y - b.y) > 3) continue;
      const d = distXZ(a, b);
      if (d >= min) continue;
      let shared = "always";
      if (a.minutes && b.minutes) {
        const both = [...a.minutes].filter((x) => b.minutes.has(x));
        if (!both.length) continue;
        const clock = (t) => `${String(Math.floor(t / 60)).padStart(2, "0")}:${String(t % 60).padStart(2, "0")}`;
        shared = `${clock(both[0])}-${clock(both[both.length - 1] + 1)}`;
      }
      bad.push({ a, b, d, shared });
    }
  }
  return bad;
}

// Every spot of a repo tree: layers of the pack worlds (the flagship's), residents of core plus packs, scene walkers.
export function collectSpots(root) {
  const layers = walk(join(root, "addons", "LF_Everon1989", "Worlds", "MainWorld"), ".layer");
  const entities = [];
  for (const f of layers) entities.push(...parseLayer(readFileSync(f, "utf8")).map((e) => ({ ...e, file: f })));
  const places = placesOf(entities);
  const anchors = anchorsOf(entities);
  const spots = [];
  const prefabText = (prefab) => {
    for (const addon of ["LifeFramework", "LF_Everon1989"]) {
      const f = join(root, "addons", addon, prefab);
      if (existsSync(f)) return readFileSync(f, "utf8");
    }
    return "";
  };
  const seats = seatsOf(entities, prefabText);
  for (const e of entities) if (isHost(e)) spots.push({ type: "host", who: e.name, what: `placed ${e.prefab.split("/").pop()}`, x: e.x, y: e.y, z: e.z });

  const sources = [];
  for (const path of [join(root, "addons", "LifeFramework", "Configs", "Residents", "LF_Residents.conf"), ...walk(join(root, "addons", "LF_Everon1989", "Configs", "Residents"), ".conf")]) {
    if (existsSync(path)) sources.push(parseResidents(readFileSync(path, "utf8")));
  }
  spots.push(...residentSpots(mergeResidents(sources), places, anchors));

  const markers = markersOf(entities);
  const scenes = [];
  for (const path of walk(join(root, "addons", "LF_Everon1989", "Configs", "Events"), ".conf")) scenes.push(...parseScenes(readFileSync(path, "utf8")));
  for (const scene of scenes) spots.push(...walkerSpots(scene, markers.filter((k) => k.scene === scene.id && k.kind === "spawn")));
  return { spots, markers, places, anchors, scenes, seats };
}

export function describe(spot) {
  return `${spot.type} ${spot.who} (${spot.what}) at ${spot.x.toFixed(1)} ${spot.z.toFixed(1)}`;
}
