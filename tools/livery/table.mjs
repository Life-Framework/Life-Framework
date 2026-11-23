// tools/livery/table.mjs - the livery table `tools\cli livery build` reads (WO-L2, #4232; one file per card, #4467).
//
// The table is every `tools/livery/rows/*.mjs`, in file-name order. A rows file is `export default [ row, ... ]`;
// a card adds its own file (police.mjs, ambulance.mjs, ...) and no list here changes. A file whose name starts
// with "_" or that does not end in .mjs is skipped.
//
// One row is one repainted variant:
//   id        PascalCase name (Police, Ambulance, ...); the files are LF_<Vehicle>_<id>
//   vehicle   a file name in tools/livery/vehicles/ (UAZ469, S105, S1203)
//   base      the {GUID}path prefab the variant inherits (the service prefab, or a vanilla variant)
//   colors    { 1: [r, g, b], 2: ..., 3: ..., 4: ... } paint layer colours, linear 0..1
//   enabled   { 2: false } turns a vanilla paint layer off (the S1203 base mask speckles under layer 2)
//   panels    { sign, paint, ink, rim, at: [{ name, coords, yaw, scale }] }: plates on fixed body panels, children of
//             the variant prefab (S105, S1203, UAZ452: doors that are part of the body mesh), #4559
//   plate     { text, paint, ink, rim }: a painted plate on each front door (UAZ469) reading text; words
//             never go in the paint (mask lettering scrambles across UV islands)
//   credit    { source, license, purpose }: required on every row; it becomes the docs/credits.md row
//
// Colours are what this server is, so the files go to the pack.
import { existsSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));

// Every rows file in `dir`, flattened in file-name order. A file that does not default-export an array names itself.
export async function loadLiveries(dir = join(HERE, "rows")) {
  if (!existsSync(dir)) throw new Error(`livery rows folder ${dir} is missing`);
  const rows = [];
  for (const name of readdirSync(dir).filter((n) => n.endsWith(".mjs") && !n.startsWith("_")).sort()) {
    const mod = await import(pathToFileURL(join(dir, name)).href);
    if (!Array.isArray(mod.default)) throw new Error(`${name}: a rows file must \`export default [ ...rows ]\``);
    rows.push(...mod.default);
  }
  return rows;
}

export const LIVERIES = await loadLiveries();
