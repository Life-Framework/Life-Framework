// tools/validation/validate-pack-sign-cards.mjs
// inputs: addons/LF_Everon1989/Configs/Signs/**, addons/LF_Everon1989/Language/lf_everon1989_localization.st
//
// The flagship pack's sign overlay adds two cards every newcomer should read (#1279): the server
// rules ("rules") and "Run your own" ("run_your_own"). Each must be an `add` record with a title and a
// What section whose #keys exist in the pack string table with non-empty text. A core test cannot do
// this (the core never names the pack, validate-addon-boundary), so the pack check lives here.
// red-proof: with the two records absent from LF_Signs_Everon1989.conf this check fails with
// "pack sign overlay has no 'rules' card" and "... no 'run_your_own' card" (2026-11-14, #1279).

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const CARDS = ["rules", "run_your_own"];

const conf = readFileSync(join(PACK, "Configs", "Signs", "LF_Signs_Everon1989.conf"), "utf8");
const table = readFileSync(join(PACK, "Language", "lf_everon1989_localization.st"), "utf8");

const text = new Map();
for (const m of table.matchAll(/Id "([^"]+)"\s+Target_en_us "([^"]*)"/g)) text.set(m[1], m[2]);

const errors = [];
const records = conf.split(/LF_SignDef "\{/).slice(1);
for (const id of CARDS) {
  const rec = records.find((r) => r.includes(`m_sId "${id}"`));
  if (!rec) { errors.push(`pack sign overlay has no '${id}' card`); continue; }
  if (!/m_sOp "add"/.test(rec)) errors.push(`'${id}' card is not an add record`);
  for (const field of ["m_sTitle", "m_sWhat", "m_sNext"]) {
    const m = rec.match(new RegExp(`${field} "([^"]*)"`));
    if (!m || !m[1]) { errors.push(`'${id}' card has no ${field}`); continue; }
    const key = m[1].replace(/^#/, "");
    if (m[1].startsWith("#") && !(text.get(key) || "").trim()) errors.push(`'${id}' ${field} key ${m[1]} has no text in the pack string table`);
  }
}

if (errors.length) {
  for (const e of errors) console.error("FAIL " + e);
  console.error(`validate-pack-sign-cards: ${errors.length} problem(s)`);
  process.exit(1);
}
console.log("validate-pack-sign-cards: OK (rules and run_your_own cards present with text)");
