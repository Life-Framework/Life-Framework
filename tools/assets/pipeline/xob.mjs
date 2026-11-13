// Reads the head of a compiled Enfusion model (.xob, XOB9): bounds, materials, and the node table
// (bones and sockets with their rest positions). The layout was worked out from vanilla files on
// 2026-11-12 (game 1.9); parse() throws on anything it does not recognise rather than guessing.
//
// FORM <size> XOB9 HEAD <size BE> <u32> min xyz, max xyz (f32 LE) ... at +40: u16 materials,
// u16 nodes, u16 bones, u16, u32, u32 stringBytes; then the string table, then per material
// (u16 name, u16 path), then per node (i32 name, pos xyz, quat xyzw, i16 next sibling, i16 first child).
export function parse(buf) {
  if (buf.toString("latin1", 0, 4) !== "FORM" || buf.toString("latin1", 8, 16) !== "XOB9HEAD") {
    throw new Error(`not an XOB9 model (starts ${JSON.stringify(buf.toString("latin1", 0, 16))})`);
  }
  const head = 20;
  const f3 = (o) => [buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)];
  const bounds = { min: f3(head + 4), max: f3(head + 16) };
  const nMat = buf.readUInt16LE(head + 44), nNode = buf.readUInt16LE(head + 46), nBone = buf.readUInt16LE(head + 48);
  const strBytes = buf.readUInt32LE(head + 56);
  const strStart = head + 60;
  const strings = buf.toString("latin1", strStart, strStart + strBytes).split("\0").slice(0, -1);
  let o = strStart + strBytes;
  const materials = [];
  for (let i = 0; i < nMat; i++, o += 4) materials.push({ name: strings[buf.readUInt16LE(o)], path: strings[buf.readUInt16LE(o + 2)] });
  const nodes = [];
  for (let i = 0; i < nNode; i++, o += 36) {
    const name = strings[buf.readInt32LE(o)];
    if (name === undefined) throw new Error(`node ${i} names string ${buf.readInt32LE(o)} of ${strings.length}`);
    nodes.push({ name, pos: f3(o + 4), quat: [...f3(o + 16), buf.readFloatLE(o + 28)], sibling: buf.readInt16LE(o + 32), child: buf.readInt16LE(o + 34), parent: null });
  }
  for (const n of nodes) {
    for (let c = n.child; c >= 0 && c < nodes.length; c = nodes[c].sibling) nodes[c].parent = n.name;
  }
  const colliders = strings.filter((s) => /^(UBX|UCX|USP|UCS|UCL|UTM)_/.test(s));
  return { bounds, materials, nodes, boneCount: nBone, colliders, strings };
}

// Model-space position of every node (rest pose, translations only; vanilla weapon rests carry no rotation).
export function worldPositions(model) {
  const by = Object.fromEntries(model.nodes.map((n) => [n.name, n]));
  const out = {};
  for (const n of model.nodes) {
    let p = [...n.pos];
    for (let a = by[n.parent]; a; a = by[a.parent]) p = p.map((v, i) => v + a.pos[i]);
    out[n.name] = p;
  }
  return out;
}

// What a compiled model is missing against its kind and the clean report. Empty means it is usable.
export function checkModel(model, kind, { sockets = [], emat, colliderName } = {}) {
  const out = [];
  const pos = worldPositions(model);
  for (const name of [...(kind.requiredBones || []), ...(kind.requiredSockets || [])]) {
    if (!pos[name]) out.push(`the .xob has no node ${name}; the prefab chain names it`);
  }
  const ref = Object.fromEntries((kind.skeleton || []).filter((n) => n.bone).map((n) => [n.name, n]));
  for (const n of model.nodes) {
    if (ref[n.name] && n.pos.some((v, i) => Math.abs(v - ref[n.name].pos[i]) > 5e-4)) {
      out.push(`bone ${n.name} rests at ${n.pos.map((v) => v.toFixed(4))}, the vanilla skeleton has ${ref[n.name].pos}; vanilla clips would move it`);
    }
    if (ref[n.name] && (Math.abs(Math.abs(n.quat[3]) - 1) > 1e-3)) out.push(`bone ${n.name} has a rest rotation ${n.quat.map((v) => v.toFixed(3))}; vanilla bones have none`);
    if (ref[n.name] && (ref[n.name].parent || null) !== (n.parent === "Scene_Root" || n.parent === "Armature" ? null : n.parent)) {
      out.push(`bone ${n.name} hangs under ${n.parent}, not ${ref[n.name].parent}`);
    }
  }
  for (const s of sockets) {
    if (pos[s.name] && pos[s.name].some((v, i) => Math.abs(v - s.pos[i]) > 1e-3)) {
      out.push(`socket ${s.name} came out at ${pos[s.name].map((v) => v.toFixed(4))}, the clean stage put it at ${s.pos} (axis or parenting error in the export)`);
    }
  }
  if (emat && !model.materials.some((m) => (m.path || "").endsWith(emat))) {
    out.push(`the .xob bakes ${model.materials.map((m) => m.path).join(", ") || "no material"}, not ${emat}: it would render white or default`);
  }
  if (colliderName && !model.colliders.includes(colliderName)) out.push(`the .xob has no collider ${colliderName} (Workbench logs "Invalid collider" when it drops one)`);
  return out;
}
