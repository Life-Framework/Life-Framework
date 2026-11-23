// tools\cli credits [--check]: build the in-game credits screen's data from docs/credits.md (#481).
//
// docs/credits.md is the single source of the credits (the website /credits page and the README read it
// too). This writes two generated outputs, so the game cannot drift from it:
//   addons/LifeFramework/Language/everonlife_localization.st   the LF-Credits_* rows (then regen-localization)
//   addons/LifeFramework/Scripts/Game/Feature/Credits/LF_CreditsData.c   the ordered row list of string keys
// `--check` writes nothing and exits 1 when either differs from what the file would produce
// (validate-credits.mjs runs it). Rules: a `##` heading is a heading row; a table row is its first cell
// (label) and its Author, Licence or Tool cells (value); a bullet is a label; a paragraph counts only in a
// section with no table or bullet. Links, backticks and bold are stripped; a `(...)` after a table label is
// dropped (it is a repo path). Code blocks are skipped.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { DATA, SOURCE, ST, dataFile, entries, patchSt } from "../merge/credits-rows.mjs";

export { DATA, SOURCE, ST, dataFile, entries, parse, patchSt, stBlock } from "../merge/credits-rows.mjs";

export const help = "credits [--check]   generate the in-game credits rows and strings from docs/credits.md";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function read(p) {
  return existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : "";
}

export function expected() {
  const list = entries(read(SOURCE));
  return { list, st: patchSt(read(ST), list), data: dataFile(list) };
}

export async function main(args) {
  const { list, st, data } = expected();
  if (!list.length) { console.log(`credits: ${SOURCE} gave no rows`); return 1; }
  const stOk = st === read(ST);
  const dataOk = data === read(DATA);
  if (args.includes("--check")) {
    if (stOk && dataOk) { console.log(`credits: ${list.length} rows in sync with ${SOURCE}`); return 0; }
    console.log(`credits: ${[!stOk && ST, !dataOk && DATA].filter(Boolean).join(" and ")} out of date with ${SOURCE}; run tools\\cli credits and tools\\cli regen-localization`);
    return 1;
  }
  mkdirSync(dirname(join(ROOT, DATA)), { recursive: true });
  writeFileSync(join(ROOT, ST), st, "utf8");
  writeFileSync(join(ROOT, DATA), data, "utf8");
  console.log(`credits: wrote ${list.length} rows from ${SOURCE}; now run tools\\cli regen-localization`);
  return 0;
}
