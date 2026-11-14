// Character clips without a rig: poses on the vanilla character skeleton, solved here and written as
// a .txa that Workbench compiles to an .anm (docs/asset-pipeline/animation.md).
//
// The skeleton is skeletons/character.json: every bone's parent-relative rest position and rotation,
// read from the vanilla animation test character with xob.mjs. A .txa key holds the same
// parent-relative rotation (checked against Anims/Anm/Player/Carry/p_2h_carry_logsmall.txa: its
// frame 0 is the rest pose to six decimals), so forward kinematics here is what the engine plays.
//
// Model space is Enfusion's: x right, y up, z forward, metres. Quaternions are x y z w.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------- vectors and quaternions
export const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
export const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
export const mul = (a, s) => [a[0] * s, a[1] * s, a[2] * s];
export const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
export const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
export const len = (a) => Math.hypot(a[0], a[1], a[2]);
export const norm = (a) => { const l = len(a); return l > 1e-12 ? mul(a, 1 / l) : [0, 0, 0]; };
export const lerp = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);

export const qmul = (a, b) => [
  a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
  a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
  a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
  a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
];
export const qinv = (q) => [-q[0], -q[1], -q[2], q[3]];
export const qnorm = (q) => { const l = Math.hypot(...q); return q.map((v) => v / l); };
export const qrot = (q, v) => qmul(qmul(q, [v[0], v[1], v[2], 0]), qinv(q)).slice(0, 3);
export function qaxis(axis, deg) {
  const a = norm(axis), h = (deg * Math.PI) / 360, s = Math.sin(h);
  return [a[0] * s, a[1] * s, a[2] * s, Math.cos(h)];
}
// The shortest rotation that turns direction a onto direction b.
export function qbetween(a, b) {
  a = norm(a); b = norm(b);
  const d = dot(a, b);
  if (d > 1 - 1e-9) return [0, 0, 0, 1];
  if (d < -1 + 1e-9) {
    const side = Math.abs(a[0]) < 0.9 ? [1, 0, 0] : [0, 1, 0];
    return qaxis(cross(a, side), 180);
  }
  const c = cross(a, b);
  return qnorm([c[0], c[1], c[2], 1 + d]);
}
export function qslerp(a, b, t) {
  let d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  if (d < 0) { b = b.map((v) => -v); d = -d; }
  if (d > 0.9995) return qnorm(a.map((v, i) => v + (b[i] - v) * t));
  const th = Math.acos(d), s = Math.sin(th);
  const wa = Math.sin((1 - t) * th) / s, wb = Math.sin(t * th) / s;
  return a.map((v, i) => v * wa + b[i] * wb);
}
// Rotation matrix columns (x, y, z axes) to a quaternion.
export function qfromAxes(x, y, z) {
  const m = [x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2]];
  const tr = m[0] + m[4] + m[8];
  let q;
  if (tr > 0) { const s = Math.sqrt(tr + 1) * 2; q = [(m[7] - m[5]) / s, (m[2] - m[6]) / s, (m[3] - m[1]) / s, s / 4]; }
  else if (m[0] > m[4] && m[0] > m[8]) { const s = Math.sqrt(1 + m[0] - m[4] - m[8]) * 2; q = [s / 4, (m[1] + m[3]) / s, (m[2] + m[6]) / s, (m[7] - m[5]) / s]; }
  else if (m[4] > m[8]) { const s = Math.sqrt(1 + m[4] - m[0] - m[8]) * 2; q = [(m[1] + m[3]) / s, s / 4, (m[5] + m[7]) / s, (m[2] - m[6]) / s]; }
  else { const s = Math.sqrt(1 + m[8] - m[0] - m[4]) * 2; q = [(m[2] + m[6]) / s, (m[5] + m[7]) / s, s / 4, (m[3] - m[1]) / s]; }
  return qnorm(q);
}

// ---------------------------------------------------------------- skeleton
export function loadSkeleton(file = join(HERE, "skeletons", "character.json")) {
  const data = JSON.parse(readFileSync(file, "utf8"));
  return makeSkeleton(data.nodes);
}

export function makeSkeleton(nodes) {
  const by = Object.fromEntries(nodes.map((n) => [n.name, n]));
  const children = {};
  for (const n of nodes) (children[n.parent] ||= []).push(n.name);
  return { nodes, by, children };
}

// A pose is { q: { bone: local rotation }, t: { bone: local position } }; a bone it does not name rests.
export const restPose = () => ({ q: {}, t: {} });
export const clonePose = (p) => ({ q: { ...p.q }, t: { ...p.t } });

// World (model-space) position and rotation of every bone in a pose.
export function fk(sk, pose) {
  const out = {};
  for (const n of sk.nodes) {
    const q = pose.q[n.name] || n.quat, t = pose.t[n.name] || n.pos;
    const p = out[n.parent];
    out[n.name] = p ? { pos: add(p.pos, qrot(p.rot, t)), rot: qmul(p.rot, q) } : { pos: t, rot: q };
  }
  return out;
}

// Turn a bone by a model-space rotation, keeping its children attached.
export function rotateWorld(sk, pose, bone, worldDelta) {
  const w = fk(sk, pose);
  const parent = w[sk.by[bone].parent];
  const pr = parent ? parent.rot : [0, 0, 0, 1];
  pose.q[bone] = qnorm(qmul(qinv(pr), qmul(worldDelta, w[bone].rot)));
}

// Give a bone an absolute model-space rotation.
export function setWorldRot(sk, pose, bone, worldRot) {
  const w = fk(sk, pose);
  const parent = w[sk.by[bone].parent];
  pose.q[bone] = qnorm(qmul(qinv(parent ? parent.rot : [0, 0, 0, 1]), worldRot));
}

const SIDE = { left: "Left", right: "Right" };

// The hand's own directions in a pose, in model space: along the fingers, across the knuckles
// (index towards pinky), and out of the palm.
export function handFrame(sk, pose, side) {
  const S = SIDE[side], w = fk(sk, pose);
  const fingers = norm(sub(w[`${S}HandMiddle2`].pos, w[`${S}Hand`].pos));
  let across = sub(w[`${S}HandPinky2`].pos, w[`${S}HandIndex2`].pos);
  across = norm(sub(across, mul(fingers, dot(across, fingers))));
  // The palm faces the same way on both hands once the mirrored knuckle direction is accounted for.
  const palm = side === "right" ? cross(across, fingers) : cross(fingers, across);
  return { fingers, across, palm: norm(palm) };
}

// Two-bone IK for a limb (upper, lower, end bone names): puts the end bone at `target` with the
// middle joint bent towards `pole`, a model-space point. Returns the target it could reach.
export function limbIk(sk, pose, [upper, lower, end], target, pole) {
  let w = fk(sk, pose);
  const s = w[upper].pos;
  const l1 = len(sub(w[lower].pos, s)), l2 = len(sub(w[end].pos, w[lower].pos));
  const toT = sub(target, s);
  const reach = Math.min(len(toT), (l1 + l2) * 0.999);
  const dir = norm(toT);
  // The joint lies on the circle around the root-target line, on the pole's side.
  const a = (l1 * l1 - l2 * l2 + reach * reach) / (2 * reach);
  const h = Math.sqrt(Math.max(l1 * l1 - a * a, 0));
  let side = sub(pole, s);
  side = norm(sub(side, mul(dir, dot(side, dir))));
  const e = add(s, add(mul(dir, a), mul(side, h)));
  rotateWorld(sk, pose, upper, qbetween(sub(w[lower].pos, s), sub(e, s)));
  w = fk(sk, pose);
  const reached = add(s, mul(dir, reach));
  rotateWorld(sk, pose, lower, qbetween(sub(w[end].pos, w[lower].pos), sub(reached, w[lower].pos)));
  return reached;
}

// Arm IK. Puts the wrist at `hand` with the elbow bent towards `elbow` (a model-space point the
// elbow leans to), then turns the hand so its fingers point along `fingers` and its palm faces
// `palm`. Returns how far the wrist ended from the target (0 unless out of reach).
export function armIk(sk, pose, side, { hand, elbow, fingers, palm }) {
  const S = SIDE[side];
  const wrist = `${S}Hand`;
  const s = fk(sk, pose)[`${S}Arm`].pos;
  limbIk(sk, pose, [`${S}Arm`, `${S}ForeArm`, wrist], hand, elbow || add(s, [side === "right" ? 0.3 : -0.3, -0.3, -0.2]));
  if (fingers) {
    const f = handFrame(sk, pose, side);
    rotateWorld(sk, pose, wrist, qbetween(f.fingers, fingers));
    if (palm) {
      // Roll about the fingers until the palm faces as close to `palm` as it can.
      const g = handFrame(sk, pose, side);
      const want = norm(sub(palm, mul(g.fingers, dot(palm, g.fingers))));
      const ang = Math.atan2(dot(cross(g.palm, want), g.fingers), dot(g.palm, want)) * 180 / Math.PI;
      rotateWorld(sk, pose, wrist, qaxis(g.fingers, ang));
    }
  }
  return len(sub(fk(sk, pose)[wrist].pos, hand));
}

// Leg IK. Puts the ankle at `foot` with the knee bent towards `knee` (default: straight ahead) and
// keeps the sole flat as it rests, turned `yaw` degrees outwards. Returns the miss distance.
export function legIk(sk, pose, side, { foot, knee, yaw = 0 }) {
  const S = SIDE[side];
  const ankle = `${S}Foot`;
  const flat = fk(sk, restPose())[ankle].rot;
  const s = fk(sk, pose)[`${S}Leg`].pos;
  limbIk(sk, pose, [`${S}Leg`, `${S}Knee`, ankle], foot, knee || add(s, [0, -0.4, 1]));
  setWorldRot(sk, pose, ankle, qmul(qaxis([0, 1, 0], side === "right" ? yaw : -yaw), flat));
  return len(sub(fk(sk, pose)[ankle].pos, foot));
}

// Put a hand's prop bone (where a held item's origin sits) at a model-space point and rotation.
export function setProp(sk, pose, side, pos, rot = [0, 0, 0, 1]) {
  const S = SIDE[side], w = fk(sk, pose)[`${S}Hand`];
  pose.t[`${S}HandProp`] = qrot(qinv(w.rot), sub(pos, w.pos));
  pose.q[`${S}HandProp`] = qnorm(qmul(qinv(w.rot), rot));
}

// Close the fingers of one hand: `curl` degrees per joint about the knuckle line, thumb half as far.
export function curlFingers(sk, pose, side, curl) {
  const S = SIDE[side];
  const f = handFrame(sk, pose, side);
  // Fingers close towards the palm: a rotation about the across axis.
  const sign = dot(cross(f.across, f.fingers), f.palm) > 0 ? 1 : -1;
  for (const finger of ["Index", "Middle", "Ring", "Pinky"]) {
    for (const j of [2, 3, 4]) {
      const bone = `${S}Hand${finger}${j}`;
      if (sk.by[bone]) rotateWorld(sk, pose, bone, qaxis(f.across, sign * curl));
    }
  }
  for (const j of [2, 3]) {
    const bone = `${S}HandThumb${j}`;
    if (sk.by[bone]) rotateWorld(sk, pose, bone, qaxis(f.fingers, (side === "right" ? 1 : -1) * curl * 0.4));
  }
}

// ---------------------------------------------------------------- pose scripts
// One key pose from a description (tools/assets/<slug>/clips/<clip>.json "keys"):
//   hips:   [dx, dy, dz]            move the hips from rest (metres)
//   turn:   { Bone: [x, y, z] }     turn bones by model-space degrees (x pitch forward, y yaw, z roll)
//   legs:   { left|right: { foot, knee, yaw } }              see legIk (the ankle rests 0.10 m up)
//   arms:   { left|right: { hand, elbow, fingers, palm } }   see armIk
//   curl:   { left|right: degrees }
//   prop:   { left|right: { pos, rot? } } the held item's origin in model space (rot as x y z degrees)
export function buildPose(sk, d) {
  const pose = restPose();
  if (d.hips) pose.t.Hips = add(sk.by.Hips.pos, qrot(qinv(sk.by.Root.quat), d.hips));
  for (const [bone, deg] of Object.entries(d.turn || {})) {
    if (!sk.by[bone]) throw new Error(`pose turns ${bone}, which the skeleton does not have`);
    const q = qmul(qaxis([0, 1, 0], deg[1] || 0), qmul(qaxis([1, 0, 0], deg[0] || 0), qaxis([0, 0, 1], deg[2] || 0)));
    rotateWorld(sk, pose, bone, q);
  }
  const miss = {};
  for (const side of ["left", "right"]) {
    const g = d.legs?.[side];
    if (g) miss[side + "Leg"] = legIk(sk, pose, side, g);
  }
  for (const side of ["left", "right"]) {
    const a = d.arms?.[side];
    if (a) miss[side] = armIk(sk, pose, side, a);
    if (d.curl?.[side]) curlFingers(sk, pose, side, d.curl[side]);
    const p = d.prop?.[side];
    if (p) {
      const r = p.rot || [0, 0, 0];
      setProp(sk, pose, side, p.pos, qmul(qaxis([0, 1, 0], r[1]), qmul(qaxis([1, 0, 0], r[0]), qaxis([0, 0, 1], r[2]))));
    }
  }
  return { pose, miss };
}

const ease = (t) => t * t * (3 - 2 * t);

// Every frame of a clip: key poses are solved, then bone rotations are blended between them.
export function buildClip(sk, clip) {
  const keys = [...clip.keys].sort((a, b) => a.frame - b.frame);
  if (!keys.length || keys[0].frame !== 0) throw new Error("a clip needs a key at frame 0");
  const last = clip.frames;
  if (keys[keys.length - 1].frame !== last) {
    if (!clip.loop) throw new Error(`a clip that does not loop needs a key at its last frame (${last})`);
    keys.push({ ...keys[0], frame: last });
  }
  const solved = keys.map((k) => ({ frame: k.frame, ...buildPose(sk, k) }));
  const frames = [];
  for (let f = 0; f <= last; f++) {
    let i = 0;
    while (i < solved.length - 2 && f >= solved[i + 1].frame) i++;
    const a = solved[i], b = solved[i + 1];
    const t = ease((f - a.frame) / Math.max(b.frame - a.frame, 1));
    const pose = restPose();
    for (const bone of new Set([...Object.keys(a.pose.q), ...Object.keys(b.pose.q)])) {
      pose.q[bone] = qslerp(a.pose.q[bone] || sk.by[bone].quat, b.pose.q[bone] || sk.by[bone].quat, t);
    }
    for (const bone of new Set([...Object.keys(a.pose.t), ...Object.keys(b.pose.t)])) {
      pose.t[bone] = lerp(a.pose.t[bone] || sk.by[bone].pos, b.pose.t[bone] || sk.by[bone].pos, t);
    }
    frames.push(pose);
  }
  return { frames, miss: solved.map((s) => s.miss) };
}

// Which bones a clip writes. "body" is every bone (a full-body clip: the bones it does not pose
// rest, so the legs stand); a list of bone names writes those bones and everything under them (an
// arm-only clip leaves the rest of the body to the locomotion graph).
export function clipBones(sk, roots) {
  if (roots === "body" || !roots) return sk.nodes.filter((n) => n.parent && n.name !== "Root" && sk.by[n.parent]).map((n) => n.name).filter((n) => under(sk, n, "Hips") || n === "Hips");
  const out = [];
  for (const n of sk.nodes) if (roots.some((r) => n.name === r || under(sk, n.name, r))) out.push(n.name);
  return out;
}
function under(sk, name, root) {
  for (let p = sk.by[name]?.parent; p; p = sk.by[p]?.parent) if (p === root) return true;
  return false;
}

// Bones whose position is keyed as well as their rotation.
const MOVES = new Set(["Hips", "LeftHandProp", "RightHandProp"]);
const num = (v) => { const s = (+v.toFixed(8)).toString(); return s === "-0" ? "0" : s; };

// The .txa text of a solved clip, in the layout Workbench exports (nested nodes, one $frame per key,
// a "first last" range for a bone that does not move).
export function writeTxa(sk, clip, built) {
  const bones = new Set(clipBones(sk, clip.bones));
  const last = clip.frames;
  const lines = [`$animation "" {`, ` #version 1.0`, ` #fps ${clip.fps || 30}`, ` #numFrames ${last}`];
  const emit = (name, depth) => {
    const pad = " ".repeat(depth);
    const n = sk.by[name];
    const moves = MOVES.has(name);
    lines.push(`${pad}$node "${name}" {`, `${pad} $keys ${moves ? "t q s" : "q s"} {`);
    const key = (f) => {
      const p = built.frames[f];
      const t = moves ? (p.t[name] || n.pos) : [0, 0, 0];
      return [t.map(num).join(" "), (p.q[name] || n.quat).map(num).join(" ")];
    };
    const first = key(0);
    let still = true;
    for (let f = 1; f <= last && still; f++) { const k = key(f); still = k[0] === first[0] && k[1] === first[1]; }
    const put = (label, k) => lines.push(`${pad}  $frame ${label} {`, `${pad}   #t ${k[0]}`, `${pad}   #q ${k[1]}`, `${pad}  }`);
    if (still) put(`0 ${last}`, first);
    else for (let f = 0; f <= last; f++) put(String(f), key(f));
    lines.push(`${pad} }`);
    for (const c of sk.children[name] || []) if (bones.has(c)) emit(c, depth + 1);
    lines.push(`${pad}}`);
  };
  for (const n of sk.nodes) if (bones.has(n.name) && !bones.has(n.parent)) emit(n.name, 1);
  lines.push("}");
  return lines.join("\n") + "\n";
}

// Parse a .txa back to { fps, frames, keys: { bone: [{ t, q } per frame] } } (the selftest round trip).
export function parseTxa(text) {
  const out = { fps: 30, frames: 0, keys: {} };
  const stack = [];
  let range = null;
  for (const line of text.split(/\r?\n/)) {
    let m;
    if ((m = /#fps (\d+)/.exec(line))) out.fps = Number(m[1]);
    else if ((m = /#numFrames (\d+)/.exec(line))) out.frames = Number(m[1]);
    else if ((m = /\$node "([^"]+)"/.exec(line))) { stack.push(m[1]); out.keys[m[1]] = []; }
    else if ((m = /\$frame (\d+)(?: (\d+))?/.exec(line))) range = [Number(m[1]), Number(m[2] ?? m[1])];
    else if ((m = /#([tq]) (.+)/.exec(line))) {
      const vals = m[2].trim().split(/\s+/).map(Number);
      for (let f = range[0]; f <= range[1]; f++) (out.keys[stack[stack.length - 1]][f] ||= {})[m[1]] = vals;
    } else if (/^\s*\}\s*$/.test(line) && range === null) stack.pop();
    else if (/^\s*\}\s*$/.test(line)) range = undefined; // closes a $frame; the $keys and $node closers follow
    if (range === undefined) range = false;
    else if (range === false && /^\s*\}\s*$/.test(line)) range = null; // closes $keys; the next closer pops the node
  }
  return out;
}
