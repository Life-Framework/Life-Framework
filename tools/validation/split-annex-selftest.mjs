// Proves tools/grid/split-annex.mjs (docs/architecture/three-worlds.md): --copy keeps each entity's
// text byte for byte, a wholly-inside layer comes out identical to its source, Crimes splits 2 inside
// and 7 outside, Shops stays in town, and --strip deletes or rewrites in a scratch copy. Warns when
// the committed GridWorld layers differ from what --copy writes today. Does not boot the game.
// inputs: tools/grid/split-annex.mjs, tools/lib/worlds.mjs, tools/lib/world-zones.mjs, tools/validation/split-annex-selftest.mjs, addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers, addons/LifeFramework/Worlds/GridWorld
//
// red-proof: 2026-11-16, each break run through LF_SPLIT_ANNEX_MODULE=<broken copy> (15, 13, 2 failures):
//   isInside always false                  -> FAIL "Crimes splits 2 inside, 7 outside (got ... inside 0, outside 9)";
//   a block ends on an indented "}" too     -> FAIL "Annex is 130 entities, all inside", "Annex.layer is copied byte for byte";
//   blank lines between blocks dropped     -> FAIL "Crimes inside + outside together keep every byte".
import { existsSync, mkdtempSync, readFileSync, readdirSync, cpSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const modPath = process.env.LF_SPLIT_ANNEX_MODULE || join(here, "..", "grid", "split-annex.mjs");
const sa = await import(pathToFileURL(modPath).href);
const worlds = await import(pathToFileURL(join(here, "..", "lib", "worlds.mjs")).href);

const failures = [];
const check = (ok, what) => {
  if (!ok) failures.push(what);
};

// 1. The parser on a sample: nested braces, a blank line between blocks, an entity with no coords.
const sample = [
  "GenericEntity In_A {",
  " components {",
  "  X {",
  "  }",
  " }",
  " coords 30 0 40",
  "}",
  "",
  "GenericEntity Out_B : \"{0000000000000001}P.et\" {",
  " coords 200 0 300",
  "}",
  "SCR_X {",
  "}",
  "",
].join("\n");
const p = sa.parseLayer(sample);
check(p.blocks.length === 3, `a block that nests braces parses as one entity (got ${p.blocks.length} blocks)`);
check(p.blocks[0]?.name === "In_A" && p.blocks[1]?.name === "Out_B", "entity names are read from the head line");
check(p.blocks[2]?.coords === null && !sa.isInside(p.blocks[2] || {}), "an entity with no coords is outside");
const s = sa.splitLayer(sample);
check(s.counts.inside === 1 && s.counts.outside === 2, "the sample splits 1 inside, 2 outside");
check(s.inside === p.blocks[0]?.text, "an inside entity's text is copied unchanged");

// 2. The real DebugWorld layers into a scratch folder.
const src = worlds.playtestLayerDirs(root)[0];
const dst = mkdtempSync(join(tmpdir(), "split-annex-"));
const plan = sa.copyAnnex(src, dst);
const byFile = Object.fromEntries(plan.map((x) => [x.file, x]));
check(byFile["Crimes.layer"]?.inside === 2 && byFile["Crimes.layer"]?.outside === 7, `Crimes splits 2 inside, 7 outside (got ${JSON.stringify(byFile["Crimes.layer"])})`);
check(byFile["Annex.layer"]?.inside === 130 && byFile["Annex.layer"]?.outside === 0, "Annex is 130 entities, all inside");
check(byFile["Shops.layer"]?.inside === 0 && !existsSync(join(dst, "Shops.layer")), "Shops stays in town and is not copied");
for (const f of ["default.layer", "Gamemode.layer", "Factions.layer", "Signs.layer", "Street_Bank.layer"]) {
  check(!existsSync(join(dst, f)), `${f} is not copied`);
}
for (const f of ["Annex", "HousesKeys", "Jobs", "NPCs", "PRFeatures", "Police", "Tests", "WhitelistChat"]) {
  const a = existsSync(join(dst, `${f}.layer`)) ? readFileSync(join(dst, `${f}.layer`)) : null;
  check(a && a.equals(readFileSync(join(src, `${f}.layer`))), `${f}.layer is copied byte for byte`);
}
const crimesSrc = readFileSync(join(src, "Crimes.layer"), "utf8");
const crimesIn = existsSync(join(dst, "Crimes.layer")) ? readFileSync(join(dst, "Crimes.layer"), "utf8") : "";
const crimesNames = [...crimesIn.matchAll(/^\w+ (\w+)/gm)].map((m) => m[1]);
check(crimesNames.includes("CrimeScene"), "CrimeScene is one of the Crimes entities copied");
check(!crimesIn.includes("FenceNpc"), "FenceNpc (town) is not copied");
for (const b of sa.parseLayer(crimesIn).blocks) check(crimesSrc.includes(b.text), `Crimes entity ${b.name || b.cls} is the source text byte for byte`);
const sc = sa.splitLayer(crimesSrc);
check((sc.inside || "").length + (sc.outside || "").length === crimesSrc.length, "Crimes inside + outside together keep every byte");

// 3. --strip on a scratch copy of DebugWorld's layers.
const strip = mkdtempSync(join(tmpdir(), "split-annex-strip-"));
cpSync(src, strip, { recursive: true });
sa.stripAnnex(strip);
check(!existsSync(join(strip, "Annex.layer")) && !existsSync(join(strip, "Tests.layer")), "--strip deletes a wholly-inside layer");
check(existsSync(join(strip, "Shops.layer")) && existsSync(join(strip, "default.layer")), "--strip keeps town and system layers");
const crimesLeft = existsSync(join(strip, "Crimes.layer")) ? readFileSync(join(strip, "Crimes.layer"), "utf8") : "";
check(sa.parseLayer(crimesLeft).blocks.length === 7 && crimesLeft.includes("FenceNpc") && !crimesLeft.includes("CrimeScene"), "--strip leaves Crimes with its 7 town entities");

// 4. The committed GridWorld layers are what --copy writes from today's DebugWorld. A warning, not a
// failure: an annex edit in DebugWorld before W3's guard must not block an unrelated PR.
const grid = worlds.gridLayersDir(root);
for (const f of readdirSync(dst)) {
  const g = join(grid, f);
  if (!(existsSync(g) && readFileSync(g).equals(readFileSync(join(dst, f))))) console.log(`WARN  GridWorld ${f} differs from split-annex --copy (node tools/grid/split-annex.mjs --copy)`);
}

for (const f of failures) console.log(`FAIL  ${f}`);
console.log(failures.length ? `split-annex-selftest: ${failures.length} failure(s)` : "split-annex-selftest: OK");
process.exit(failures.length ? 1 : 0);
