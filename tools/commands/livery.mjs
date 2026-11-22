// tools\cli livery: turn the livery table (tools/livery/table.mjs) into repainted variant prefabs in the pack.
//
//   livery list                          the rows, and whether each one's files are already written
//   livery build [<Vehicle_Id>...]       write each row's body material, variant prefab, door parts (UAZ469)
//          [--dry-run]                   print what would be written, write nothing
//          [--mask]                      also build the lettering mask: unpack the vanilla mask (edds.mjs),
//                                        stamp the spec in Blender (tools/livery/spike/mask.py), import it
//                                        through Workbench (`tools\cli ui-image --batch`)
//
// Every row carries a credit; build adds its row under "## Liveries" in docs/credits.md when it is missing.
// The generator is tools/livery/generate.mjs (pure, proven by tools/validation/livery-selftest.mjs).
// Method and limits: docs/architecture/livery-spike.md.
import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";

export const help = "livery list | livery build [<Vehicle_Id>...] [--dry-run] [--mask]   repainted variant prefabs and credit rows from tools/livery/table.mjs";

const CREDITS_HEAD = "## Liveries";
const CREDITS_TABLE = "| Asset | Source and license | What it is for | Where |\n|---|---|---|---|";

export function addCreditRows(text, rows) {
  const missing = rows.filter((r) => !text.includes(r.split(" | ")[0] + " |"));
  if (!missing.length) return text;
  if (!text.includes(`\n${CREDITS_HEAD}\n`)) {
    return `${text.replace(/\n*$/, "\n")}\n${CREDITS_HEAD}\n\nRepainted vehicle bodies and lettering masks written by \`tools\\cli livery build\`.\n\n${CREDITS_TABLE}\n${missing.join("\n")}\n`;
  }
  // Append after the last table row of the Liveries section.
  const start = text.indexOf(`\n${CREDITS_HEAD}\n`) + 1;
  const next = text.indexOf("\n## ", start + CREDITS_HEAD.length);
  const end = next < 0 ? text.length : next + 1;
  const section = text.slice(start, end).replace(/\n*$/, "\n");
  return text.slice(0, start) + section + missing.join("\n") + "\n" + (next < 0 ? "" : "\n") + text.slice(end);
}

function pick(LIVERIES, names) {
  if (!names.length) return LIVERIES;
  const want = new Set(names.map((n) => n.toLowerCase()));
  const rows = LIVERIES.filter((r) => want.has(`${r.vehicle}_${r.id}`.toLowerCase()));
  if (rows.length !== want.size) throw new Error(`unknown livery in ${names.join(", ")} (known: ${LIVERIES.map((r) => `${r.vehicle}_${r.id}`).join(", ")})`);
  return rows;
}

async function buildMask(root, vfs, plan) {
  const { eddsToDds } = await import("../livery/edds.mjs");
  const { runHeadless } = await import("../lib/blender.mjs");
  const script = join(root, "tools", "livery", "spike", "mask.py");
  if (!existsSync(script)) throw new Error("tools/livery/spike/mask.py is missing (it lands with the spike follow-up, PR #4277)");
  const work = join(root, "tmp", "livery");
  mkdirSync(work, { recursive: true });
  const dds = join(work, `${plan.id}_base.dds`);
  writeFileSync(dds, eddsToDds(vfs.readFile(plan.mask.vanilla)).dds);
  const spec = join(work, `${plan.id}_spec.json`);
  writeFileSync(spec, JSON.stringify(plan.mask.spec));
  const tif = join(root, plan.mask.tif);
  mkdirSync(dirname(tif), { recursive: true });
  const r = runHeadless(script, [dds, tif, spec], { timeoutMs: 180000 });
  if (!/LF_MASK wrote/.test(r.stdout)) throw new Error(`mask.py wrote no mask: ${r.stdout.slice(-400)}`);
  writeFileSync(join(root, plan.mask.edds + ".meta"), plan.mask.meta);
  const edds = join(root, plan.mask.edds);
  if (existsSync(edds) && statSync(edds).mtimeMs < statSync(tif).mtimeMs) rmSync(edds);
  const list = join(work, `${plan.id}_batch.txt`);
  writeFileSync(list, plan.mask.tif + "\n");
  const imp = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "ui-image", "--batch", `tmp/livery/${plan.id}_batch.txt`], { cwd: root, encoding: "utf8", windowsHide: true, timeout: 600000 });
  if (!existsSync(edds)) throw new Error(`the import built no ${plan.mask.edds}: ${(imp.stdout || "").trim().split("\n").slice(-3).join(" / ")}`);
  console.log(`  mask {${plan.mask.guid}}${plan.mask.edds}`);
}

export async function main(args, { root }) {
  const sub = args[0];
  const names = args.slice(1).filter((a) => !a.startsWith("--"));
  const gen = await import("../livery/generate.mjs");
  const { LIVERIES } = await import("../livery/table.mjs");
  if (sub !== "list" && sub !== "build") {
    console.log(`usage: tools\\cli ${help}`);
    return 2;
  }
  const { gameVfs } = await import("../lib/game-prefab.mjs");
  const vfs = await gameVfs();
  const vanilla = (rel) => {
    const b = vfs.readFile(rel);
    if (!b) throw new Error(`vanilla file not in the game paks: ${rel}`);
    return b.toString("utf8");
  };
  let plans;
  try {
    plans = pick(LIVERIES, names).map((r) => gen.planLivery(r, vanilla));
    gen.checkUnique(LIVERIES.map((r) => gen.planLivery(r, vanilla)));
  } catch (e) {
    console.log(`FAIL  livery: ${e.message}`);
    return 1;
  }
  if (sub === "list") {
    for (const p of plans) {
      const have = p.files.filter((f) => existsSync(join(root, f.rel))).length;
      console.log(`  ${p.id.padEnd(22)} ${String(p.files.length).padStart(2)} files, ${have} written${p.mask ? ", lettering mask" : ""}`);
    }
    return 0;
  }
  const dry = args.includes("--dry-run");
  for (const p of plans) {
    console.log(`${p.id}:`);
    for (const f of p.files) {
      console.log(`  {${f.guid}}${f.rel}`);
      if (dry) continue;
      const abs = join(root, f.rel);
      mkdirSync(dirname(abs), { recursive: true });
      writeFileSync(abs, f.text);
      writeFileSync(abs + ".meta", gen.metaText(f));
    }
    if (p.mask && !dry && args.includes("--mask")) await buildMask(root, vfs, p);
    else if (p.mask) console.log(`  (lettering mask ${p.mask.edds}: run with --mask to build it)`);
  }
  const credits = join(root, "docs", "credits.md");
  const before = readFileSync(credits, "utf8");
  const after = addCreditRows(before, plans.map((p) => p.credit));
  if (after !== before) {
    if (dry) console.log("credits: would add rows to docs/credits.md");
    else { writeFileSync(credits, after); console.log("credits: rows added to docs/credits.md \"Liveries\""); }
  }
  console.log(`SUMMARY livery build ok rows=${plans.length}${dry ? " dry-run" : ""}`);
  return 0;
}
