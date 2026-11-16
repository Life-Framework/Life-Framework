// tools\cli grid: one debug-grid cell per feature in the GridWorld annex (docs/architecture/world-zones.md).
//
//   grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--span WxH] [--dry] [--no-verify] [--no-fetch]
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
// `place` claims a cell for the feature (deterministic: the first usable cell from a hash of the
// slug; `--span 3x2` takes a block of cells for a big item: the claim file gets "span": [3, 2] and one yard goes
// round the whole block), writes GridWorld_Layers/Grid_<slug>.layer with the cell's fenced yard (a 5 m gap facing
// the annex centre) and a label sign reading the slug, records the claim in
// tools/grid/cells/<slug>.json, then puts the prefabs in a row inside the yard through
// `tools\cli place arrange row` (offline checks, ground probe, audit). Each later call for the same
// slug adds the next row, three rows in all. `free` deletes the layer and the claim.
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CELL, isFrameClaim } from "../grid/lattice.mjs";
import { loadWorld } from "../../agent-box/layers.mjs";
import { withSpawn } from "../grid/cell-spawn.mjs";
import {
  CELLS_DIR, GRID_LAYERS, SLUG_RX, allCells, allocate, blockers, cellBounds, claimCellIds, claimRect, entityPrefix, layerOfSlug, mainClaims, openPrClaims, parseCellId, readCells, rowLine, yardPlan, yawAlong,
} from "../lib/world-zones.mjs";

export const help = "grid place <slug> [<prefab>...] | grid list | grid free <slug>   a debug-grid cell per feature in the GridWorld annex: fenced yard, label sign, props placed and audited (docs/architecture/world-zones.md)";

export const FENCE = { cls: "SCR_DestructibleEntity", ref: "{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et" };
export const SIGN = { cls: "GenericEntity", ref: "{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et", component: "{026619B31213409D}", baseY: 0.505 };
export const SIGN_PREFIX = "grid:"; // LF_SignCatalog.GridLabel turns grid:<slug> into the plate's title

// Every entity of GridWorld's layers (the legacy annex stations and the grid layers); none when the folder is missing.
const gridEnts = (root) => (existsSync(join(root, GRID_LAYERS)) ? loadWorld(root, join(root, GRID_LAYERS)) : []);

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
  const { slug, cell, layer, rows, created, span } = entry;
  const multi = Array.isArray(span) && (span[0] !== 1 || span[1] !== 1);
  return `${JSON.stringify(multi ? { slug, cell, span, layer, rows, created } : { slug, cell, layer, rows, created }, null, 2)}\n`;
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
      out.flags[k] = ["name", "cell", "span"].includes(k) ? argv[++i] : true;
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
  const frame = [...local, ...extra].find(isFrameClaim);
  const claims = [...local, ...extra, ...inFlight].filter((c) => !isFrameClaim(c));
  const ents = gridEnts(root);
  const taken = new Set(claims.flatMap(claimCellIds));
  const free = allCells().filter((c) => !taken.has(c.id) && !blockers(c, ents).length).length;
  console.log(`grid: ${claims.length} cell(s) claimed (${extra.length} only on origin/main, ${inFlight.length} in open PRs), ${free} free (${CELLS_DIR}/)`);
  claims.forEach((c, i) => {
    const b = claimRect(c);
    const layer = join(root, GRID_LAYERS, layerOfSlug(c.slug));
    const n = existsSync(layer) ? (readFileSync(layer, "utf8").match(/^\S.* : "\{/gm) || []).length : 0;
    const mine = new Set(claimCellIds(c));
    const clash = inFlight.includes(c) ? claims.filter((x) => x !== c && claimCellIds(x).some((id) => mine.has(id))).map((x) => x.slug) : [];
    const tail = inFlight.includes(c)
      ? `claimed in open PR #${c.pr} (${c.branch}), not merged yet${clash.length ? `  CLASH with ${clash.join(", ")}` : ""}`
      : extra.includes(c) ? "claimed on origin/main, not merged here yet" : `${n} entities, ${c.rows ?? 0}/3 rows`;
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
  const layer = join(root, GRID_LAYERS, layerOfSlug(slug));
  if (!existsSync(reg) && !existsSync(layer)) {
    console.log(`grid: ${slug} has no cell`);
    return 1;
  }
  for (const f of [reg, layer]) if (existsSync(f)) unlinkSync(f);
  console.log(`grid: freed ${slug} (deleted ${CELLS_DIR}/${slug}.json and ${layerOfSlug(slug)})`);
  return 0;
}

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
  const layerFile = join(root, GRID_LAYERS, layerName);
  let entry = claims.find((c) => c.slug === slug) ?? null;
  const ents = gridEnts(root);
  let fresh = false;
  let span = [1, 1];
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
    const takenOnMain = new Map(onMain.flatMap((c) => claimCellIds(c).map((id) => [id, c.slug])));
    const takenInPr = new Map(inFlight.flatMap((c) => claimCellIds(c).map((id) => [id, c])));
    const taken = new Set([...claims.flatMap(claimCellIds), ...takenOnMain.keys(), ...takenInPr.keys()]);
    const sp = /^([0-9]+)x([0-9]+)$/.exec(flags.span === true ? "" : String(flags.span ?? "1x1"));
    if (!sp) throw new Error(`--span ${flags.span} is not cols x rows, like 3x2`);
    span = [Number(sp[1]), Number(sp[2])];
    let cell;
    if (flags.cell) {
      if (!parseCellId(flags.cell)) throw new Error(`--cell ${flags.cell} is not a cell (c0r0 to c${CELL.cols - 1}r${CELL.rows - 1})`);
      cell = claimRect({ cell: flags.cell, span });
      if (!cell) throw new Error(`--cell ${flags.cell} --span ${span.join("x")} leaves the lattice (${CELL.cols} x ${CELL.rows} cells)`);
      const why = [];
      for (const id of claimCellIds({ cell: flags.cell, span })) {
        const owner = takenOnMain.get(id);
        const pr = takenInPr.get(id);
        const c1 = allCells().find((c) => c.id === id);
        for (const w of [...(owner ? [`claimed on origin/main by ${owner}`] : pr ? [`claimed by ${pr.slug} in open PR #${pr.pr}`] : taken.has(id) ? ["claimed"] : []), ...blockers(c1, ents)]) why.push(`${id}: ${w}`);
      }
      if (why.length) {
        console.log(`grid: cell ${cell.id} is not free: ${why.join("; ")}`);
        return 1;
      }
    } else cell = allocate(slug, { ents, taken, span });
    if (!cell) {
      console.log("grid: no free cell left; free one with tools\\cli grid free <slug> (grid list shows them)");
      return 1;
    }
    entry = { slug, cell: cell.id, layer: layerName, rows: 0, created: new Date().toISOString().slice(0, 10) };
    if (span[0] !== 1 || span[1] !== 1) entry.span = span;
    fresh = true;
  }
  const cell = claimRect(entry);
  if (!cell) throw new Error(`${slug}'s claim (cell ${entry.cell}, span ${JSON.stringify(entry.span ?? [1, 1])}) is not a block of the lattice`);
  console.log(`grid: ${slug} -> cell ${cell.id}${cell.w * cell.h > 1 ? ` +${cell.w}x${cell.h}` : ""} (x ${cell.x0}..${cell.x1}, z ${cell.z0}..${cell.z1}, gap ${yardPlan(cell).gap.side})${fresh ? " (new)" : ""}`);
  if (flags.dry) {
    if (fresh) console.log(`  would write ${GRID_LAYERS}/${layerName} (the fence runs and the label sign) and ${CELLS_DIR}/${slug}.json`);
    const line = prefabs.length ? rowLine(cell, entry.rows ?? 0) : null;
    if (line) console.log(`  would place ${prefabs.length} prop(s) in row ${entry.rows ?? 0}: ${line.a.join(",")} to ${line.b.join(",")}, facing ${line.facing}`);
    console.log("grid: dry run, nothing written.");
    return 0;
  }
  if (fresh || !existsSync(layerFile)) writeFileSync(layerFile, withSpawn(yardLayer(slug, cell, P), slug, yardPlan(cell)));
  if (fresh) {
    mkdirSync(join(root, CELLS_DIR), { recursive: true });
    writeFileSync(regFile, registryText(entry));
    console.log(`  wrote ${GRID_LAYERS}/${layerName} (yard and sign ${entityPrefix(slug)}_Sign) and ${CELLS_DIR}/${slug}.json`);
  }
  let code = 0;
  if (prefabs.length) {
    const line = rowLine(cell, entry.rows ?? 0);
    if (!line) {
      console.log(`grid: ${slug}'s cell has its three rows; move a prop with tools\\cli place --move, or free the cell and start again`);
      return 1;
    }
    const pass = ["no-verify", "keep", "force"].filter((k) => flags[k]).map((k) => `--${k}`);
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
    console.log(`  auditing the label sign`);
    code = await P.main(["verify", `${entityPrefix(slug)}_Sign`]);
  }
  if (code === 0) console.log(`grid: done. tools\\cli look grid:${slug} for the picture.`);
  return code;
}

function usage() {
  console.log("usage: tools\\cli grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--span WxH] [--dry] [--no-verify] [--no-fetch]\n       tools\\cli grid list [--no-fetch]\n       tools\\cli grid free <slug>\n(docs/architecture/world-zones.md)");
  return 1;
}

// `openPrClaims(root)` in the context replaces the `gh` reader (the selftest's seam).
export async function main(argv, { root, openPrClaims: prReader }) {
  const ctx = { openPrClaims: prReader };
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
