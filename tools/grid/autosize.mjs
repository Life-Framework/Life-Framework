// tools/grid/autosize.mjs: size a prefab's block of grid cells from its real bounds (`tools\cli grid place <slug> <prefab>`).
//
// Pure maths on a bounds box (the placement tool's own reader, `prefabInfo` in tools/place/place.mjs, gives it:
// the .xob header of the prefab's mesh, offline); no I/O. grid.mjs reads the bounds, calls these, claims the
// block and hands the centre to `place ground`.
//
//   footprintOf    size, height and the pivot's offset from the footprint centre, at a yaw
//   spanFor        ceil((extent + margin) / CELL.size): the cells one axis needs
//   spanOptions    the block sizes the two orientations (0/180 and 90/270 yaw) need, smallest first
//   chooseYaw      the yaw (of 0/90/180/270) whose front faces the block's open gap and whose footprint fits it
//   largestFree    the biggest free block, for the "nothing fits" message
//
// `P` is tools/place/place.mjs's maths (footprintRect, rot), passed in so this file stays free of the tool's
// imports, as yardLayer does.
import { CELL, allCells, claimCellIds } from "./lattice.mjs";

// Total walk-around room added to each axis before sizing: 2 m each side from the block's edge, which is
// 1 m inside the yard fence. `--margin M` changes it.
export const MARGIN = 4;

// Cells one axis needs. The tiny epsilon keeps a footprint that fills a block exactly (20 m + 4 m margin = 24 m
// = two cells) at two cells instead of three.
export function spanFor(extent, margin = MARGIN) {
  return Math.max(1, Math.ceil((extent + margin) / CELL.size - 1e-9));
}

// What the bounds say at one yaw: world extents along x and z, the footprint centre relative to the pivot
// (the mesh is not always centred on its pivot: a MunicipalOffice's centre is 0.36, 0.56 off), and the height.
export function footprintOf(P, bounds, scale = 1, yaw = 0) {
  const fp = P.footprintRect(bounds, scale, yaw);
  return {
    w: fp.max[0] - fp.min[0],
    d: fp.max[1] - fp.min[1],
    mid: [(fp.min[0] + fp.max[0]) / 2, (fp.min[1] + fp.max[1]) / 2],
    height: (bounds.max[1] - bounds.min[1]) * scale,
    base: bounds.min[1] * scale,
  };
}

// The block sizes the building can take, smallest area first (then the 0/180 orientation first). Each is
// { yaw, span: [cols, rows], area, ex, ez }. A square-ish building gives two equal ones; the caller tries
// every one of the smallest area and keeps the one whose front can face the gap.
export function spanOptions(P, bounds, scale = 1, margin = MARGIN) {
  const out = [0, 90].map((yaw) => {
    const f = footprintOf(P, bounds, scale, yaw);
    const span = [spanFor(f.w, margin), spanFor(f.d, margin)];
    return { yaw, span, area: span[0] * span[1], ex: f.w, ez: f.d };
  });
  // The same block twice (a square footprint) is one option.
  const uniq = out.filter((o, i) => out.findIndex((p) => p.span.join("x") === o.span.join("x")) === i);
  return uniq.sort((a, b) => a.area - b.area);
}

const FRONT_VEC = { "+z": [0, 1], "-z": [0, -1], "+x": [1, 0], "-x": [-1, 0] };

// The yaw (0, 90, 180 or 270) for a block: its front (`front`, a local axis, "+z" by default) faces the gap
// side as nearly as a yaw allows, among the yaws whose footprint plus margin fits the block. `out` is the unit
// vector pointing out of the block through the gap (gapSide(block).out). Returns { yaw, faces, fits } or null when no yaw fits;
// `faces` is true when the front points straight out of the gap.
export function chooseYaw(P, { bounds, scale = 1, margin = MARGIN, front = "+z", block, out }) {
  const fv = FRONT_VEC[front];
  if (!fv) throw new Error(`--front must be one of ${Object.keys(FRONT_VEC).join(", ")}`);
  let best = null;
  for (const yaw of [0, 90, 180, 270]) {
    const f = footprintOf(P, bounds, scale, yaw);
    if (spanFor(f.w, margin) > block.w || spanFor(f.d, margin) > block.h) continue;
    const dir = P.rot(yaw, fv[0], fv[1]);
    const dot = dir[0] * out[0] + dir[1] * out[1];
    if (!best || dot > best.dot + 1e-9) best = { yaw, dot, f };
  }
  return best ? { yaw: best.yaw, faces: best.dot > 0.999, fp: best.f } : null;
}

// Where the pivot goes so the footprint's centre sits at the block's centre (`place ground` does the same
// from its --at, so this is for the dry run and the log).
export function pivotAt(block, fp) {
  const c = [(block.x0 + block.x1) / 2, (block.z0 + block.z1) / 2];
  return { centre: c, pivot: [c[0] - fp.mid[0], c[1] - fp.mid[1]] };
}

// The largest block of free cells (`taken` is a Set of claimed cell ids, `ents` the grid's entities the way
// `blockers` reads them), by area then by the longer side: { w, h, id } or null when no cell is free.
export function largestFree({ taken = new Set(), ents = [], blocked }) {
  const cells = allCells();
  let best = null;
  for (const c of cells) {
    for (let h = 1; c.row + h <= CELL.rows; h++) {
      for (let w = 1; c.col + w <= CELL.cols; w++) {
        const ids = claimCellIds({ cell: c.id, span: [w, h] });
        if (ids.length !== w * h) continue;
        if (ids.some((id) => taken.has(id) || blocked(id, ents))) break; // a wider block of this height cannot be free either
        const area = w * h;
        if (!best || area > best.area || (area === best.area && Math.max(w, h) > Math.max(best.w, best.h))) best = { w, h, area, id: c.id };
      }
    }
  }
  return best;
}

// "3 x 2 cells (36 x 24 m)".
export const blockText = (w, h) => `${w}x${h} cells (${w * CELL.size} x ${h * CELL.size} m)`;

// Why no block fits, in one line an agent can act on: what was tried and the largest free block.
export function noFitMessage(tried, big) {
  const wanted = tried.map((o) => `${o.span[0]}x${o.span[1]} (yaw ${o.yaw}/${(o.yaw + 180) % 360})`).join(" and ");
  if (!big) return `grid: no free block for this prefab (it needs ${wanted}) and no free cell is left; free one with tools\\cli grid free <slug> (grid list shows them)`;
  return `grid: no free ${wanted} block (the grid is ${CELL.cols} x ${CELL.rows} cells). The largest free block is ${blockText(big.w, big.h)} at ${big.id}; free a claim with tools\\cli grid free <slug>, shrink --margin, or wait for the lattice to grow`;
}
