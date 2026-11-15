// tools/gen-layout-keywords.mjs (a generator, kept out of tools/validation/ so validate never runs it and CI never needs the game)
//
// Writes tools/validation/layout-keywords.json: every property keyword the base game's own .layout
// files write directly on each widget class, read from the installed game paks. The CI runner has no
// game install, so validate-layout-keywords.mjs checks against this file. Run it again after a game
// update: node tools/gen-layout-keywords.mjs

import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildStamp, openVfs } from "./lib/install-api.mjs";
import { widgetKeys } from "./lib/layout-keywords.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const OUT = join(ROOT, "tools", "validation", "layout-keywords.json");

const vfs = await openVfs();
const byClass = {};
let files = 0;
for (const p of vfs.allFilePaths()) {
  if (!p.toLowerCase().endsWith(".layout")) continue;
  let text;
  try { text = vfs.readTextFile(p); } catch { continue; }
  files++;
  for (const { cls, key } of widgetKeys(text)) (byClass[cls] ||= new Set()).add(key);
}
const classes = {};
for (const cls of Object.keys(byClass).sort()) classes[cls] = [...byClass[cls]].sort();
const doc = { source: `vanilla .layout files in the installed game paks, ${buildStamp().text}`, files, classes };
writeFileSync(OUT, JSON.stringify(doc, null, 1) + "\n");
console.log(`gen-layout-keywords: ${files} vanilla layouts, ${Object.keys(classes).length} widget classes -> ${OUT}`);
