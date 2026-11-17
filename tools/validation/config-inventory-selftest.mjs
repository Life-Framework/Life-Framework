// Proves the config inventory (tools/config/inventory.mjs) is deterministic, that a new
// constant is exactly one new row, and that a stale doc names `regen-config-inventory`.
// inputs: tools/**
//
// red-proof: before tools/commands/regen-config-inventory.mjs and the exported
// staleMessage/checkDoc existed, this file failed to import (`buildInventory`/`checkDoc`
// were missing) and the stale message named `node tools/config/inventory.mjs --write`; with
// the new code it prints "config-inventory-selftest: OK".
import { existsSync, mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { buildInventory, render, checkDoc, writeDoc, staleMessage } from "../config/inventory.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const dir = scratchDir("lf-config-inventory-");
{
  const addon = join(dir, "addon");
  const feature = join(addon, "Scripts", "Game", "Feature");
  mkdirSync(join(feature, "Jobs"), { recursive: true });
  mkdirSync(join(feature, "Shop"), { recursive: true });
  mkdirSync(join(addon, "Configs"), { recursive: true });
  // Written out of order on purpose: the output must not depend on the order files are made.
  writeFileSync(join(feature, "Shop", "LF_ShopRules.c"), "class LF_ShopRules\n{\n\tstatic const int MAX_STOCK = 20;\n\tstatic const float RESTOCK_INTERVAL_SEC = 30.0;\n}\n");
  writeFileSync(join(feature, "Jobs", "LF_JobRules.c"), "class LF_JobRules\n{\n\tstatic const int WAGE_PER_JOB = 50;\n\tstatic const int SAVE_VERSION = 2;\n}\n");
  const opts = { addonDir: addon, featureDir: feature };

  const first = render(buildInventory(opts));
  const second = render(buildInventory(opts));
  expect(first === second, "two runs on the same tree must be byte-identical");
  expect(!/\d{4}-\d{2}-\d{2}|\d{2}:\d{2}:\d{2}/.test(first), "the output must hold no timestamp");

  const rows = (text) => text.split("\n").filter((l) => l.startsWith("| `") && l.includes("lf_") === false);
  const constRows = (text) => text.split("\n").filter((l) => /^\| `[\w/]+\.c` \| `\w+` \|/.test(l));
  expect(constRows(first).length === 3, `expected 3 tunable rows, got ${constRows(first).length}`);
  const sorted = [...constRows(first)].sort();
  expect(JSON.stringify(sorted) === JSON.stringify(constRows(first)), "constant rows must be sorted (file, constant)");

  // Add one constant: exactly one new row, every other row unchanged.
  writeFileSync(join(feature, "Jobs", "LF_JobRules.c"), "class LF_JobRules\n{\n\tstatic const int WAGE_PER_JOB = 50;\n\tstatic const int BONUS_FINE = 9;\n\tstatic const int SAVE_VERSION = 2;\n}\n");
  const third = render(buildInventory(opts));
  const before = new Set(constRows(first)), after = constRows(third);
  const added = after.filter((r) => !before.has(r));
  expect(after.length === before.size + 1, `adding a constant must add one row, rows ${before.size} -> ${after.length}`);
  expect(added.length === 1 && added[0].includes("`BONUS_FINE`"), `the one new row must be BONUS_FINE, got ${added}`);
  expect(rows(first).length >= 1, "sanity: the fixture renders rows");

  // The stale message names the new command, in the function and in the CLI's own output.
  expect(/regen-config-inventory/.test(staleMessage()), `stale message must name regen-config-inventory: ${staleMessage()}`);
  const doc = join(dir, "config-inventory.md");
  expect(checkDoc(buildInventory(opts), doc) !== null, "a missing doc must be stale");
  expect(/regen-config-inventory/.test(checkDoc(buildInventory(opts), doc) ?? ""), "checkDoc must return the message naming regen-config-inventory");
  writeDoc(buildInventory(opts), doc);
  expect(checkDoc(buildInventory(opts), doc) === null, "a freshly written doc must not be stale");
  writeFileSync(doc, readFileSync(doc, "utf8") + "stray\n");
  expect(checkDoc(buildInventory(opts), doc) !== null, "an edited doc must be stale");

  // `tools\cli help` lists every tools/commands/*.mjs that exports `help`, and cli dispatches any file there.
  const cmdFile = join(here, "..", "commands", "regen-config-inventory.mjs");
  expect(existsSync(cmdFile), "tools/commands/regen-config-inventory.mjs must exist");
  const mod = await import(pathToFileURL(cmdFile).href);
  expect(/^regen-config-inventory\s/.test(mod.help ?? ""), `the command must export a help line starting with its name, got ${mod.help}`);
  expect(typeof mod.main === "function", "the command must export main");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-inventory-selftest: OK (deterministic, one row per constant, stale message names the command)");
