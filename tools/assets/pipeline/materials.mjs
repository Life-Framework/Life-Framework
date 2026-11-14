// Material classes for the clean stage (docs/asset-pipeline/README.md "Materials").
// Meshy's PBR maps read as polished chrome in Enfusion's daylight whatever the object is. The spec
// declares what each region is made of; the clean stage remaps Meshy's roughness and metallic into
// that class's range (blender/materials.py does the pixels, this file decides the plan).
//
// spec.materials = {
//   default: "plastic",                                   // class of everything no region claims
//   regions: [ { class: "metal", metallic_above: 0.5 },   // where Meshy's own metallic map is high
//              { class: "steel", above_height_m: 0.0505 } // faces whose centre is above this height
//   ],
//   classes: { plastic: { base_scale: 0.9 } }             // optional overrides, merged over the defaults
// }
// A later region wins over an earlier one. No block at all means plastic, with Meshy's metal kept
// as metal: a future asset is matte by default and needs no hand-written tone-map.
//
// A class: rough [lo, hi] and metal [lo, hi] map the Meshy value 0..1 linearly onto lo..hi
// (clipped), so [0.8, 0.8] pins it. Colour: base_scale multiplies the base colour (hue kept);
// base_lum [lo, hi] replaces it with grey, lo + (hi - lo) * the original luminance (scuffs kept).
export const CLASSES = {
  plastic: { rough: [0.75, 0.85], metal: [0, 0.03], base_scale: 1 },
  rubber: { rough: [0.88, 0.95], metal: [0, 0], base_scale: 0.6 },
  metal: { rough: [0.3, 0.5], metal: [0.8, 0.95], base_scale: 0.9 },
  fabric: { rough: [0.85, 1], metal: [0, 0], base_scale: 1 },
  // A worked metal surface: dull, not a mirror. Ingots, tools, castings.
  brushed_metal: { rough: [0.4, 0.65], metal: [0.85, 0.95], base_scale: 1 },
  // Rock, ore and anything mineral: rough and never metal.
  stone: { rough: [0.78, 0.95], metal: [0, 0], base_scale: 1 },
};
const KEYS = ["rough", "metal", "base_scale", "base_lum"];

export const DEFAULT_MATERIALS = { default: "plastic", regions: [{ class: "metal", metallic_above: 0.5 }] };

export function resolveMaterials(spec) {
  const m = spec.materials || DEFAULT_MATERIALS;
  const classes = {};
  for (const name of new Set([...Object.keys(CLASSES), ...Object.keys(m.classes || {})])) {
    classes[name] = { ...(CLASSES[name] || {}), ...((m.classes || {})[name] || {}) };
  }
  return { default: m.default || "plastic", regions: m.regions || [], classes };
}

// The plan of each spec variant (docs/asset-pipeline/README.md "Variants"): its own materials block,
// or the spec's when it has none.
export function variantPlans(spec) {
  return (spec.variants || []).map((v) => ({ name: v.name, plan: resolveMaterials({ materials: v.materials || spec.materials }) }));
}

export function materialProblems(spec) {
  const out = [];
  for (const v of spec.variants || []) {
    if (v.materials) out.push(...materialProblems({ materials: v.materials }).map((p) => `variant ${v.name}: ${p}`));
  }
  if (spec.materials === undefined) return out;
  const m = spec.materials;
  const r = resolveMaterials(spec);
  const range = (label, v) => {
    if (!Array.isArray(v) || v.length !== 2 || !v.every((x) => typeof x === "number" && x >= 0 && x <= 1.5)) out.push(`materials: ${label} must be [lo, hi]`);
    else if (v[0] > v[1]) out.push(`materials: ${label} has lo above hi`);
  };
  for (const [name, c] of Object.entries(r.classes)) {
    for (const k of Object.keys(c)) if (!KEYS.includes(k)) out.push(`materials: class ${name} has unknown key ${k}`);
    if (!c.rough) out.push(`materials: class ${name} needs rough`);
    else range(`${name}.rough`, c.rough);
    if (!c.metal) out.push(`materials: class ${name} needs metal`);
    else range(`${name}.metal`, c.metal);
    if (c.base_lum !== undefined) range(`${name}.base_lum`, c.base_lum);
    if (c.base_scale !== undefined && !(c.base_scale > 0 && c.base_scale <= 2)) out.push(`materials: ${name}.base_scale must be 0 to 2`);
  }
  if (!r.classes[r.default]) out.push(`materials: default class ${r.default} is not defined`);
  for (const [i, g] of (m.regions || []).entries()) {
    if (!r.classes[g.class]) out.push(`materials: region ${i} names class ${g.class}, which is not defined`);
    const sel = ["metallic_above", "above_height_m"].filter((k) => g[k] !== undefined);
    if (sel.length !== 1) out.push(`materials: region ${i} needs exactly one of metallic_above, above_height_m`);
  }
  return out;
}
