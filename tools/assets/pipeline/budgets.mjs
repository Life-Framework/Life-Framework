// Triangle budgets per asset kind, set next to vanilla Reforger items.
//
// Vanilla LOD0 triangle counts, read from the compiled .xob (LZO4 LOD blocks in the HEAD chunk; the
// first u16 of each part record is its triangle count) on 2026-11-14, game 1.9:
//
//   handguns   StarterPistol 4,132   PM 7,551   M9 17,515
//   gadgets    Watch_Vostok 3,686    Compass_SY183 7,046   Flashlight_MX991 7,440   Binoculars_B8 8,681
//   grenade    M67 8,114
//   kits       MedicalKit_01 2,250   (Gauze_01_packed, the bare held gadget the cuffs sit on: 210)
//   small props AlarmClock 1,212   Chock_01 1,192   Bucket_01 1,540
//
// A vanilla item keeps 53 to 73 % of LOD0 in LOD1, about 21 % in LOD2 and 7 % in LOD3. Every vanilla
// model has the same LOD factors in the .xob (0.5, 0.15, 0.02, 0.005 ...) and so does a model this
// pipeline imports: the importer writes the factors, we set no distance. The engine switches on the
// sphere size and the triangle counts, so the lever is the triangle count of each LOD.
//
// `lod0` is the triangle count of LOD0 (for a deployable: of the whole thing, shared by its links),
// `min` the floor below which the model looks faceted next to vanilla, `max` the cap. `source` is
// the Meshy target_polycount to ask for: at least `lod0 * sourceFactor`, so the clean stage has
// geometry to keep. `ratios` are the LOD1.. sizes as a share of LOD0.
export const BUDGETS = {
  weapon: { lod0: 12000, min: 6000, max: 20000, ratios: [0.5, 0.25, 0.125], source: 20000 },
  gadget: { lod0: 8000, min: 4000, max: 12000, ratios: [0.5, 0.25, 0.125], source: 30000 },
  deployable: { lod0: 14000, min: 6000, max: 20000, ratios: [0.5, 0.25, 0.125], source: 40000 },
};

// Where LOD0 comes from: the asset pipeline's remesh stage asks Meshy for exactly this count (5
// credits), so the Blender clean stage does not collapse a big raw mesh (it smears UVs and dents flat
// panels). The LODs below LOD0 are gentle Blender steps of at least half the previous LOD.
//
// Share of the generated mesh the clean stage may throw away before the model needs a baked normal
// map to hold its shape. The bake is always on (clean.py), so this is only the warning line.
export const MAX_DECIMATION = 0.35; // LOD0 should keep at least this share of the source triangles unless baked

export function budgetFor(kindName) {
  const b = BUDGETS[kindName];
  if (!b) throw new Error(`no triangle budget for kind ${kindName}`);
  return b;
}

// Problems with a spec's budget, as sentences an agent can act on. `sourceTris` (the raw Meshy
// triangle count) is optional: the generate stage knows it, a spec alone does not.
export function checkBudget(spec, sourceTris) {
  const out = [];
  const b = BUDGETS[spec.kind];
  // A model our own Blender script builds (source.blender) has exactly the triangles it needs:
  // nothing is collapsed, so the floor and the step rules do not apply.
  if (!b || spec.source?.blender) return out;
  const s = spec.budget || {};
  if (s.lod0_tris < b.min) out.push(`budget.lod0_tris ${s.lod0_tris} is under the ${spec.kind} floor of ${b.min} (vanilla items of this kind are at ${b.lod0}); it will look faceted`);
  if (s.lod0_tris > b.max) out.push(`budget.lod0_tris ${s.lod0_tris} is over the ${spec.kind} cap of ${b.max}`);
  const r = s.lod_ratios || [];
  if (r.length < 2) out.push("budget.lod_ratios needs at least two entries (LOD1 and LOD2)");
  r.forEach((v, i) => {
    if (!(v > 0 && v < 1)) out.push(`budget.lod_ratios[${i}] must be between 0 and 1`);
    if (i > 0 && v >= r[i - 1]) out.push(`budget.lod_ratios must shrink, but [${i}] ${v} is not under [${i - 1}] ${r[i - 1]}`);
  });
  r.forEach((v, i) => {
    if (i > 0 && v / r[i - 1] < 0.5) out.push(`budget.lod_ratios[${i}] ${v} drops under half of the LOD before it (${r[i - 1]}); a step that big collapses UVs. Ask for a remesh or keep each step at 0.5 or more`);
  });
  if (r[0] > 0.75) out.push(`budget.lod_ratios[0] ${r[0]} keeps almost all of LOD0 in LOD1; vanilla keeps 0.5 to 0.7`);
  if (r.length && r[r.length - 1] < 0.03) out.push(`the last budget.lod_ratios entry ${r[r.length - 1]} is under 0.03; a thin part will vanish (vanilla lowest LODs are under 100 triangles, but only far away)`);
  if (sourceTris && s.lod0_tris < sourceTris * MAX_DECIMATION && !(spec.meshy && spec.meshy.target_polycount)) {
    out.push(`LOD0 keeps ${(100 * s.lod0_tris / sourceTris).toFixed(0)} % of the source's ${sourceTris} triangles`);
  }
  return out;
}

// The default budget block a new spec starts with.
export function defaultBudget(kindName) {
  const b = budgetFor(kindName);
  return { lod0_tris: b.lod0, lod_ratios: [...b.ratios], texture_px: 1024, credits: 300 };
}
