// tools\cli livery: turn the livery table (tools/livery/table.mjs) into repainted variant prefabs in the pack.
//
//   livery list                          the rows, and whether each one's files are already written
//   livery build [<Vehicle_Id>...]       write each row's body material, variant prefab, door parts (UAZ469)
//          [--dry-run]                   print what would be written, write nothing
//
// Colours only go in the paint. Words are a painted plate on each front door (row `plate`): build writes
// its pack sign row, adds its string to the pack table (addons/LF_Everon1989/Language/) and runs
// `tools\cli regen-localization`.
//
// Every row carries a credit; build adds its row under "## Liveries" in docs/credits.md when it is missing.
// The generator is tools/livery/generate.mjs (pure, proven by tools/validation/livery-selftest.mjs).
// Method and limits: docs/architecture/livery-spike.md.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";

export const help = "livery list | livery build [<Vehicle_Id>...] [--dry-run]   repainted variant prefabs, door plates and credit rows from tools/livery/table.mjs";

const CREDITS_HEAD = "## Liveries";
const CREDITS_TABLE = "| Asset | Source and license | What it is for | Where |\n|---|---|---|---|";

export function addCreditRows(text, rows) {
  const missing = rows.filter((r) => !text.includes(r.split(" | ")[0] + " |"));
  if (!missing.length) return text;
  if (!text.includes(`\n${CREDITS_HEAD}\n`)) {
    return `${text.replace(/\n*$/, "\n")}\n${CREDITS_HEAD}\n\nRepainted vehicle bodies and door plates written by \`tools\\cli livery build\`.\n\n${CREDITS_TABLE}\n${missing.join("\n")}\n`;
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

// Append the plate strings the pack table lacks, before its closing braces; regen-localization sorts it.
export function addStrings(st, items, stringItem) {
  const missing = items.filter((s) => !st.includes(`Id "${s.id}"`));
  if (!missing.length) return st;
  const at = st.lastIndexOf("\n }\n}");
  if (at < 0) throw new Error("the pack string table has no closing braces");
  return st.slice(0, at + 1) + missing.map(stringItem).join("") + st.slice(at + 1);
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
      console.log(`  ${p.id.padEnd(22)} ${String(p.files.length).padStart(2)} files, ${have} written${p.strings.length ? ", door plates" : ""}`);
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
  }
  const stPath = join(root, gen.PACK, "Language", "lf_everon1989_localization.st");
  const st = readFileSync(stPath, "utf8");
  const st2 = addStrings(st, plans.flatMap((p) => p.strings), gen.stringItem);
  if (st2 !== st) {
    if (dry) console.log("strings: would add plate strings to the pack table");
    else {
      writeFileSync(stPath, st2);
      const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "regen-localization"], { cwd: root, encoding: "utf8", windowsHide: true, timeout: 300000 });
      console.log(`strings: plate strings added to the pack table; regen-localization exit ${r.status}`);
      if (r.status !== 0) return 1;
    }
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
