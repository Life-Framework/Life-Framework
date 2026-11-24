// No script calls the engine's GetDepositItemCountByResource (#4824, #4870).
// inputs: addons/**/Scripts/**/*.c, tools/lib/test-resetall.mjs, tools/validation/validate-no-deposit-count.mjs
//
// LF_InventoryUtils.GetAmount used it as a fallback when FindItems found no stack. On a client whose last
// stack had just been deleted (paying a fine at the booking desk, depositing all cash at an ATM) it answered
// -1 and the engine then held the game thread for 15-30 s: every player who paid out their last cash froze.
// Counting goes through LF_InventoryUtils.GetAmount (FindItemsByPrefab plus CarriedAmount). Comments and
// strings that name the call are fine.
//
// LF_DEPOSIT_COUNT_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-24, no-deposit-count-selftest.mjs runs this over a tree holding a script that calls it;
// it fails with `FAIL addons/LifeFramework/Scripts/Game/Feature/LF_Bad.c:3` and passes once the call is gone.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { stripNonCode } from "../lib/test-resetall.mjs";

export function depositCountLines(text) {
  const lines = [];
  stripNonCode(text).split("\n").forEach((line, i) => {
    if (/\bGetDepositItemCountByResource\s*\(/.test(line)) lines.push(i + 1);
  });
  return lines;
}

function scripts(dir, rel, out) {
  for (const name of readdirSync(dir)) {
    const full = join(dir, name);
    if (statSync(full).isDirectory()) scripts(full, `${rel}/${name}`, out);
    else if (name.endsWith(".c")) out.push(`${rel}/${name}`);
  }
}

function main() {
  const root = process.env.LF_DEPOSIT_COUNT_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = [];
  const addons = join(root, "addons");
  for (const addon of existsSync(addons) ? readdirSync(addons) : []) {
    const dir = join(addons, addon, "Scripts");
    if (existsSync(dir)) scripts(dir, `addons/${addon}/Scripts`, files);
  }

  const bad = [];
  for (const f of files) {
    for (const line of depositCountLines(readFileSync(join(root, f), "utf8"))) bad.push(`FAIL ${f}:${line} GetDepositItemCountByResource`);
  }

  if (bad.length) {
    console.log(bad.join("\n"));
    console.log(`validate-no-deposit-count: ${bad.length} call(s). Count items with LF_InventoryUtils.GetAmount: the engine count froze a client for 15-30 s when its last stack went (#4824).`);
    process.exit(1);
  }
  console.log(`validate-no-deposit-count: OK (${files.length} script file(s))`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
