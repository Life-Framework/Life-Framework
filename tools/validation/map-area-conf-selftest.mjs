// tools/validation/map-area-conf-selftest.mjs
// inputs: tools/map/areas-from-dossier.mjs, tools/map/lib/overlay.mjs, tools/map/lib/conf.mjs, addons/LifeFramework/Configs/Map/LF_MapAreas.conf, addons/LF_Everon1989/Configs/Map/LF_MapAreas_Everon1989.conf, docs/everon/places/saint-philippe/dossier.json
//
// The area conf (#2233 split M5b): the emulator reads the same LF_MapAreas*.conf the game does. Core
// holds no row; the pack's file is exactly what tools/map/areas-from-dossier.mjs makes from the
// Saint-Philippe dossier (so no coordinate is typed by hand); the loader returns the town line and the safe
// square; a pack row with the same Id replaces, op remove drops. The Enforce side is LF_Test_MapAreaConf and
// LF_Test_EveronMapAreas.
// red-proof: observed 2026-11-17: the pack file with the safe square's x moved one metre made this fail
// "is stale: run node tools/map/areas-from-dossier.mjs"; regenerated, it passes.
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { confOf, OUT, rowsOf } from "../map/areas-from-dossier.mjs";
import { parseConf, vecListXZ, vecXZ } from "../map/lib/conf.mjs";
import { loadAreas } from "../map/lib/overlay.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = false;
const fail = (m) => { console.log(`FAIL ${m}`); failed = true; };

// 1. The pack file is what the dossier makes; core is empty of rows.
const dossier = JSON.parse(readFileSync(join(ROOT, "docs/everon/places/saint-philippe/dossier.json"), "utf8"));
const want = confOf(rowsOf(dossier));
if (readFileSync(join(ROOT, OUT), "utf8") !== want) fail(`${OUT} is stale: run node tools/map/areas-from-dossier.mjs`);
const nudged = want.replace(/m_aPoints \{ (\d+\.\d) 0 /, (m, n) => `m_aPoints { ${Number(n) + 1} 0 `);
if (nudged === want) fail("selftest could not nudge a coordinate");
if (readFileSync(join(ROOT, OUT), "utf8") === nudged) fail("a nudged coordinate matched the file: the stale check cannot fail");
if (/LF_MapAreaRow/.test(readFileSync(join(ROOT, "addons/LifeFramework/Configs/Map/LF_MapAreas.conf"), "utf8"))) fail("the core area conf holds a row: core holds no coordinate");

// 2. The loader reads the real files: the town line and the safe square, no lawless area (D4).
const real = loadAreas(ROOT, () => "Safe zone");
const line = real.get("saint-philippe-town-line");
const square = real.get("saint-philippe-square-safe");
if (!line || line.style !== "TOWN" || line.shape !== "POLYGON" || !(line.points?.length >= 3)) fail(`the town line did not load as a polygon: ${JSON.stringify(line)}`);
if (!square || square.style !== "SAFE" || square.shape !== "CIRCLE" || !(square.radius > 0)) fail(`the safe square did not load as a circle: ${JSON.stringify(square)}`);
if (square && square.name !== "Safe zone") fail(`the square's label is the style's own: ${square.name}`);
if ([...real.values()].some((a) => a.style === "LAWLESS")) fail("a lawless area is in the pack rows before M7 (D4)");
if ([...real.values()].some((a) => a.points && a.points.some((p) => !Number.isFinite(p[0]) || !Number.isFinite(p[1])))) fail("a polygon point did not parse");

// 3. The multi-line array form Workbench writes parses like the one-line form.
const multi = parseConf('LF_MapAreaConfig {\n m_aAreas {\n  LF_MapAreaRow "{0000000000000001}" {\n   m_sId "x"\n   m_aPoints {\n    "1 0 2"\n    "3 0 4"\n    "5 0 6"\n   }\n  }\n }\n}\n');
const row = multi.children[0]?.children[0];
if (!row || row.props.m_aPoints?.length !== 3) fail(`a multi-line m_aPoints did not parse as a prop: ${JSON.stringify(row?.props)}`);

// 3b. The engine's form (unquoted, a flat run of numbers) reads as the same points, and an unquoted centre.
{
  const flat = parseConf(`LF_MapAreaConfig {
 m_aAreas {
  LF_MapAreaRow "{0000000000000003}" {
   m_sId "f"
   m_aPoints { 1 0 2 3 0 4 5 0 6 }
   m_vCentre 7 0 8
  }
 }
}
`);
  const r = flat.children[0]?.children[0];
  const pts = vecListXZ(r?.props.m_aPoints);
  if (JSON.stringify(pts) !== "[[1,2],[3,4],[5,6]]") fail(`a flat m_aPoints did not read as three points: ${JSON.stringify(pts)}`);
  if (JSON.stringify(vecXZ(r?.props.m_vCentre)) !== "[7,8]") fail(`an unquoted m_vCentre did not read: ${JSON.stringify(r?.props.m_vCentre)}`);
  if (JSON.stringify(vecListXZ(['"1 0 2"'.replace(/"/g, ""), "3 0 4"])) !== "[[1,2],[3,4]]") fail("the quoted form no longer parses");
}

// 4. Layering: a later file's row with the same Id replaces, op remove drops.
const dir = mkdtempSync(join(tmpdir(), "lf-area-conf-"));
try {
  for (const d of ["addons/LifeFramework/Configs/Map", "addons/LF_Everon1989/Configs/Map"]) mkdirSync(join(dir, d), { recursive: true });
  const body = (id, extra) => `LF_MapAreaConfig {\n m_aAreas {\n  LF_MapAreaRow "{0000000000000002}" {\n   m_sId "${id}"\n   m_eStyle SAFE\n   m_eShape CIRCLE\n   m_vCentre "10 0 20"\n   m_fRadius ${extra}\n  }\n }\n}\n`;
  writeFileSync(join(dir, "addons/LifeFramework/Configs/Map/LF_MapAreas.conf"), body("a", 5));
  writeFileSync(join(dir, "addons/LF_Everon1989/Configs/Map/LF_MapAreas_P.conf"), body("a", 9));
  const merged = loadAreas(dir);
  if (merged.size !== 1 || merged.get("a")?.radius !== 9) fail(`a pack row did not replace the core row: ${JSON.stringify([...merged.values()])}`);
  writeFileSync(join(dir, "addons/LF_Everon1989/Configs/Map/LF_MapAreas_P.conf"), body("a", 9).replace('   m_eStyle', '   m_sOp "remove"\n   m_eStyle'));
  if (loadAreas(dir).size !== 0) fail("op remove did not drop the row");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) process.exit(1);
console.log("map-area-conf-selftest: ok");
