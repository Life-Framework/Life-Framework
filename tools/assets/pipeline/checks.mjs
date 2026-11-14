// Deterministic checks on the clean stage's report.json. Each returns a sentence an agent can act on.
export function checkReport(spec, kind, report) {
  const out = [];
  const lod0 = report.lods?.[0];
  if (!lod0) return ["the clean report lists no LOD"];
  if (lod0.tris > spec.budget.lod0_tris * 1.02) out.push(`LOD0 has ${lod0.tris} triangles, over the budget of ${spec.budget.lod0_tris}`);
  report.lods.forEach((l, i) => {
    if (l.name !== `${spec.name}_LOD${i}`) out.push(`LOD ${i} is named ${l.name}; Workbench needs ${spec.name}_LOD${i}`);
    if (i > 0 && l.tris >= report.lods[i - 1].tris) out.push(`${l.name} is not smaller than LOD${i - 1}`);
  });
  const [length, height, width] = report.size;
  const want = spec.size_m;
  // The clean stage scales a pistol on its height (the grip) and every other kind on its length, so
  // that axis is tight; the other two say whether the proportions fit the spec.
  const tols = kind.fit === "pistol" ? { length: 0.25, height: 0.05, width: 0.4 } : { length: 0.05, height: 0.4, width: 0.4 };
  for (const [label, got, tol] of [["length", length, tols.length], ["height", height, tols.height], ["width", width, tols.width]]) {
    if (Math.abs(got - want[label]) > want[label] * tol) out.push(`${label} is ${got.toFixed(3)} m against the spec's ${want[label]} m (tolerance ${tol * 100}%): regenerate, or correct size_m if the spec was wrong`);
  }
  if (report.uv.min < -0.01 || report.uv.max > 1.01) out.push(`UVs leave the 0..1 square (${report.uv.min.toFixed(2)}..${report.uv.max.toFixed(2)}); the atlas would tile`);
  if (!report.textures.found.includes("base_color")) out.push("no base colour map");
  if (report.bake && !report.bake.ok) out.push(`the high-to-low normal bake left ${(100 * report.bake.empty_share).toFixed(0)} % of the map empty; the generator's own normal map was kept, which is flat on most generated models`);
  if (report.collider.name !== kind.collider.prefix + spec.name) out.push(`collider is named ${report.collider.name}`);
  if (report.collider.usage !== kind.collider.layerPreset) out.push(`collider usage is ${report.collider.usage}, not ${kind.collider.layerPreset}`);
  if (report.collider.faces < 4) out.push("collider has no volume");
  out.push(...checkSockets(kind, report));
  for (const p of report.parts || []) if (p.faces === 0) out.push(`part ${p.name}: no face inside its box, so ${p.bone} moves nothing`);
  return out;
}

// Sockets: every name the prefab chain uses exists, and each sits where a weapon needs it.
export function checkSockets(kind, report) {
  const out = [];
  const by = Object.fromEntries((report.sockets || []).map((s) => [s.name, s.pos]));
  for (const name of kind.requiredSockets || []) if (!by[name]) out.push(`socket ${name} is missing`);
  const bones = new Set((report.bones || []).map((b) => b.name));
  for (const name of kind.requiredBones || []) if (!bones.has(name)) out.push(`bone ${name} is missing`);
  const ref = Object.fromEntries((kind.skeleton || []).filter((n) => n.bone).map((n) => [n.name, n]));
  for (const b of report.bones || []) {
    // Bone rest positions are the vanilla skeleton's; moving one breaks every vanilla clip.
    const n = ref[b.name];
    if (!n) continue;
    let p = n.pos, parent = n.parent;
    while (parent) { p = p.map((v, i) => v + ref[parent].pos[i]); parent = ref[parent].parent; }
    if (p.some((v, i) => Math.abs(v - b.pos[i]) > 1e-4)) out.push(`bone ${b.name} is at ${b.pos} but the vanilla skeleton has it at ${p.map((v) => +v.toFixed(5))}`);
  }
  const { min, max } = report.bounds;
  const m = by.barrel_muzzle;
  if (m) {
    if (Math.abs(m[2] - max[2]) > 0.01) out.push(`barrel_muzzle is ${(Math.abs(m[2] - max[2]) * 100).toFixed(1)} cm from the front face; the flash and the projectile start there`);
    if (m[1] < min[1] || m[1] > max[1] || Math.abs(m[0]) > 0.01) out.push("barrel_muzzle is outside the mesh's height or off the centre line");
    if (by.eye && by.eye[2] >= m[2]) out.push("eye is in front of the muzzle");
    if (by.barrel_chamber && by.barrel_chamber[2] >= m[2]) out.push("barrel_chamber is in front of the muzzle");
  }
  if (by.eye && by.eye[1] < (min[1] + max[1]) / 2) out.push("eye is below the middle of the weapon; aiming would look through the body");
  if (by.slot_magazine && by.slot_magazine[1] > (min[1] + max[1]) / 2) out.push("slot_magazine is above the middle of the weapon");
  return out;
}
