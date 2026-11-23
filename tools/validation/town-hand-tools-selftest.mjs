// Selftest for validate-town-hand-tools.mjs (#4156). No game, no git.
// inputs: tools/validation/validate-town-hand-tools.mjs, tools/lib/world-zones.mjs
// red-proof: removing the `zoneOfLayer(...) !== "town"` skip in scan() makes the grid drill count and
// this exits 1; dropping the inheritance loop in handToolGuids() loses the child drill and it exits 1.
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { DEBUG_LAYERS } from "../lib/world-zones.mjs";
import { handToolGuids, looseTools, scan } from "./validate-town-hand-tools.mjs";

const fails = [];
const expect = (cond, what) => { if (!cond) fails.push(what); };

const prefabs = [
  { guid: "AAAAAAAAAAAAAAAA", parent: "8B993DEBBF41116C", text: 'GenericEntity : "{8B993DEBBF41116C}x.et" {\n components {\n  LF_HandToolComponent "{1}" {\n  }\n }\n}' },
  { guid: "BBBBBBBBBBBBBBBB", parent: "AAAAAAAAAAAAAAAA", text: 'GenericEntity : "{AAAAAAAAAAAAAAAA}Prefabs/Items/Fence/LF_Drill.et" {\n}' },
  { guid: "CCCCCCCCCCCCCCCC", parent: "8B993DEBBF41116C", text: 'GenericEntity : "{8B993DEBBF41116C}x.et" {\n}' },
];
const tools = handToolGuids(prefabs);
expect(tools.has("AAAAAAAAAAAAAAAA") && tools.has("BBBBBBBBBBBBBBBB"), "a hand tool and its child are hand tools");
expect(!tools.has("CCCCCCCCCCCCCCCC"), "a plain item is not a hand tool");
expect(looseTools('GenericEntity Shelf_Item : "{CCCCCCCCCCCCCCCC}Prefabs/Items/A.et" {\n}', tools).length === 0, "a plain item is not reported");

const root = scratchDir("lf-town-hand-tools-");
const layers = join(root, DEBUG_LAYERS);
mkdirSync(layers, { recursive: true });
writeFileSync(join(layers, "Street_Bank.layer"), 'GenericEntity Photo_Drill : "{BBBBBBBBBBBBBBBB}Prefabs/Items/Fence/LF_Drill_Child.et" {\n coords 1 0 1\n}\n');
writeFileSync(join(layers, "Grid_bank_heist.layer"), 'GenericEntity Grid_Drill : "{AAAAAAAAAAAAAAAA}Prefabs/Items/Fence/LF_Drill.et" {\n coords 1 0 1\n}\n');
const found = scan(root, tools);
expect(found.length === 1 && found[0].name === "Photo_Drill", `only the town drill is reported (got ${JSON.stringify(found.map((f) => f.name))})`);

for (const f of fails) console.log(`FAIL ${f}`);
console.log(fails.length ? "town-hand-tools-selftest: FAIL" : "town-hand-tools-selftest: OK (town drill caught, grid drill and plain item pass, inherited tools found)");
process.exit(fails.length ? 1 : 0);
