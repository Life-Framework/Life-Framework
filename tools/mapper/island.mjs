// tools/mapper/island.mjs: the island slot writer of `tools\cli mapper import --slot Place_<place>_<slot>.layer` (#3851).
//
// `place` reaches only the playtest town; an island slot (`MainWorld_Layers/Place_<place>_<slot>.layer`) is written by
// `town fit`, from a kit. This is the same job for loose props: a Mapper layout's props, seated on the island's terrain, appended
// to a slot that `town fit` already made, through the same guards:
//   - a `ws/town-draft-<place>-<slot>[-<word>]` branch (the zone guard, validate-island-zones);
//   - the slot has its claim (tools/town/claims/<place>-<slot>.json) and its layer;
//   - every prop stands inside the slot's ground (its plot or building footprint) plus the slot's margin, checked before the
//     probe and again by `checkClaimsAndFootprints` after the write (the check validate-island-zones runs);
//   - the heights come from ONE test-server boot on the island that probes the terrain under every prop's footprint
//     (`probeTerrain`, the probe `town fit --terrain` uses), never a typed coordinate;
//   - overlap with the slot's own entities and with the batch is refused offline (`offlineProblems`);
//   - then `audit placement --strict --world MainWorld --layer <layer>` judges the layer; a flagged layer is restored.
// Nothing is typed: the layout's centre goes to `--at`, or to the slot's ground centre (the plot's, from the dossier).
// A later `town fit <place> <slot> --refit` rewrites the layer from the kit and drops these props: re-run the import after it.
//
// Pure helpers (claimOfLayer, branchOwnsSlot, footprintProblems, seatOnTerrain) are what tools/validation/mapper-selftest.mjs
// runs; `intoIslandSlot` takes its collaborators (the place tool `P`, the probe, the audit, the place data) from the context.
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CLAIMS_DIR, checkClaimsAndFootprints, distToPolygon, loadPlace, marginOf, slotGround } from "../town/check.mjs";
import { pinKey, probeTerrain } from "../town/terrain.mjs";
import { islandLayersDir } from "../lib/worlds.mjs";
import { entityName } from "./convert.mjs";
import { frameOf, gameVfsSync, layerY } from "../lib/height-frame.mjs";

const r2 = (n) => Math.round(n * 100) / 100;
const r3 = (n) => Math.round(n * 1000) / 1000;
const ident = (s) => String(s).replace(/\W/g, "_");
const REF = /^\{([0-9A-Fa-f]{16})\}(.+)$/;

// The claim whose layer is `layerName` ({ place, slot, ... }): a place and a slot can both hold a '-', so the layer name alone
// cannot be split; the claim says which is which. null when the layer has no claim (it was never fitted).
export function claimOfLayer(root, layerName) {
  const dir = join(root, CLAIMS_DIR);
  if (!existsSync(dir)) return null;
  for (const f of readdirSync(dir).filter((n) => n.endsWith(".json")).sort()) {
    try {
      const c = JSON.parse(readFileSync(join(dir, f), "utf8"));
      if (c && c.place && c.slot && String(c.layer ?? "").replace(/\\/g, "/").endsWith(`/${layerName}`)) return c;
    } catch {
      // an unreadable claim is validate-island-zones's to report
    }
  }
  return null;
}

// The zone guard's rule for this slot: ws/town-draft-<place>-<slot>, or a follow-up with a '-<word>' tail.
export const branchOwnsSlot = (branch, place, slot) => new RegExp(`^ws/town-draft-${place}-${slot}(-[a-z0-9]+)*$`).test(String(branch ?? ""));

// The y of the first `coords` line of a layer (the slot's marker): where the terrain probe rays are centred.
export function markerY(text) {
  const m = /^\s*coords\s+\S+\s+(-?\d+(?:\.\d+)?)\s+\S+/m.exec(String(text ?? ""));
  return m ? Number(m[1]) : null;
}

// The props whose footprint corners stand beyond the margin of the slot's ground. `plan`: [{ id, x, z, yaw, item: {bounds, scale} }]
// with x/z the footprint centre; `P` is the place tool (footprintCorners, groundProbePoints).
export function footprintProblems(P, plan, ground, margin) {
  const out = [];
  for (const e of plan) {
    const { pivot } = P.groundProbePoints(e.item, [e.x, e.z], e.yaw);
    const far = Math.max(...P.footprintCorners(e.item.bounds, e.item.scale, e.yaw).map((c) => distToPolygon(pivot[0] + c[0], pivot[1] + c[1], ground.poly)));
    if (far > margin) out.push({ e, why: `its footprint reaches ${r2(far)} m outside ${ground.what}, over the ${margin} m margin of the slot` });
  }
  return out;
}

// One prop's pivot and height from probed terrain: the same probe points as `place ground`, the heights from `pins`
// ({ "x,z": y }). Throws a PlaceError-like Error for ground that is missing or not level (the place tool's own rule).
export function seatOnTerrain(P, e, pins) {
  const { pivot, pts } = P.groundProbePoints(e.item, [e.x, e.z], e.yaw);
  const samples = pts.map((p) => ({ y: pins[pinKey(p[0], p[1])] }));
  if (samples.some((s) => !Number.isFinite(s.y))) throw new Error("the island's probe gave no terrain height under it");
  const res = P.onGround({ samples, prop: { name: e.name, prefab: e.item.prefab, bounds: e.item.bounds, scale: e.item.scale } });
  return { x: r3(pivot[0]), y: r3(res.y), z: r3(pivot[1]), notes: res.notes, ty: pins[pinKey(pivot[0], pivot[1])] };
}

// The y the layer must hold for a seat (a world height): the prefab's own frame (tools/lib/height-frame.mjs). A terrain-frame
// prefab with no probed ground under its pivot throws.
export function layerYOf(seat, prefabRef, { root, frameOfRef = null } = {}) {
  const frame = (frameOfRef ?? ((ref) => frameOf(root, gameVfsSync(), ref)))(prefabRef);
  if (frame !== "terrain") return seat.y;
  if (!Number.isFinite(seat.ty)) throw new Error("its prefab reads y over the terrain (RELATIVE_Y) and the probe gave no ground height under its pivot");
  return r3(layerY(seat.y, seat.ty, frame));
}

// ------------------------------------------------------------------ the writer

// kept: the importer's props { id, prefab: {path}, ref, x, z, yaw } already translated onto the target. Returns an exit code.
// translate(kept, at) moves the layout's centre onto `at` (convert.mjs); `--at` is optional here: the slot's ground centre is the default.
export async function intoIslandSlot({ root, P, layer, kept, flags, ctx, branch, translate }) {
  const claim = claimOfLayer(root, layer);
  if (!claim) {
    console.log(`mapper: ${layer} has no claim in ${CLAIMS_DIR}/: an island slot is fitted first (tools\\cli town fit <place> <slot>); the import adds props to a slot that exists`);
    return 1;
  }
  const { place, slot } = claim;
  if (!branchOwnsSlot(branch, place, slot)) {
    console.log(`mapper: ${layer} is the island slot ${place}/${slot}: only a worktree named town-draft-${place}-${slot}[-<word>] (branch ws/town-draft-${place}-${slot}...) may change it, this branch is ${branch || "unknown"} (tools\\cli wt new town-draft-${place}-${slot}-mapper; docs/starter-packs/town.md)`);
    return 1;
  }
  let data;
  try {
    data = (ctx.placeData ?? loadPlace)(root, place);
  } catch (e) {
    console.log(`mapper: cannot read the place ${place}: ${e.message}`);
    return 1;
  }
  const entry = data.mapping?.slots?.[slot];
  const ground = entry ? slotGround(entry, data.dossier, data.others) : null;
  if (!ground) {
    console.log(`mapper: ${place}/${slot} has no ground in its dossier (the building or plot the mapping names): nothing to place inside`);
    return 1;
  }
  const margin = marginOf((data.template?.slots ?? []).find((s) => s.id === slot) ?? { id: slot }, entry);
  let at = null;
  if (flags.at != null) {
    const m = /^(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)$/.exec(String(flags.at));
    if (!m) {
      console.log("mapper: --at is not x,z (like 9527.5,2972)");
      return 1;
    }
    at = [Number(m[1]), Number(m[2])];
  } else at = ground.centre;
  const layerText = data.layers?.[layer] ?? readFileSync(join(islandLayersDir(root), layer), "utf8");
  const prefix = `Mapper_${ident(place)}_${ident(slot)}`;
  if (new RegExp(`\\b${prefix}_M\\d+\\b`).test(layerText)) {
    console.log(`mapper: ${layer} already holds an import (${prefix}_M...); remove those entities first (or tools\\cli town fit ${place} ${slot} --refit, which rewrites the layer from the kit)`);
    return 1;
  }
  const plan = translate(kept, at).map((e, i) => {
    const info = P.prefabInfo(P.ROOT ?? root, e.prefab.path);
    return { ...e, name: entityName(prefix, i + 1), item: { prefab: e.prefab.path, bounds: info.bounds, scale: info.scale, cls: info.cls, dynamic: info.dynamic } };
  });
  console.log(`mapper: island slot ${place}/${slot}, ${ground.what}, margin ${margin} m; layout centre ${at.map(r2).join(",")}${flags.at == null ? " (the slot's ground centre)" : ""}`);
  const outside = footprintProblems(P, plan, ground, margin);
  for (const o of outside) console.log(`mapper: ${o.e.id} (${o.e.name}): ${o.why}`);
  if (flags.dry) {
    plan.forEach((e) => console.log(`  ${e.name}  ${e.prefab.path.split("/").pop()}  ${r2(e.x)},${r2(e.z)}  yaw ${e.yaw}  (${e.id})${outside.some((o) => o.e === e) ? "  OUTSIDE" : ""}`));
    console.log("mapper: dry run, nothing written.");
    return outside.length ? 1 : 0;
  }
  if (!ctx.placeData && (await import("../wt.mjs")).isMainCheckout(root)) {
    console.log("mapper: REFUSE: this is the main checkout. Create a worktree first: tools\\cli wt new town-draft-<place>-<slot>-mapper");
    return 1;
  }
  const refused = outside.map((o) => ({ e: o.e, why: o.why }));
  const todo = plan.filter((e) => !outside.some((o) => o.e === e));
  if (!todo.length) {
    console.log("mapper: nothing left inside the slot's ground");
    return 1;
  }
  // 1. ONE boot on the island for every footprint's terrain heights.
  // The pivot too: a prefab whose flags carry RELATIVE_Y reads its y over the terrain under its pivot (tools/lib/height-frame.mjs).
  const points = todo.flatMap((e) => {
    const g = P.groundProbePoints(e.item, [e.x, e.z], e.yaw);
    return [...g.pts, g.pivot];
  });
  const aroundY = markerY(layerText) ?? 0;
  console.log(`  probing the island's terrain under ${todo.length} prop(s) (${points.length} points, one test-server boot)`);
  let pins;
  try {
    const probed = await (ctx.probe ?? ((pts, o) => probeTerrain(pts, o)))(points, { layerName: layer, aroundY });
    if (probed.missing?.length) console.log(`mapper: note: no terrain answer for ${probed.missing.length} point(s)`);
    pins = probed.points;
  } catch (e) {
    console.log(`mapper: the terrain probe failed: ${e.message}`);
    return 1;
  }
  // 2. seat each prop, refuse a prop that is not on level ground or overlaps the slot's entities or the batch.
  const staged = [];
  const existing = (ctx.modelOf ?? P.modelOf)(root, layerText, layer);
  let text = layerText;
  for (const e of todo) {
    let seat;
    try {
      seat = seatOnTerrain(P, e, pins);
    } catch (err) {
      refused.push({ e, why: err.message });
      continue;
    }
    const cand = { name: e.name, prefab: e.prefab.path, x: seat.x, y: seat.y, z: seat.z, yaw: e.yaw, scale: e.item.scale, bounds: e.item.bounds, dynamic: e.item.dynamic };
    const found = P.offlineProblems(cand, { ents: [...existing, ...staged] });
    if (found.length) {
      refused.push({ e, why: found.join("; ") });
      continue;
    }
    staged.push({ ...cand, file: layer, tilted: false, mesh: null, parent: null, cls: e.item.cls });
    const m = REF.exec(e.ref);
    if (!m) {
      refused.push({ e, why: `the prefab reference ${e.ref} is not {GUID}path` });
      staged.pop();
      continue;
    }
    let ly;
    try {
      ly = layerYOf(seat, e.ref, { root, frameOfRef: ctx.frameOf ?? null });
    } catch (err) {
      refused.push({ e, why: err.message });
      staged.pop();
      continue;
    }
    text = P.appendEntity(text, { cls: e.item.cls, name: e.name, guid: m[1], path: m[2], coords: [seat.x, ly, seat.z], yaw: e.yaw, scale: null });
    console.log(`  ${e.name}  ${e.prefab.path.split("/").pop()}  ${seat.x},${seat.z}  y ${seat.y}  yaw ${e.yaw}  (${seat.notes[0]})`);
  }
  const placed = staged.map((s) => s.name);
  for (const r of refused) console.log(`  refused ${r.e.id} (${r.e.prefab.path}) at ${r2(r.e.x)},${r2(r.e.z)}: ${r.why}`);
  if (!placed.length) {
    console.log(`mapper: nothing was placed in ${layer}; ${refused.length} refused`);
    return 1;
  }
  const file = join(islandLayersDir(root), layer);
  const original = layerText;
  writeFileSync(file, text);
  const rollback = (why) => {
    writeFileSync(file, original);
    console.log(`mapper: ${why}; ${layer} restored`);
  };
  // 3. the check validate-island-zones will run on the written layer.
  data.layers = { ...(data.layers ?? {}), [layer]: text };
  const zone = checkClaimsAndFootprints(data);
  if (zone.errors.length) {
    for (const er of zone.errors) console.log(`mapper: ${er}`);
    rollback("the zone check failed");
    return 1;
  }
  let code = refused.length ? 1 : 0;
  // 4. the strict placement audit of the layer, on the island.
  if (!flags["no-verify"]) {
    console.log(`  auditing the slot (audit placement --strict --world MainWorld --layer ${layer})`);
    const audit = ctx.auditMain ?? (async (argv) => (await import("../../agent-box/place-audit.mjs")).main(argv));
    const a = await audit(["placement", "--strict", "--world", "MainWorld", "--layer", layer, "--hard-layer", layer]);
    if (a !== 0) {
      if (!flags.keep) {
        rollback("the audit flagged the layer");
        return a;
      }
      code = a;
    }
  }
  console.log(`mapper: ${placed.length} placed in ${layer} (${place}/${slot}), ${refused.length} refused by the checks`);
  console.log(code === 0 ? `mapper: done. tools\\cli look place:${place}/${slot} for the picture.` : "mapper: the slot has findings above");
  return code;
}
