// Selftest: the height-frame rule (tools/lib/height-frame.mjs), the check that enforces it (validate-layer-height-frame.mjs)
// and the writers that apply it (town fit's framedRows, the night-light lamps).
// inputs: tools/lib/height-frame.mjs, tools/validation/validate-layer-height-frame.mjs, tools/town/fit.mjs, tools/town/nightlights.mjs
//
// red-proof: 2026-11-21, with RELATIVE_Y set to 0x80 it printed "FAILED a Lamps_Base child is in the terrain frame (the flag is
// inherited)" and "FAILED a row's own Flags line decides over its prefab's" (fail 3); restored, OK. The check and writer cases take an
// injected frame, so they prove the conversions, not the bit.
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { frameOf, layerY, prefabFlags, RELATIVE_Y, rowFlags, worldYOf } from "../lib/height-frame.mjs";
import { wrongFrameRows } from "./validate-layer-height-frame.mjs";
import { framedRows } from "../town/fit.mjs";
import { frameRows } from "../town/nightlights.mjs";

let failed = 0;
const check = (ok, msg) => {
  if (!ok) {
    failed++;
    console.log(`FAILED ${msg}`);
  }
};

const root = scratchDir("lf-height-frame-");
const put = (rel, text) => {
  const f = join(root, "addons", "Fixture", ...rel.split("/"));
  mkdirSync(dirname(f), { recursive: true });
  writeFileSync(f, text);
};
put("Prefabs/Lamps_Base.et", 'GenericEntity {\n ID "1"\n Flags 0x80403\n}\n');
put("Prefabs/Lamp.et", 'GenericEntity : "{1111111111111111}Prefabs/Lamps_Base.et" {\n ID "2"\n}\n');
put("Prefabs/Props_Base.et", 'GenericEntity : "{1111111111111111}Prefabs/Lamps_Base.et" {\n ID "3"\n Flags 0x403\n}\n');
put("Prefabs/Bin.et", 'GenericEntity : "{3333333333333333}Prefabs/Props_Base.et" {\n ID "4"\n}\n');
put("Prefabs/Plain.et", 'GenericEntity {\n ID "5"\n}\n');
put("Prefabs/Orphan.et", 'GenericEntity : "{9999999999999999}Prefabs/Gone.et" {\n ID "6"\n}\n');

check(RELATIVE_Y === 0x80000, "RELATIVE_Y is 0x80000");
check(rowFlags(" Flags 0x80403 0\n coords 1 2 3\n") === 0x80403 && rowFlags(" coords 1 2 3\n") === null, "rowFlags reads a block's own Flags line, first value");
check(frameOf(root, null, "{2222222222222222}Prefabs/Lamp.et") === "terrain", "a Lamps_Base child is in the terrain frame (the flag is inherited)");
check(frameOf(root, null, "Prefabs/Bin.et") === "world", "a nearer Flags without the bit wins over the base's (a bin over Props_Base 0x403)");
check(frameOf(root, null, "Prefabs/Plain.et") === "world", "no Flags anywhere up the chain is a world height");
check(frameOf(root, null, "Prefabs/Orphan.et") === "unknown" && prefabFlags(root, null, "Prefabs/Orphan.et").missing === "Prefabs/Gone.et", "an unreadable parent is unknown, never guessed");
check(frameOf(root, null, "Prefabs/Lamp.et", 0x403) === "world" && frameOf(root, null, "Prefabs/Bin.et", 0x80003) === "terrain", "a row's own Flags line decides over its prefab's");
check(frameOf(root, null, "") === "world", "a bare GenericEntity row is a world height");
check(layerY(18.822, 18.656, "terrain") === 18.822 - 18.656 && layerY(18.8, 18.6, "world") === 18.8 && worldYOf(0.2, 18.6, "terrain") === 18.8, "layerY / worldYOf convert only the terrain frame");

// the check: the island's two failure shapes, and the rows that must stay clean
const at = (name, y, frame, x = 0, z = 0) => ({ name, layer: "Place_t.layer", x, y, z, frame });
const recs = [
  at("pump_flying", 18.822, "terrain"),
  at("pump_seated", 0.166, "terrain", 3),
  at("bin_sunk", 0.0, "world", 5),
  at("desk_a", 18.2, "world", 8),
  at("desk_b", 18.3, "world", 10),
  at("desk_c", 18.9, "world", 12),
  at("shore_bench", 1.01, "world", 900, 900),
  at("shore_post", 1.2, "world", 905, 900),
  { ...at("child", 18.5, "parent", 1), nested: true },
];
const wrong = wrongFrameRows(recs).map((w) => w.e.name).sort();
check(wrong.includes("pump_flying"), "a fuel pump at world 18.8 in a terrain-frame row is flagged");
check(wrong.includes("bin_sunk"), "a street bin at y 0 among world-frame rows at 18 m is flagged");
check(wrong.length === 2, `only those two are flagged (got ${wrong.join(", ")})`);

// town fit: each row written in its prefab's frame at render time
const pins = { at: (x, z) => (x === 1 && z === 2 ? 18.656 : null) };
const fr = (ref) => (ref.includes("Pump") ? "terrain" : "world");
const ents = [
  { id: "pump", ref: "{A}Prefabs/Pump.et", coords: [1, 18.822, 2] },
  { id: "bin", ref: "{B}Prefabs/Bin.et", coords: [1, 0.004, 2] },
  { id: "desk", ref: "{C}Prefabs/Desk.et", coords: [1, 18.9, 2] },
  { id: "far_pump", ref: "{A}Prefabs/Pump.et", coords: [7, 18.8, 7] },
];
const f = framedRows(ents, { terrain: pins, overGround: new Set(["bin"]), frameOfRef: fr });
const y = (id) => f.rowEnts.find((e) => e.id === id).coords[1];
check(y("pump") === 0.166, `fit writes a terrain-frame pump as its height over the ground (got ${y("pump")})`);
check(y("bin") === 18.66, `fit writes a world-frame "ground" row as terrain + its height (got ${y("bin")})`);
check(y("desk") === 18.9, "a world-frame row planned in world heights is written as it is");
check(f.pending.length === 1 && f.pending[0] === "far_pump" && f.points.length === 3, `a row with no probed ground is pending, never guessed (pending ${f.pending}, points ${f.points.length})`);
check(ents[0].coords[1] === 18.822, "framedRows leaves the planned rows untouched");

// night lights: a self-lit vanilla lamp (Lamps_Base) is written over the ground under it
const built = { glows: [{ name: "Glow_1", y: 12 }], heads: [], lamps: [{ name: "Lamp_1", y: 16.05, ty: 16.05 }, { name: "Lamp_2", y: 9.3 }], errors: [] };
const lit = frameRows(built, (ref) => (/LampStreet/.test(ref) ? "terrain" : "world"));
check(lit.lamps[0].y === 0 && lit.glows[0].y === 12, `a lamp seat is written as its height over the probed ground (got ${lit.lamps[0].y})`);
check(lit.errors.length === 1 && /Lamp_2/.test(lit.errors[0]), "a lamp with no probed ground height is an error");

console.log(failed ? `SUMMARY height-frame-selftest fail ${failed}` : "SUMMARY height-frame-selftest ok");
process.exit(failed ? 1 : 0);
