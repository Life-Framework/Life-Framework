// tools/validation/validate-layout-keywords.mjs
// inputs: addons/**, tools/validation/layout-keywords.json, tools/lib/layout-keywords.mjs
//
// Every property keyword written directly on a widget in a .layout must be one the engine knows for
// that widget class. The engine reads keywords case-sensitively and stops at an unknown one with
// "GUI (E): Unknown keyword/data '<key>' at offset N", so the widget is lost: LF_LawlessTag.layout
// wrote `Visible 0` (vanilla: "Is Visible") and LF_UITitle.layout wrote "Horizontal alignment" on a
// TextWidget (vanilla: "Horizontal Alignment"; only EditBox spells it lower case). Both showed on
// every Workbench Play.
//
// The known keywords are every keyword the base game's own layouts use per class
// (tools/validation/layout-keywords.json, written by gen-layout-keywords.mjs from the paks, since CI
// has no game install). A keyword the engine accepts that vanilla never writes goes in EXTRA with
// the layout that proves it.
//
// red-proof: 2026-11-16, on main this printed
//   ERROR addons/LifeFramework/UI/Layouts/HUD/LF_LawlessTag.layout:14 TextWidgetClass has no keyword 'Visible' (did you mean 'Is Visible'?)
//   ERROR addons/LifeFramework/UI/Layouts/UIKit/LF_UITitle.layout:9 TextWidgetClass has no keyword 'Horizontal alignment' (did you mean 'Horizontal Alignment'?)
// and FAILED; with both layouts fixed, OK.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { widgetKeys } from "../lib/layout-keywords.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const DATA = join(ROOT, "tools", "validation", "layout-keywords.json");
const ADDONS = join(ROOT, "addons");

// Keywords the engine accepts that no vanilla layout writes: { class: [key, ...] }.
const EXTRA = {};

function walk(dir, out) {
  for (const name of readdirSync(dir)) {
    if (name === "log" || name === "WorkbenchGame" || name.startsWith(".")) continue;
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) walk(p, out);
    else if (name.endsWith(".layout")) out.push(p);
  }
  return out;
}

if (!existsSync(DATA)) {
  console.log("validate-layout-keywords: FAILED (tools/validation/layout-keywords.json is missing; run node tools/validation/gen-layout-keywords.mjs)");
  process.exit(1);
}
const known = JSON.parse(readFileSync(DATA, "utf8")).classes;
// A keyword vanilla writes on COMMON or more widget classes is a base Widget property (Opacity, the
// navigation rules): valid on every class, even one whose vanilla layouts never set it.
const COMMON = 5;
const uses = {};
for (const cls of Object.keys(known)) for (const k of known[cls]) uses[k] = (uses[k] || 0) + 1;
const base = Object.keys(uses).filter((k) => uses[k] >= COMMON);
const sets = {};
for (const cls of Object.keys(known)) sets[cls] = new Set([...known[cls], ...base, ...(EXTRA[cls] || [])]);

let bad = 0;
let checked = 0;
const files = walk(ADDONS, []);
for (const file of files) {
  const rel = relative(ROOT, file).replace(/\\/g, "/");
  for (const { cls, key, line } of widgetKeys(readFileSync(file, "utf8"))) {
    const set = sets[cls];
    if (!set) continue; // a class vanilla never writes: nothing to compare against
    checked++;
    if (set.has(key)) continue;
    const near = [...set].find((k) => k.toLowerCase() === key.toLowerCase() || k.toLowerCase() === ("is " + key).toLowerCase());
    console.log(`ERROR ${rel}:${line} ${cls} has no keyword '${key}'${near ? ` (did you mean '${near}'?)` : ""}; the engine stops reading the widget there ("Unknown keyword/data")`);
    bad++;
  }
}
if (bad) {
  console.log(`validate-layout-keywords: FAILED (${bad} unknown keyword(s) in ${files.length} layouts)`);
  process.exit(1);
}
console.log(`validate-layout-keywords: OK (${checked} widget keywords in ${files.length} layouts)`);
