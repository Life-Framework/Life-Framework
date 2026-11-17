// tools\cli grid: one debug-grid cell per feature in the GridWorld annex (docs/architecture/world-zones.md).
//
//   grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--span WxH] [--pack] [--margin M] [--front +z] [--rows] [--centre] [--dry] [--no-verify] [--no-fetch]
//   grid list [--no-fetch]
//   grid free <slug>
//
// A new claim is made against origin/main as well as this worktree. Every worktree holds its own copy of
// tools/grid/cells/, so two slices claiming at once each saw a free cell (both took c2r4, 2026-11-16).
// `place` fetches origin/main and treats the claims there as taken; `--no-fetch` reads the origin/main
// this clone already has.
//
// Claims in flight count too: a PR's claim is not on origin/main until it merges, so `place` and
// `list` also read the claim files of every open PR (`gh pr list`, cached 60 s in the temp dir) and
// treat those cells as taken (#2014 and #2016 both took c0r6, 2026-11-16). No `gh` or token is a
// printed warning, not a failure. `--no-fetch` is offline and skips it.
//
// Claims not yet committed count too: a claim file still `??` in a sibling local worktree is on no remote
// and in no PR (ws/police-station-kit and ws/building-bank both took c3r1, 2026-11-16). `place` and `list`
// read tools/grid/cells/*.json of every other worktree of this clone (`git worktree list --porcelain`, no
// network, so `--no-fetch` keeps it), treat those cells as taken and name the worktree in the clash.
//
// A pack cell (--pack, or any prefab under addons/LF_Everon1989/) writes its layer to the pack's grid world
// (LF_Everon1989/Worlds/Everon1989Grid, a SubScene of GridWorld) and records "pack": true in the claim; the cell
// still comes from the one lattice (docs/architecture/world-zones.md "Pack cells").
//
// One prefab on a new slug is sized for you (tools/grid/autosize.mjs): the bounds come from the placement tool's
// reader (the .xob header, offline), the span is ceil((footprint + 4 m) / 12) per axis for whichever of the two
// orientations needs the smaller block, the first free block of that size is claimed, and a block of two cells or
// more gets the building itself: its class read from the prefab, its footprint centred in the block, its front
// facing the yard's gap, stood on the ground through `place ground` and audited. `--dry` prints all of it and
// writes nothing. `--span` and `--cell` still override; several prefabs, or `--rows`, keep the rows of props.
//
// `place` claims a cell for the feature (deterministic: the first usable cell from a hash of the
// slug; `--span 3x2` takes a block of cells for a big item: the claim file gets "span": [3, 2] and one yard goes
// round the whole block), writes GridWorld_Layers/Grid_<slug>.layer with the cell's fenced yard (a 5 m gap facing
// the annex centre) and a label sign reading the slug, records the claim in
// tools/grid/cells/<slug>.json, then puts the prefabs in a row inside the yard through
// `tools\cli place arrange row` (offline checks, ground probe, audit). Each later call for the same
// slug adds the next row, three rows in all. `free` deletes the layer and the claim.
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { CELL, isFrameClaim } from "../grid/lattice.mjs";
import { loadWorld } from "../../agent-box/layers.mjs";
import { withSpawn } from "../grid/cell-spawn.mjs";
import { MARGIN, blockText, boundsWithChildren, chooseYaw, footprintOf, largestFree, noFitMessage, pivotAt, spanOptions } from "../grid/autosize.mjs";
import { readResource } from "../actions/place-action-contexts.mjs";
import { resolvePrefab } from "../lib/game-prefab.mjs";
import {
  CELLS_DIR, GRID_LAYERS, PACK_GRID_LAYERS, SLUG_RX, allCells, allocate, blockers, cellBounds, claimCellIds, claimRect, entityPrefix, gapSide, gridLayersOfClaim, isPackPrefab, layerOfSlug, mainClaims, openPrClaims, parseCellId, readCells, rowLine, worktreeClaims, yardPlan, yawAlong,
} from "../lib/world-zones.mjs";
import { gridLayerDirs } from "../lib/worlds.mjs";

export const help = "grid place <slug> <prefab> [--dry] (a building: sized from its bounds, claimed, centred, front to the gap) | grid place <slug> [<prefab>...] | grid list | grid free <slug>   a debug-grid cell per feature in the GridWorld annex: fenced yard, label sign, props placed and audited (docs/architecture/world-zones.md)";

export const FENCE = { cls: "SCR_DestructibleEntity", ref: "{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et" };
export const SIGN = { cls: "GenericEntity", ref: "{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et", component: "{026619B31213409D}", baseY: 0.505 };
export const SIGN_PREFIX = "grid:"; // LF_SignCatalog.GridLabel turns grid:<slug> into the plate's title

// Every entity of GridWorld's layers (the legacy annex stations and the grid layers) and of the pack's grid world
// (Everon1989Grid: the pack cells); none when the folders are missing.
const gridEnts = (root) => {
  const dirs = gridLayerDirs(root).filter((d) => existsSync(d));
  return dirs.length ? loadWorld(root, dirs) : [];
};

// Pack routing (docs/architecture/world-zones.md "Pack cells"): a cell is a PACK cell when it is asked for with --pack, when
// its claim already says so, or when a prefab it places lives under addons/LF_Everon1989/. Its layer then goes to
// the pack's grid world and the claim records "pack": true. A claimed cell keeps its world.
export function packRouting(root, { entry = null, flags = {}, prefabs = [] } = {}) {
  const packPrefabs = prefabs.filter((p) => isPackPrefab(root, p));
  if (entry) {
    const pack = entry.pack === true;
    if (!pack && (flags.pack || packPrefabs.length)) {
      const why = flags.pack ? "--pack asks for the pack" : `${packPrefabs[0]} is pack content`;
      return { pack, error: `${entry.slug} is a core cell (its layer is in GridWorld_Layers) but ${why}. A core layer must not name a pack prefab: run tools\\cli grid free ${entry.slug}, then grid place ${entry.slug} again (a pack prefab or --pack picks the pack grid world)` };
    }
    return { pack };
  }
  return { pack: Boolean(flags.pack) || packPrefabs.length > 0, auto: !flags.pack && packPrefabs.length > 0 };
}

const num = (v) => {
  const r = Math.round(v * 1000) / 1000;
  return String(Object.is(r, -0) ? 0 : r);
};

// The text of a new grid layer: the fence runs and the label sign. Pure; `rot` and `yawForFacing`
// come from tools/place/place.mjs.
export function yardLayer(slug, cell, { rot, yawForFacing, parseHeading }) {
  const plan = yardPlan(cell);
  const pre = entityPrefix(slug);
  const out = [`// Grid cell ${cell.id} for ${slug}: written by tools\\cli grid place (docs/architecture/world-zones.md). Debug zone: low look bar.`];
  plan.runs.forEach((r, i) => {
    const yaw = yawAlong(r.dir, rot);
    out.push(`${FENCE.cls} ${pre}_Fence_${i + 1} : "${FENCE.ref}" {`, ` coords ${num(r.at[0])} 0 ${num(r.at[1])}`);
    if (yaw) out.push(` angleY ${yaw}`);
    out.push("}");
  });
  const signYaw = yawForFacing(parseHeading(plan.gap.side), "+z");
  out.push(
    `${SIGN.cls} ${pre}_Sign : "${SIGN.ref}" {`,
    " components {",
    `  LF_SignComponent "${SIGN.component}" {`,
    `   m_sSignId "${SIGN_PREFIX}${slug}"`,
    "   m_vRimColor 0.1 0.1 0.1",
    "   m_vPaintColor 0.95 0.78 0.1",
    "   m_vTextColor 0.1 0.1 0.1",
    "  }",
    " }",
    ` coords ${num(plan.signAt[0])} ${SIGN.baseY} ${num(plan.signAt[1])}`,
  );
  if (signYaw) out.push(` angleY ${num(signYaw)}`);
  out.push("}");
  return `${out.join("\n")}\n`;
}

export function registryText(entry) {
  const { slug, cell, layer, rows, created, span, pack } = entry;
  const multi = Array.isArray(span) && (span[0] !== 1 || span[1] !== 1);
  const base = multi ? { slug, cell, span, layer, rows, created } : { slug, cell, layer, rows, created };
  return `${JSON.stringify(pack === true ? { ...base, pack: true } : base, null, 2)}\n`;
}

// A grid map: one character per cell, north at the top. `#` blocked, `.` free, a letter per claim.
export function mapText(allClaims, ents) {
  const claims = allClaims.filter((c) => !isFrameClaim(c));
  const byCell = new Map(claims.flatMap((c, i) => claimCellIds(c).map((id) => [id, String.fromCharCode(97 + (i % 26))])));
  const rows = [];
  const cells = allCells();
  const maxRow = Math.max(...cells.map((c) => c.row));
  for (let row = maxRow; row >= 0; row--) {
    const line = cells.filter((c) => c.row === row).map((c) => byCell.get(c.id) ?? (blockers(c, ents).length ? "#" : ".")).join("");
    rows.push(`  z ${String(cellBounds(0, row).z0).padStart(3)}  ${line}`);
  }
  return rows.join("\n");
}

function parse(argv) {
  const out = { _: [], flags: {} };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      out.flags[k] = ["name", "cell", "span", "margin", "front"].includes(k) ? argv[++i] : true;
    } else out._.push(a);
  }
  return out;
}

// The claims open PRs hold (`inFlight`): the injected reader in tests, none offline (`--no-fetch`), else `gh`.
function prView(root, flags, ctx, local, onMain) {
  if (!ctx.openPrClaims && flags["no-fetch"]) return { inFlight: [] };
  const info = (ctx.openPrClaims ?? openPrClaims)(root);
  if (info.note) console.log(`grid: warning: ${info.note}`);
  const known = new Set([...local, ...onMain].map((c) => c.slug));
  return { inFlight: info.claims.filter((c) => c.slug && c.cell && !known.has(c.slug)) };
}

// The claims still uncommitted in sibling local worktrees (`inTrees`): no network, so `--no-fetch` keeps it.
// A slug this tree, origin/main or an open PR already has is theirs, not a second claim. The injected reader
// replaces `git worktree list` in tests.
function treeView(root, ctx, known) {
  const info = (ctx.worktreeClaims ?? worktreeClaims)(root);
  if (info.note) console.log(`grid: warning: ${info.note}`);
  const seen = new Set(known.map((c) => c.slug));
  return { inTrees: info.claims.filter((c) => c.slug && c.cell && !seen.has(c.slug)) };
}

// The claims origin/main holds (`onMain`), and those this tree lacks (`extra`: a branch cut before they merged).
function mainView(root, flags, local) {
  const info = mainClaims(root, { fetch: !flags["no-fetch"] });
  if (info.note) console.log(`grid: ${info.note}`);
  const onMain = info.claims.filter((c) => !c.error && c.slug && c.cell);
  const mine = new Set(local.map((c) => c.slug));
  return { onMain, extra: onMain.filter((c) => !mine.has(c.slug)) };
}

function list(root, flags, ctx = {}) {
  const local = readCells(root);
  const { onMain, extra } = mainView(root, flags, local);
  const { inFlight } = prView(root, flags, ctx, local, onMain);
  const { inTrees } = treeView(root, ctx, [...local, ...onMain, ...inFlight]);
  const frame = [...local, ...extra].find(isFrameClaim);
  const claims = [...local, ...extra, ...inFlight, ...inTrees].filter((c) => !isFrameClaim(c));
  const ents = gridEnts(root);
  const taken = new Set(claims.flatMap(claimCellIds));
  const free = allCells().filter((c) => !taken.has(c.id) && !blockers(c, ents).length).length;
  console.log(`grid: ${claims.length} cell(s) claimed (${extra.length} only on origin/main, ${inFlight.length} in open PRs, ${inTrees.length} in other worktrees), ${free} free (${CELLS_DIR}/)`);
  claims.forEach((c, i) => {
    const b = claimRect(c);
    const layer = join(root, gridLayersOfClaim(c), layerOfSlug(c.slug));
    const n = existsSync(layer) ? (readFileSync(layer, "utf8").match(/^\S.* : "\{/gm) || []).length : 0;
    const mine = new Set(claimCellIds(c));
    const clash = inFlight.includes(c) || inTrees.includes(c) ? claims.filter((x) => x !== c && claimCellIds(x).some((id) => mine.has(id))).map((x) => x.slug) : [];
    const clashText = clash.length ? `  CLASH with ${clash.join(", ")}` : "";
    const tail = inFlight.includes(c)
      ? `claimed in open PR #${c.pr} (${c.branch}), not merged yet${clashText}`
      : inTrees.includes(c) ? `claimed in worktree ${c.worktree} (${c.branch || "detached"}), not pushed yet${clashText}`
      : extra.includes(c) ? "claimed on origin/main, not merged here yet" : `${n} entities, ${c.rows ?? 0}/3 rows${c.pack ? ", pack cell" : ""}`;
    console.log(`  ${String.fromCharCode(97 + (i % 26))} ${c.slug.padEnd(28)} ${`${c.cell}${b && b.w * b.h > 1 ? `+${b.w}x${b.h}` : ""}`.padEnd(6)} ${b ? `x ${b.x0}..${b.x1} z ${b.z0}..${b.z1}` : "bad cell"}  ${tail}${c.error ? `  ERROR ${c.error}` : ""}`);
  });
  console.log(mapText(claims, ents));
  if (frame) console.log(`  frame: ${frame.layer} marks every cell (GridMarker_cXrY) and lists them on GridMapBoard at the gate; node tools/grid/lattice.mjs write refreshes the plates`);
  console.log("  (# blocked by legacy stations or reserved ground, . free, letters as listed)");
  return 0;
}

function free(root, slug) {
  if (!slug) return usage();
  const reg = join(root, CELLS_DIR, `${slug}.json`);
  const claim = existsSync(reg) ? readCells(root).find((c) => c.slug === slug) : null;
  const dirs = claim ? [gridLayersOfClaim(claim)] : [GRID_LAYERS, PACK_GRID_LAYERS];
  const layerFiles = dirs.map((d) => join(root, d, layerOfSlug(slug))).filter((f) => existsSync(f));
  if (!existsSync(reg) && !layerFiles.length) {
    console.log(`grid: ${slug} has no cell`);
    return 1;
  }
  for (const f of [reg, ...layerFiles]) if (existsSync(f)) unlinkSync(f);
  console.log(`grid: freed ${slug} (deleted ${CELLS_DIR}/${slug}.json and ${layerOfSlug(slug)}${claim?.pack ? " from the pack grid world" : ""})`);
  return 0;
}

// A prefab's real bounds through the placement tool's reader (prefabInfo: the measured list, else the .xob header
// in the paks, offline). { ref: "{GUID}path", info } or null with a printed reason. The context's `readPrefab` is the selftest's seam.
async function readPrefab(root, P, arg, ctx) {
  if (ctx.readPrefab) return ctx.readPrefab(arg);
  try {
    const r = await resolvePrefab(root, arg);
    if (!r.ref) throw new Error(`no GUID known for ${r.path}`);
    const info = P.prefabInfo(P.ROOT, r.path);
    if (!info.bounds) throw new Error(`${r.path} has no mesh the tool can measure (${info.error || "no MeshObject in its chain"})`);
    // The block takes the prefab's annex children too (FenceYard: a main building and a side one), not the root mesh alone.
    let sized = info;
    try {
      const text = readResource(P.ROOT, r.path);
      const kids = text ? P.modelOf(P.ROOT, text, r.path, { prefabRoot: r.path }) : [];
      sized = { ...info, bounds: boundsWithChildren(P, info.bounds, info.scale, kids) };
    } catch {
      // a child the tool cannot read leaves the root's own bounds
    }
    return { ref: r.ref, info: sized };
  } catch (e) {
    console.log(`grid: cannot size ${arg}: ${e.message}`);
    return null;
  }
}

const nameOf = (ref) => String(ref).split("/").pop().replace(/.et$/, "");
const r2 = (n) => Math.round(n * 100) / 100;

async function place(root, args, flags, ctx = {}) {
  const [slug, ...prefabs] = args;
  if (!slug || !SLUG_RX.test(slug)) {
    console.log(`grid: the slug must be lower-case letters, digits and dashes (2 to 41), like police-gates; got ${slug ?? "nothing"}`);
    return 1;
  }
  const P = await import("../place/place.mjs");
  const claims = readCells(root);
  const regFile = join(root, CELLS_DIR, `${slug}.json`);
  const layerName = layerOfSlug(slug);
  let entry = claims.find((c) => c.slug === slug) ?? null;
  const route = packRouting(root, { entry, flags, prefabs });
  if (route.error) {
    console.log(`grid: ${route.error}`);
    return 1;
  }
  const layersDir = route.pack ? PACK_GRID_LAYERS : GRID_LAYERS;
  const layerFile = join(root, layersDir, layerName);
  const ents = gridEnts(root);
  let fresh = false;
  let span = [1, 1];
  // One prefab on a new slug is measured and sized (the span, the block and the yaw come from its bounds).
  const margin = flags.margin == null ? MARGIN : Number(flags.margin);
  if (!Number.isFinite(margin) || margin < 0) throw new Error(`--margin ${flags.margin} is not a number of metres (default ${MARGIN})`);
  const front = flags.front === true || flags.front == null ? "+z" : String(flags.front);
  const auto = !entry && prefabs.length === 1 && !flags.rows ? await readPrefab(root, P, prefabs[0], ctx) : null;
  const opts = auto ? spanOptions(P, auto.info.bounds, auto.info.scale, margin) : null;
  if (auto) {
    const f = footprintOf(P, auto.info.bounds, auto.info.scale, 0);
    console.log(`grid: ${nameOf(auto.ref)} ${auto.ref} (${auto.info.cls}): footprint ${r2(f.w)} x ${r2(f.d)} m, height ${r2(f.height)} m (base ${r2(f.base)} m from the pivot), footprint centre ${r2(f.mid[0])}, ${r2(f.mid[1])} m (x, z) from the pivot`);
    console.log(`  sizing with a ${margin} m walk-around margin: ${opts.map((o) => `${o.span[0]}x${o.span[1]} at yaw ${o.yaw}/${(o.yaw + 180) % 360}`).join(", ")}`);
  }
  if (!entry) {
    const { onMain } = mainView(root, flags, claims);
    const dup = onMain.find((c) => c.slug === slug);
    if (dup) {
      console.log(`grid: origin/main already has a claim for ${slug} (cell ${dup.cell}); merge origin/main into this branch, or pick another slug`);
      return 1;
    }
    const { inFlight } = prView(root, flags, ctx, claims, onMain);
    const dupPr = inFlight.find((c) => c.slug === slug);
    if (dupPr) {
      console.log(`grid: open PR #${dupPr.pr} (${dupPr.branch}) already claims ${slug} (cell ${dupPr.cell}); work on that branch, or pick another slug`);
      return 1;
    }
    const { inTrees } = treeView(root, ctx, [...claims, ...onMain, ...inFlight]);
    const dupTree = inTrees.find((c) => c.slug === slug);
    if (dupTree) {
      console.log(`grid: worktree ${dupTree.worktree} (${dupTree.branch}) already claims ${slug} (cell ${dupTree.cell}); work there, or pick another slug`);
      return 1;
    }
    const takenOnMain = new Map(onMain.flatMap((c) => claimCellIds(c).map((id) => [id, c.slug])));
    const takenInPr = new Map(inFlight.flatMap((c) => claimCellIds(c).map((id) => [id, c])));
    const takenInTrees = new Map(inTrees.flatMap((c) => claimCellIds(c).map((id) => [id, c])));
    const taken = new Set([...claims.flatMap(claimCellIds), ...takenOnMain.keys(), ...takenInPr.keys(), ...takenInTrees.keys()]);
    const cellOf = (id) => allCells().find((c) => c.id === id);
    const blocked = (id, e) => blockers(cellOf(id), e).length > 0;
    const whyNot = (id) => {
      const owner = takenOnMain.get(id);
      const pr = takenInPr.get(id);
      const wt = takenInTrees.get(id);
      return [...(owner ? [`claimed on origin/main by ${owner}`] : pr ? [`claimed by ${pr.slug} in open PR #${pr.pr}`] : wt ? [`claimed by ${wt.slug} in worktree ${wt.worktree} (${wt.branch || "detached"}), not pushed yet`] : taken.has(id) ? ["claimed"] : []), ...blockers(cellOf(id), ents)];
    };
    const sized = auto && flags.span == null; // no --span: the bounds say how big
    if (flags.span != null) {
      const sp = /^([0-9]+)x([0-9]+)$/.exec(flags.span === true ? "" : String(flags.span));
      if (!sp) throw new Error(`--span ${flags.span} is not cols x rows, like 3x2`);
      span = [Number(sp[1]), Number(sp[2])];
    }
    let cell;
    if (flags.cell) {
      if (!parseCellId(flags.cell)) throw new Error(`--cell ${flags.cell} is not a cell (c0r0 to c${CELL.cols - 1}r${CELL.rows - 1})`);
      if (sized) span = (opts.find((o) => claimRect({ cell: flags.cell, span: o.span })) ?? opts[0]).span;
      cell = claimRect({ cell: flags.cell, span });
      if (!cell) throw new Error(`--cell ${flags.cell} --span ${span.join("x")} leaves the lattice (${CELL.cols} x ${CELL.rows} cells)`);
      const why = claimCellIds({ cell: flags.cell, span }).flatMap((id) => whyNot(id).map((w) => `${id}: ${w}`));
      if (why.length) {
        console.log(`grid: cell ${cell.id} is not free: ${why.join("; ")}`);
        return 1;
      }
    } else if (sized) {
      // The smallest block first; among blocks of the same area (the two orientations), the one whose front can face the gap.
      for (const area of [...new Set(opts.map((o) => o.area))]) {
        const found = opts.filter((o) => o.area === area).map((o) => ({ o, rect: allocate(slug, { ents, taken, span: o.span }) })).filter((x) => x.rect);
        if (!found.length) continue;
        const pick = found.find((x) => chooseYaw(P, { bounds: auto.info.bounds, scale: auto.info.scale, margin, front, block: x.rect, out: gapSide(x.rect).out })?.faces) ?? found[0];
        cell = pick.rect;
        span = pick.o.span;
        break;
      }
      if (!cell) {
        console.log(noFitMessage(opts, largestFree({ taken, ents, blocked })));
        return 1;
      }
    } else cell = allocate(slug, { ents, taken, span });
    if (!cell) {
      const big = largestFree({ taken, ents, blocked });
      console.log(span[0] * span[1] > 1 && big ? `grid: no free ${blockText(span[0], span[1])} block. The largest free block is ${blockText(big.w, big.h)} at ${big.id}; free a claim with tools\\cli grid free <slug>` : "grid: no free cell left; free one with tools\\cli grid free <slug> (grid list shows them)");
      return 1;
    }
    entry = { slug, cell: cell.id, layer: layerName, rows: 0, created: new Date().toISOString().slice(0, 10) };
    if (span[0] !== 1 || span[1] !== 1) entry.span = span;
    if (route.pack) entry.pack = true;
    fresh = true;
  }
  const cell = claimRect(entry);
  if (!cell) throw new Error(`${slug}'s claim (cell ${entry.cell}, span ${JSON.stringify(entry.span ?? [1, 1])}) is not a block of the lattice`);
  const gap = yardPlan(cell).gap;
  console.log(`grid: ${slug} -> cell ${cell.id}${cell.w * cell.h > 1 ? ` +${cell.w}x${cell.h}` : ""} (x ${cell.x0}..${cell.x1}, z ${cell.z0}..${cell.z1}, gap ${gap.side})${fresh ? " (new)" : ""}${route.pack ? ` [pack cell${route.auto ? `: ${prefabs.find((p) => isPackPrefab(root, p))} is pack content` : ""}]` : ""}`);
  // A block of two cells or more (or --centre) gets the building itself, centred, front to the gap; one cell keeps the rows of props.
  let building = null;
  if (auto && fresh) {
    const y = chooseYaw(P, { bounds: auto.info.bounds, scale: auto.info.scale, margin, front, block: cell, out: gap.out });
    if (!y) {
      const need = opts.map((o) => `${o.span[0]}x${o.span[1]}`).join(" or ");
      console.log(`grid: ${nameOf(auto.ref)} does not fit ${cell.w}x${cell.h} cells with a ${margin} m margin (it needs ${need}); give a bigger --span, or lower --margin`);
      return 1;
    }
    if (cell.w * cell.h > 1 || flags.centre) building = { ...y, ...pivotAt(cell, y.fp), name: flags.name ?? `${entityPrefix(slug)}_Main` };
  }
  if (flags.dry) {
    if (fresh) console.log(`  would write ${layersDir}/${layerName} (the fence runs and the label sign) and ${CELLS_DIR}/${slug}.json`);
    if (building) {
      console.log(`  would place ${auto.info.cls} ${building.name} (${nameOf(auto.ref)}): footprint centre ${num(building.centre[0])} ${num(building.centre[1])}, pivot x ${num(building.pivot[0])} z ${num(building.pivot[1])}, angleY ${num(building.yaw)}; ${building.faces ? `front (${front}) faces the ${gap.side} gap` : `front (${front}) cannot face the ${gap.side} gap in this block and turns as near as a right angle allows`}; y from the ground probe`);
    } else {
      const line = prefabs.length ? rowLine(cell, entry.rows ?? 0) : null;
      if (line) console.log(`  would place ${prefabs.length} prop(s) in row ${entry.rows ?? 0}: ${line.a.join(",")} to ${line.b.join(",")}, facing ${line.facing}`);
    }
    console.log("grid: dry run, nothing written.");
    return 0;
  }
  if (fresh || !existsSync(layerFile)) mkdirSync(dirname(layerFile), { recursive: true });
  if (fresh || !existsSync(layerFile)) writeFileSync(layerFile, withSpawn(yardLayer(slug, cell, P), slug, yardPlan(cell)));
  if (fresh) {
    mkdirSync(join(root, CELLS_DIR), { recursive: true });
    writeFileSync(regFile, registryText(entry));
    console.log(`  wrote ${layersDir}/${layerName} (yard and sign ${entityPrefix(slug)}_Sign) and ${CELLS_DIR}/${slug}.json`);
  }
  let code = 0;
  const pass = ["no-verify", "keep", "force"].filter((k) => flags[k]).map((k) => `--${k}`);
  if (building) {
    const argv = ["ground", `${num(building.centre[0])},${num(building.centre[1])}`, auto.ref, "--layer", layerName, "--yaw", num(building.yaw), "--y-hint", "0", "--name", building.name, ...pass];
    console.log(`  tools\\cli place ${argv.join(" ")}`);
    code = await (ctx.placeMain ?? P.main)(argv);
    if (code !== 0 && fresh && !flags.keep) {
      // This call made the claim: a building that would not stand leaves no half-claimed block behind.
      for (const f of [regFile, layerFile]) if (existsSync(f)) unlinkSync(f);
      console.log(`grid: the building was not placed, so the claim ${slug} (${cell.id}) and its yard were removed; fix what the place tool says (--force, --margin, --span, --cell) and run it again`);
      return code;
    }
  } else if (prefabs.length) {
    const line = rowLine(cell, entry.rows ?? 0);
    if (!line) {
      console.log(`grid: ${slug}'s cell has its three rows; move a prop with tools\\cli place --move, or free the cell and start again`);
      return 1;
    }
    const name = flags.name ?? `${entityPrefix(slug)}_R${entry.rows ?? 0}`;
    const argv = ["arrange", "row", line.a.map(num).join(","), line.b.map(num).join(","), ...prefabs, "--layer", layerName, "--facing", line.facing, "--y-hint", "0", "--name", name, ...pass];
    console.log(`  tools\\cli place ${argv.join(" ")}`);
    code = await P.main(argv);
    if (code === 0) {
      entry.rows = (entry.rows ?? 0) + 1;
      writeFileSync(regFile, registryText(entry));
    }
  }
  if (fresh && !flags["no-verify"] && code === 0) {
    // A building: the placement audit of the whole layer (yard, sign, building) on GridWorld, strict; else the sign alone.
    console.log(building ? `  auditing the cell (audit placement --grid --strict --layer ${layerName})` : "  auditing the label sign");
    code = await (ctx.auditMain ?? defaultAudit(P))(building ? ["audit-layer", layerName] : ["verify", `${entityPrefix(slug)}_Sign`]);
  }
  if (code === 0) console.log(`grid: done. tools\\cli look grid:${slug} for the picture.`);
  return code;
}

// The audit step: `place verify` for the sign, the placement audit on one grid layer (strict) for a building.
const defaultAudit = (P) => async (verify) => {
  if (verify[0] === "verify") return P.main(verify);
  const A = await import("../../agent-box/place-audit.mjs");
  return A.main(["placement", "--grid", "--layer", verify[1], "--strict"]);
};

function usage() {
  console.log("usage: tools\\cli grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--span WxH] [--pack] [--margin M] [--front +z] [--rows] [--centre] [--dry] [--no-verify] [--no-fetch]\n       tools\\cli grid list [--no-fetch]\n       tools\\cli grid free <slug>\n(docs/architecture/world-zones.md)");
  return 1;
}

// `openPrClaims(root)`, `worktreeClaims(root)`, `readPrefab(arg)`, `placeMain(argv)` and `auditMain(argv)` in the context replace the `gh`, `git worktree`, bounds, place-tool and audit steps (the selftest's seam).
export async function main(argv, { root, openPrClaims: prReader, worktreeClaims: treeReader, readPrefab: prefabReader, placeMain, auditMain }) {
  const ctx = { openPrClaims: prReader, worktreeClaims: treeReader, readPrefab: prefabReader, placeMain, auditMain };
  const [sub, ...rest] = argv;
  const { _: args, flags } = parse(rest);
  try {
    if (sub === "place") return await place(root, args, flags, ctx);
    if (sub === "list") return list(root, flags, ctx);
    if (sub === "free") return free(root, args[0]);
  } catch (e) {
    console.log(`grid: ${e.message}`);
    return 1;
  }
  return usage();
}
