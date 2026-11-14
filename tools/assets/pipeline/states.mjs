// Stage "states": how an item looks and sits in each state it has (docs/asset-pipeline/states.md).
// An asset declares only the states it needs in spec.states; the stage writes those settings into the
// prefab, checks them, and with --shots takes one picture per declared state in a session.
//
//   inventory  PreviewRenderAttributes: the camera the engine renders the inventory picture with
//   ground     ItemPhysAttributes: which model axis faces up when dropped, and a rest offset/rotation
//   hand       the vanilla hand pose to borrow (its anim graph, anim set and gadget anim variable) and
//              HandAdditiveRotation/Translation, computed so the model's grip point sits in that
//              pose's closed hand, turned the way the spec says
//   use        declared only: what each use state needs (a mesh variant, a bone, an animation)
//
// How a held gadget is placed (measured 2026-11-14 with the harness verb heldxf, docs/asset-pipeline/
// states.md "Hand"): the item's origin is put on the character's LeftHandProp bone, its axes on the
// bone's axes. HandAdditiveTranslation moves it in the bone's frame. HandAdditiveRotation (a b c)
// turns it: the rows of the item's axes in the bone's frame are Rz(c) * Rx(a) * Ry(b), with
// Rx(90) taking item y to -bone z and item z to bone y (an exact fit to four measured rotations).
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { addonPath } from "./addon.mjs";

// Vanilla hold sets a gadget can borrow whole. `grip` is the centre of the closed hand and `out` the
// direction away from the palm, both in the LeftHandProp frame (x right, y up, z forward when the
// arm is raised in front), read with heldxf while holding the vanilla item (game 1.9).
export const HAND_POSES = {
  // The MX991 angle-head flashlight: a fist closed round an upright column about 5 cm thick, the arm
  // raised in front. The flashlight's origin is its base, on the bone; the knuckles sit 4.4 cm left
  // and 5.6 cm behind the column, the fist spans 2 to 8 cm above the bone.
  fist: {
    graph: "{82906ACD75DF8523}Assets/Items/Equipment/workspaces/Flashlight_MX991.agr",
    instance: "{903E4A0B96B34B8F}Assets/Items/Equipment/workspaces/MX991_player.asi",
    variable: "MX991",
    grip: [0, 0.045, 0],
    out: [0.616, 0, 0.788],
    fits: "an item held upright in a fist: a torch, a radio handset, cuffs held by the lock boxes",
  },
};

// RestingUP values the engine knows (vanilla prefabs, 2026-11-14): which model axis faces up when dropped.
export const RESTING_UP = ["Up", "Down", "Left", "Right", "Forward", "Backward"];
// Kinds a player holds in the hand: with a states block they must declare the hand state.
export const HANDHELD = ["gadget"];
const AXIS = { x: 0, y: 1, z: 2 };
const AXIS_RE = /^[+-][xyz]$/;

const isVec = (v, n = 3) => Array.isArray(v) && v.length === n && v.every((x) => typeof x === "number" && Number.isFinite(x));
const fmt = (n) => String(+(Math.abs(n) < 5e-5 ? 0 : n).toFixed(4));
const vec = (v) => v.map(fmt).join(" ");
const rad = (a) => (a * Math.PI) / 180;
const deg = (r) => (r * 180) / Math.PI;
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const mul = (A, B) => A.map((r) => [0, 1, 2].map((j) => r[0] * B[0][j] + r[1] * B[1][j] + r[2] * B[2][j]));

export const hasStates = (spec) => !!spec.states && typeof spec.states === "object" && Object.keys(spec.states).length > 0;
export const declared = (spec) => (hasStates(spec) ? ["inventory", "ground", "hand", "use"].filter((s) => spec.states[s] !== undefined) : []);

// The engine's rotation for HandAdditiveRotation [a, b, c]: rows are the item's x, y, z axes in the bone frame.
export const rotX = (a) => { const c = Math.cos(rad(a)), s = Math.sin(rad(a)); return [[1, 0, 0], [0, c, -s], [0, s, c]]; };
export const rotY = (a) => { const c = Math.cos(rad(a)), s = Math.sin(rad(a)); return [[c, 0, -s], [0, 1, 0], [s, 0, c]]; };
export const rotZ = (a) => { const c = Math.cos(rad(a)), s = Math.sin(rad(a)); return [[c, -s, 0], [s, c, 0], [0, 0, 1]]; };
export const additiveMatrix = ([a, b, c]) => mul(rotZ(c), mul(rotX(a), rotY(b)));

// Angles [a, b, c] whose additiveMatrix is M (rows = item axes in the bone frame).
export function additiveAngles(M) {
  const sa = Math.max(-1, Math.min(1, M[2][1]));
  const a = Math.asin(sa);
  if (Math.abs(Math.cos(a)) < 1e-6) {
    // Item z along the bone's y: b and c turn about the same axis; put it all in b.
    return [deg(a), deg(Math.atan2(-M[0][2], M[0][0])), 0];
  }
  return [deg(a), deg(Math.atan2(M[2][0], M[2][2])), deg(Math.atan2(-M[0][1], M[1][1]))];
}

const axisVec = (token) => { const v = [0, 0, 0]; v[AXIS[token[1]]] = token[0] === "-" ? -1 : 1; return v; };

// The hand state as prefab values: { graph, instance, variable, rotation, translation }.
//   hand.pose  a HAND_POSES key
//   hand.grip  the model point (metres, model space) the hand closes on
//   hand.up    the model axis that runs up through the fist ("+z")
//   hand.out   the model axis that points away from the palm ("+x")
//   hand.turn  degrees about the fist's column on top of the pose's own out direction (default 0)
export function handValues(hand) {
  const pose = HAND_POSES[hand.pose];
  const t = rad(hand.turn || 0);
  const o = pose.out;
  const out = [o[0] * Math.cos(t) - o[2] * Math.sin(t), o[1], o[0] * Math.sin(t) + o[2] * Math.cos(t)];
  const up = [0, 1, 0];
  // Rows of M: where each model axis points in the bone frame.
  const rows = [null, null, null];
  const put = (token, dir) => { rows[AXIS[token[1]]] = token[0] === "-" ? dir.map((v) => -v) : dir; };
  put(hand.up, up);
  put(hand.out, out);
  const missing = rows.findIndex((r) => r === null);
  if (missing === 0) rows[0] = cross(rows[1], rows[2]);
  if (missing === 1) rows[1] = cross(rows[2], rows[0]);
  if (missing === 2) rows[2] = cross(rows[0], rows[1]);
  const rotation = additiveAngles(rows).map((a) => Math.round(a * 100) / 100);
  const M = additiveMatrix(rotation);
  const g = hand.grip;
  const gM = [0, 1, 2].map((j) => g[0] * M[0][j] + g[1] * M[1][j] + g[2] * M[2][j]);
  const translation = pose.grip.map((c, j) => c - gM[j]);
  return { graph: pose.graph, instance: pose.instance, variable: pose.variable, rotation, translation, matrix: M };
}

// Everything wrong with spec.states, as a list (the spec stage reports it).
export function statesProblems(spec) {
  const out = [];
  if (spec.states === undefined) return out;
  if (!spec.states || typeof spec.states !== "object" || Array.isArray(spec.states)) return ["states must be an object: { inventory, ground, hand, use }"];
  const s = spec.states;
  for (const k of Object.keys(s)) if (!["inventory", "ground", "hand", "use", "prefab"].includes(k)) out.push(`states.${k}: the states are inventory, ground, hand and use (and prefab, when the prefab is not <prefab_dir>/<name>.et)`);
  if (s.prefab !== undefined && !/^Prefabs\/.+\.et$/.test(s.prefab)) out.push("states.prefab must be a Prefabs/... .et path");
  if (HANDHELD.includes(spec.kind) && !s.hand) out.push(`states.hand is missing: a ${spec.kind} is held in the hand, so it declares how (pose, grip, up, out)`);
  if (s.inventory !== undefined) {
    const i = s.inventory;
    if (!isVec(i.angles)) out.push("states.inventory.angles must be [pitch, yaw, roll] in degrees (CameraOrbitAngles)");
    if (!(i.distance > 0.05 && i.distance < 10)) out.push("states.inventory.distance must be metres from the camera to the item");
    if (i.fov !== undefined && !(i.fov >= 5 && i.fov <= 90)) out.push("states.inventory.fov must be 5 to 90 degrees");
    if (i.offset !== undefined && !isVec(i.offset)) out.push("states.inventory.offset must be [x, y, z] metres (CameraOffset)");
  }
  if (s.ground !== undefined) {
    const g = s.ground;
    if (!RESTING_UP.includes(g.up)) out.push(`states.ground.up must be one of ${RESTING_UP.join(", ")}: the model axis that faces up when dropped`);
    if (g.offset !== undefined && !isVec(g.offset)) out.push("states.ground.offset must be [x, y, z] metres (RestingAdditiveOffsetLS)");
    if (g.rotation !== undefined && !isVec(g.rotation)) out.push("states.ground.rotation must be [x, y, z] degrees (RestingAdditiveRotationLS)");
  }
  if (s.hand !== undefined) {
    const h = s.hand;
    if (!HAND_POSES[h.pose]) out.push(`states.hand.pose must be one of ${Object.keys(HAND_POSES).join(", ")} (a vanilla hold set; a new one is measured with heldxf first)`);
    if (!isVec(h.grip)) out.push("states.hand.grip must be the model point [x, y, z] the hand closes on");
    if (!AXIS_RE.test(h.up || "")) out.push('states.hand.up must be a model axis such as "+z": the one that runs up through the fist');
    if (!AXIS_RE.test(h.out || "")) out.push('states.hand.out must be a model axis such as "+x": the one that points away from the palm');
    if (AXIS_RE.test(h.up || "") && AXIS_RE.test(h.out || "") && h.up[1] === h.out[1]) out.push("states.hand.up and states.hand.out must be different axes");
    if (h.turn !== undefined && !(typeof h.turn === "number" && Math.abs(h.turn) <= 180)) out.push("states.hand.turn must be -180 to 180 degrees");
    if (isVec(h.grip) && spec.size_m) {
      const half = Math.max(spec.size_m.length, spec.size_m.height, spec.size_m.width) / 2 + 0.01;
      if (h.grip.some((v) => Math.abs(v) > half)) out.push(`states.hand.grip ${vec(h.grip)} is outside the model (half size ${fmt(half)} m)`);
    }
  }
  if (s.use !== undefined) {
    if (!Array.isArray(s.use)) out.push("states.use must be a list of { name, needs, ... }");
    else {
      const names = new Set();
      for (const u of s.use) {
        if (!/^[a-z][a-z0-9_]*$/.test(u?.name || "")) out.push(`states.use: ${JSON.stringify(u?.name)} needs a lowercase name`);
        else if (names.has(u.name)) out.push(`states.use: ${u.name} is listed twice`);
        names.add(u?.name);
        if (!["mesh", "prefab", "bone", "anim"].includes(u?.needs)) out.push(`states.use ${u?.name}: needs must be mesh, prefab, bone or anim`);
        if (u?.needs === "prefab" && !/^Prefabs\/.+\.et$/.test(u.prefab || "")) out.push(`states.use ${u.name}: a prefab use state names its Prefabs/... .et`);
        // A planned use state records what it will need; only a built one must point at something real.
        if (u?.needs === "bone" && !u.planned && !(spec.anim?.bones || []).some((b) => b.name === u.bone)) out.push(`states.use ${u.name}: bone ${u.bone || "?"} is not in anim.bones (or mark it planned)`);
      }
    }
  }
  return out;
}

// ---- the prefab text --------------------------------------------------------------------------
// A block is found by a regex on its header line ("Name Class "{GUID}" {") and runs to its matching
// brace. Values are lines "Key value" directly inside it (not in a nested block).
function findBlock(text, header, from = 0, to = text.length) {
  const re = new RegExp(header.source, "g");
  re.lastIndex = from;
  const m = re.exec(text);
  if (!m || m.index >= to) return null;
  const open = text.indexOf("{", m.index + m[0].length - 1);
  let depth = 0;
  for (let i = open; i < to; i++) {
    if (text[i] === "{") depth++;
    else if (text[i] === "}" && --depth === 0) return { start: m.index, open, close: i };
  }
  return null;
}

function directLines(text, block) {
  // [{ key, start, end, depth }] of the lines at the block's own level.
  const out = [];
  let depth = 0;
  let i = text.indexOf("\n", block.open) + 1;
  while (i > 0 && i < block.close) {
    const end = text.indexOf("\n", i);
    const line = text.slice(i, end);
    if (depth === 0) {
      const m = /^\s*([A-Za-z_][\w]*)\s/.exec(line);
      if (m && !line.trimEnd().endsWith("{")) out.push({ key: m[1], start: i, end });
    }
    for (const ch of line) { if (ch === "{") depth++; else if (ch === "}") depth--; }
    i = end + 1;
  }
  return out;
}

export function readValue(text, header, key, within) {
  const b = findBlock(text, header, within?.open, within?.close);
  if (!b) return undefined;
  const line = directLines(text, b).find((l) => l.key === key);
  return line ? text.slice(line.start, line.end).trim().slice(key.length).trim() : null;
}

function setValue(text, header, key, value, within) {
  const b = findBlock(text, header, within?.open, within?.close);
  if (!b) throw new Error(`no block ${header.source} in the prefab`);
  const line = directLines(text, b).find((l) => l.key === key);
  if (line) {
    const pad = /^\s*/.exec(text.slice(line.start, line.end))[0];
    return text.slice(0, line.start) + `${pad}${key} ${value}` + text.slice(line.end);
  }
  // A new line goes last in the block, one space deeper than the block's header (the .et layout).
  const at = text.lastIndexOf("\n", b.close) + 1;
  const pad = text.slice(text.lastIndexOf("\n", b.start) + 1, b.start).replace(/\S.*$/, "") + " ";
  return text.slice(0, at) + `${pad}${key} ${value}\n` + text.slice(at);
}

const H = {
  phys: /ItemPhysAttributes ItemPhysicalAttributes [^\n{]*\{/,
  anim: /ItemAnimationAttributes ItemAnimationAttributes "\{[0-9A-F]{16}\}" \{/,
  attach: /AnimationAttachmentInfo "\{[0-9A-F]{16}\}" \{/,
  preview: /PreviewRenderAttributes "\{[0-9A-F]{16}\}" \{/,
  gadget: /SCR_GadgetComponent "\{[0-9A-F]{16}\}" \{/,
};

// The values the spec's states put in the prefab: [[block, key, value, inside]] (inside: a block the
// first one is nested in).
export function stateValues(spec) {
  const s = spec.states || {};
  const out = [];
  if (s.inventory) {
    const i = s.inventory;
    out.push(["preview", "CameraOrbitAngles", vec(i.angles)], ["preview", "CameraDistanceToItem", fmt(i.distance)], ["preview", "FOV", fmt(i.fov ?? 20)]);
    if (i.offset) out.push(["preview", "CameraOffset", vec(i.offset)]);
  }
  if (s.ground) {
    out.push(["phys", "RestingUP", s.ground.up]);
    if (s.ground.offset) out.push(["phys", "RestingAdditiveOffsetLS", vec(s.ground.offset)]);
    if (s.ground.rotation) out.push(["phys", "RestingAdditiveRotationLS", vec(s.ground.rotation)]);
  }
  if (s.hand) {
    const h = handValues(s.hand);
    out.push(["attach", "AnimGraph", JSON.stringify(h.graph), "anim"], ["attach", "AnimInstance", JSON.stringify(h.instance), "anim"]);
    out.push(["anim", "HandAdditiveRotation", vec(h.rotation)], ["anim", "HandAdditiveTranslation", vec(h.translation)]);
    out.push(["gadget", "m_eAnimVariable", h.variable]);
  }
  return out;
}

const norm = (v) => String(v ?? "").trim().split(/\s+/).map((x) => (/^-?[\d.]+$/.test(x) ? fmt(Number(x)) : x)).join(" ");

// Where the prefab differs from the spec's states: ["Key: prefab has X, the spec says Y"].
export function statesDrift(text, spec) {
  const out = [];
  for (const [block, key, value, inside] of stateValues(spec)) {
    const within = inside ? findBlock(text, H[inside]) : undefined;
    if (inside && !within) { out.push(`${key}: the prefab has no ${inside} block`); continue; }
    const have = readValue(text, H[block], key, within);
    if (have === undefined) out.push(`${key}: the prefab has no ${block} block`);
    else if (norm(have) !== norm(value)) out.push(`${key}: the prefab has ${have ?? "nothing"}, the spec says ${value}`);
  }
  return out;
}

// The prefab text with the spec's states written in. Blocks must exist (the gadget scaffold writes them).
export function applyStates(text, spec) {
  let t = text;
  for (const [block, key, value, inside] of stateValues(spec)) {
    const within = inside ? findBlock(t, H[inside]) : undefined;
    if (inside && !within) throw new Error(`the prefab has no ${inside} block for ${key}`);
    t = setValue(t, H[block], key, value, within);
  }
  return t;
}

export const prefabPath = (root, spec) => join(addonPath(root, spec), spec.states?.prefab || `${spec.prefab_dir}/${spec.name}.et`);

// The stage: check (and with fix, write) the prefab; list the pictures to take.
export function stageStates(ctx, { fix = false } = {}) {
  const { root, spec } = ctx;
  if (!hasStates(spec)) { console.log("states: the spec declares none; the prefab keeps what the scaffold wrote"); return { skipped: true }; }
  const problems = statesProblems(spec);
  if (problems.length) throw new Error("states problems:\n  " + problems.join("\n  "));
  const file = prefabPath(root, spec);
  if (!existsSync(file)) throw new Error(`no prefab at ${file}; run the prefab stage first`);
  const before = readFileSync(file, "utf8");
  let drift = statesDrift(before, spec);
  if (drift.length && fix) {
    writeFileSync(file, applyStates(before, spec));
    console.log(`states: wrote ${drift.length} value(s) into ${file}`);
    drift = statesDrift(readFileSync(file, "utf8"), spec);
  }
  if (drift.length) throw new Error(`the prefab differs from spec.states (asset states ${spec.slug} --fix writes them):\n  ` + drift.join("\n  "));
  const h = spec.states.hand ? handValues(spec.states.hand) : null;
  if (h) console.log(`states: hand pose ${spec.states.hand.pose}, rotation ${vec(h.rotation)}, translation ${vec(h.translation)}`);
  for (const u of spec.states.use || []) console.log(`states: use state ${u.name} needs ${u.needs}${u.prefab ? ` (${u.prefab})` : ""}${u.bone ? ` (bone ${u.bone})` : ""}${u.planned ? ", planned" : ""}`);
  const shots = shotList(spec);
  console.log(`states: pictures to take: ${shots.join(", ")}; tools\\cli asset states ${spec.slug} --shots`);
  return { states: declared(spec), shots };
}

// What a session can photograph: the states with a picture of their own, plus "use" when a use state
// is built on a bone (the harness switches the item on with itemon; a mesh or prefab state is a
// separate item and has its own picture).
export function shotList(spec) {
  const out = declared(spec).filter((s) => s !== "use");
  if ((spec.states?.use || []).some((u) => u.needs === "bone" && !u.planned)) out.push("use");
  return out;
}

// ---- the pictures ---------------------------------------------------------------------------------
// One picture per declared state (use states excepted). The ground and inventory pictures need no session:
// `look item` (tools/visual/look-item.mjs) poses the item the way the inventory drops it and renders the
// prefab's own preview camera in a hidden Workbench. Only hand (and use) need a fresh `--profile full`
// session with two clients: agent-box/proofs/asset-states-proof.mjs gives the item and holds it. The PNGs
// land in tmp/assets/<slug>/states/. Each step is a repo command that blocks with its own budget.
export function shootStates(root, spec, dir) {
  const meta = `${prefabPath(root, spec)}.meta`;
  const guid = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  if (!guid) { console.log(`FAIL  no GUID in ${meta}`); return 1; }
  const rel = spec.states.prefab || `${spec.prefab_dir}/${spec.name}.et`;
  const shots = shotList(spec);
  const out = join(dir, "states");
  const lookShots = shots.filter((s) => s === "ground" || s === "inventory");
  const sessionShots = shots.filter((s) => !lookShots.includes(s));
  if (lookShots.length) {
    const cli = join(root, "tools", "cli.mjs");
    const r = spawnSync(process.execPath, [cli, "look", "item", `{${guid}}${rel}`, "--states", lookShots.join(","), "--wait"], { cwd: root, stdio: "inherit", windowsHide: true });
    if (r.status !== 0) { console.log("FAIL  look item"); return 1; }
    const leaf = rel.split("/").pop().replace(/\.et$/, "");
    mkdirSync(out, { recursive: true });
    for (const s of lookShots) copyFileSync(join(root, "tmp", "look", `item-${leaf.replace(/[^A-Za-z0-9_-]/g, "_")}`, `${s}.png`), join(out, `state_${s}_${leaf}.png`));
    console.log(`states: ${lookShots.join(", ")} from look item (no session); ${sessionShots.length ? "hand/use need a session" : "no session was used"}`);
  }
  if (!sessionShots.length) { console.log(`states: pictures in ${out}; read every one`); return 0; }
  const session = join(root, "agent-box", "session.mjs");
  const node = (args) => spawnSync(process.execPath, [session, ...args], { cwd: root, stdio: "inherit", windowsHide: true }).status;
  if (node(["up", "--clients", "2", "--profile", "full", "--wait"]) !== 0) { console.log("FAIL  session up"); return 1; }
  let code = 1;
  try {
    code = node(["asset-states-proof", `{${guid}}${rel}`, sessionShots.join(","), out]);
  } finally {
    node(["down"]);
  }
  console.log(code === 0 ? `states: pictures in ${out}; read every one` : "FAIL  asset-states-proof");
  return code;
}
