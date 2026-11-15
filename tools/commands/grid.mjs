// tools\cli grid: one debug-grid cell per feature in the DebugWorld annex (docs/architecture/world-zones.md).
//
//   grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--dry] [--no-verify]
//   grid list
//   grid free <slug>
//
// `place` claims a cell for the feature (deterministic: the first usable cell from a hash of the
// slug), writes DebugWorld_Layers/Grid_<slug>.layer with the cell's fenced yard (a 5 m gap facing
// the annex centre) and a label sign reading the slug, records the claim in
// tools/grid/cells/<slug>.json, then puts the prefabs in a row inside the yard through
// `tools\cli place arrange row` (offline checks, ground probe, audit). Each later call for the same
// slug adds the next row, three rows in all. `free` deletes the layer and the claim.
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { loadWorld } from "../../agent-box/layers.mjs";
import {
  CELLS_DIR, DEBUG_LAYERS, SLUG_RX, allCells, allocate, blockers, cellBounds, entityPrefix, layerOfSlug, parseCellId, readCells, rowLine, yardPlan, yawAlong,
} from "../lib/world-zones.mjs";

export const help = "grid place <slug> [<prefab>...] | grid list | grid free <slug>   a debug-grid cell per feature in the DebugWorld annex: fenced yard, label sign, props placed and audited (docs/architecture/world-zones.md)";

export const FENCE = { cls: "SCR_DestructibleEntity", ref: "{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et" };
export const SIGN = { cls: "GenericEntity", ref: "{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et", component: "{026619B31213409D}", baseY: 0.505 };
export const SIGN_PREFIX = "grid:"; // LF_SignCatalog.GridLabel turns grid:<slug> into the plate's title

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
  const { slug, cell, layer, rows, created } = entry;
  return `${JSON.stringify({ slug, cell, layer, rows, created }, null, 2)}\n`;
}

// A grid map: one character per cell, north at the top. `#` blocked, `.` free, a letter per claim.
export function mapText(claims, ents) {
  const byCell = new Map(claims.map((c, i) => [c.cell, String.fromCharCode(97 + (i % 26))]));
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
      out.flags[k] = ["name", "cell"].includes(k) ? argv[++i] : true;
    } else out._.push(a);
  }
  return out;
}

function list(root) {
  const claims = readCells(root);
  const ents = loadWorld(root);
  const free = allCells().filter((c) => !claims.some((x) => x.cell === c.id) && !blockers(c, ents).length).length;
  console.log(`grid: ${claims.length} cell(s) claimed, ${free} free (${CELLS_DIR}/)`);
  claims.forEach((c, i) => {
    const p = parseCellId(c.cell);
    const b = p ? cellBounds(p.col, p.row) : null;
    const layer = join(root, DEBUG_LAYERS, layerOfSlug(c.slug));
    const n = existsSync(layer) ? (readFileSync(layer, "utf8").match(/^\S.* : "\{/gm) || []).length : 0;
    console.log(`  ${String.fromCharCode(97 + (i % 26))} ${c.slug.padEnd(28)} ${String(c.cell).padEnd(6)} ${b ? `x ${b.x0}..${b.x1} z ${b.z0}..${b.z1}` : "bad cell"}  ${n} entities, ${c.rows ?? 0}/3 rows${c.error ? `  ERROR ${c.error}` : ""}`);
  });
  console.log(mapText(claims, ents));
  console.log("  (# blocked by legacy stations or reserved ground, . free, letters as listed)");
  return 0;
}

function free(root, slug) {
  if (!slug) return usage();
  const reg = join(root, CELLS_DIR, `${slug}.json`);
  const layer = join(root, DEBUG_LAYERS, layerOfSlug(slug));
  if (!existsSync(reg) && !existsSync(layer)) {
    console.log(`grid: ${slug} has no cell`);
    return 1;
  }
  for (const f of [reg, layer]) if (existsSync(f)) unlinkSync(f);
  console.log(`grid: freed ${slug} (deleted ${CELLS_DIR}/${slug}.json and ${layerOfSlug(slug)})`);
  return 0;
}

async function place(root, args, flags) {
  const [slug, ...prefabs] = args;
  if (!slug || !SLUG_RX.test(slug)) {
    console.log(`grid: the slug must be lower-case letters, digits and dashes (2 to 41), like police-gates; got ${slug ?? "nothing"}`);
    return 1;
  }
  const P = await import("../place/place.mjs");
  const claims = readCells(root);
  const regFile = join(root, CELLS_DIR, `${slug}.json`);
  const layerName = layerOfSlug(slug);
  const layerFile = join(root, DEBUG_LAYERS, layerName);
  let entry = claims.find((c) => c.slug === slug) ?? null;
  const ents = loadWorld(root);
  let fresh = false;
  if (!entry) {
    const taken = new Set(claims.map((c) => c.cell));
    let cell;
    if (flags.cell) {
      const p = parseCellId(flags.cell);
      if (!p) throw new Error(`--cell ${flags.cell} is not a cell (c0r0 to c6r6)`);
      cell = allCells().find((c) => c.col === p.col && c.row === p.row);
      const why = [...(taken.has(cell.id) ? ["claimed"] : []), ...blockers(cell, ents)];
      if (why.length) {
        console.log(`grid: cell ${cell.id} is not free: ${why.join("; ")}`);
        return 1;
      }
    } else cell = allocate(slug, { ents, taken });
    if (!cell) {
      console.log("grid: no free cell left; free one with tools\\cli grid free <slug> (grid list shows them)");
      return 1;
    }
    entry = { slug, cell: cell.id, layer: layerName, rows: 0, created: new Date().toISOString().slice(0, 10) };
    fresh = true;
  }
  const p = parseCellId(entry.cell);
  const cell = { id: entry.cell, ...p, ...cellBounds(p.col, p.row) };
  console.log(`grid: ${slug} -> cell ${cell.id} (x ${cell.x0}..${cell.x1}, z ${cell.z0}..${cell.z1}, gap ${yardPlan(cell).gap.side})${fresh ? " (new)" : ""}`);
  if (flags.dry) {
    if (fresh) console.log(`  would write ${layerName} (7 fence runs and the label sign) and ${CELLS_DIR}/${slug}.json`);
    const line = prefabs.length ? rowLine(cell, entry.rows ?? 0) : null;
    if (line) console.log(`  would place ${prefabs.length} prop(s) in row ${entry.rows ?? 0}: ${line.a.join(",")} to ${line.b.join(",")}, facing ${line.facing}`);
    console.log("grid: dry run, nothing written.");
    return 0;
  }
  if (fresh || !existsSync(layerFile)) writeFileSync(layerFile, yardLayer(slug, cell, P));
  if (fresh) {
    mkdirSync(join(root, CELLS_DIR), { recursive: true });
    writeFileSync(regFile, registryText(entry));
    console.log(`  wrote ${layerName} (yard and sign ${entityPrefix(slug)}_Sign) and ${CELLS_DIR}/${slug}.json`);
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
  console.log("usage: tools\\cli grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--dry] [--no-verify]\n       tools\\cli grid list\n       tools\\cli grid free <slug>\n(docs/architecture/world-zones.md)");
  return 1;
}

export async function main(argv, { root }) {
  const [sub, ...rest] = argv;
  const { _: args, flags } = parse(rest);
  try {
    if (sub === "place") return await place(root, args, flags);
    if (sub === "list") return list(root);
    if (sub === "free") return free(root, args[0]);
  } catch (e) {
    console.log(`grid: ${e.message}`);
    return 1;
  }
  return usage();
}
