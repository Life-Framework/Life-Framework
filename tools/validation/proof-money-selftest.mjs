// tools/validation/proof-money-selftest.mjs
// inputs: agent-box/**, tools/**
//
// The one money reader for proofs (#5163): agent-box/proofs/_money.mjs. LF_MoneyFormat.FormatMoney prints "$1,040" and
// "-$1,040"; a proof regex `\$(\d+)` read that as 1 and the export-crane proof compared a paid $1 with a cash move of
// 1040 (RC dry run 6, 2026-11-27). This selftest checks the helper on every shape the game prints, runs each proof's
// own money parser on a comma amount, and fails any agent-box or tools source that still reads an amount with a bare
// `\$(\d+)`-style regex (a deliberate raw-digit read is marked `// lf-money-ok: <why>` on its line).
// red-proof: 2026-11-27, with `AMOUNT` back to `\d+` in _money.mjs this printed "FAIL parseMoney reads $1,040" and the
// proof parsers' comma cases, and exited 1; restored, it passes.
import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { AMOUNT, MONEY, allMoney, firstMoney, parseMoney } from "../../agent-box/proofs/_money.mjs";
import { firstPrice } from "../../agent-box/proofs/config-hotfix-proof.mjs";
import { parseEconomy as parseDriveEconomy } from "../../agent-box/proofs/drive-haul-proof.mjs";
import { parseEconomy as parseLumberEconomy } from "../../agent-box/proofs/lumber-arc.mjs";
import { parseEconomy as parseTruckEconomy } from "../../agent-box/proofs/truck-haul-proof.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// The helper, on every shape the game prints.
ok(parseMoney("$1,040") === 1040, "parseMoney reads $1,040");
ok(parseMoney("-$1,040") === -1040, "parseMoney reads -$1,040");
ok(parseMoney("+$1,040") === 1040, "parseMoney reads +$1,040");
ok(parseMoney("$1040") === 1040, "parseMoney reads a raw $1040");
ok(parseMoney("$12,345,678") === 12345678, "parseMoney reads two commas");
ok(parseMoney("-$150") === -150, "parseMoney reads -$150");
ok(parseMoney("$0") === 0, "parseMoney reads $0");
ok(parseMoney("1,040") === 1040, "parseMoney reads bare 1,040");
ok(Number.isNaN(parseMoney("none")), "parseMoney of no amount is NaN");
ok(Number.isNaN(parseMoney(undefined)), "parseMoney of undefined is NaN");
ok(firstMoney("Pays $1,040 now, $5 later") === 1040, "firstMoney takes the first amount whole");
ok(firstMoney("no price") === null, "firstMoney with no amount is null");
ok(JSON.stringify(allMoney("+$1,040 then -$2,500 and $7,")) === JSON.stringify([1040, -2500, 7]), "allMoney lists each amount, a trailing comma is not part of it");

const crane = "export order handed in by abc-123: $1,040";
const m = new RegExp(String.raw`export order handed in by \S+: (${MONEY})`).exec(crane);
ok(m && parseMoney(m[1]) === 1040, "the export crane line reads $1,040 (it read $1)");
ok(new RegExp(String.raw`fine=\$(${AMOUNT})`).exec("booking offer: x fine=$1,250 payable=1")?.[1] === "1,250", "AMOUNT captures the whole 1,250");

// Each proof's own parser, on the comma form and the raw form.
for (const [name, parse, row] of [
  ["drive-haul", parseDriveEconomy, "LOOP truck hardwood -> boards, firewood: ~40 logs/h, steady %S/h, first hour %F/h; duty x"],
  ["truck-haul", parseTruckEconomy, "LOOP truck hardwood -> boards, firewood: ~40 logs/h, steady %S/h, first hour %F/h; duty x"],
  ["lumber-arc", parseLumberEconomy, "LOOP hand-haul softwood -> planks: ~40 logs/h, steady %S/h, first hour %F/h; duty x"],
]) {
  const comma = parse(row.replace("%S", "$1,040").replace("%F", "$1,180"));
  ok(comma && comma.steady === 1040 && comma.first === 1180, `${name} parseEconomy reads $1,040 and $1,180`);
  const raw = parse(row.replace("%S", "$680").replace("%F", "$680"));
  ok(raw && raw.steady === 680 && raw.first === 680, `${name} parseEconomy still reads $680`);
}
ok(firstPrice("Water $1,040") === 1040, "config-hotfix firstPrice reads $1,040");
ok(firstPrice("Water $ 5") === 5, "config-hotfix firstPrice reads $ 5");
ok(firstPrice("Water") === -1, "config-hotfix firstPrice of no price is -1");

// No source reads an amount with a bare digit run after a "$".
const BARE = /\\\$\s?\(?\\d/;
const SKIP = new Set(["node_modules", ".git", "tmp", "reports", "fixtures"]);
function* sources(dir) {
  for (const name of readdirSync(dir)) {
    if (SKIP.has(name)) continue;
    const p = join(dir, name);
    const s = statSync(p);
    if (s.isDirectory()) yield* sources(p);
    else if (/\.mjs$/.test(name)) yield p;
  }
}
const self = join(root, "tools", "validation", "proof-money-selftest.mjs");
const helper = join(root, "agent-box", "proofs", "_money.mjs");
for (const dir of [join(root, "agent-box"), join(root, "tools")]) {
  for (const file of sources(dir)) {
    if (file === self || file === helper) continue;
    readFileSync(file, "utf8").split("\n").forEach((line, i) => {
      if (BARE.test(line) && !/lf-money-ok:\s*\S/.test(line)) {
        ok(false, `${relative(root, file).replace(/\\/g, "/")}:${i + 1} reads a "$" amount with a bare \\d: use AMOUNT/MONEY/parseMoney from agent-box/proofs/_money.mjs (or mark \`// lf-money-ok: <why>\`)`);
      }
    });
  }
}

if (failed) {
  console.log(`proof-money-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("proof-money-selftest: ok");
