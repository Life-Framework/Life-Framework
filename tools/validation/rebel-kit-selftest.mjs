// tools/validation/rebel-kit-selftest.mjs
// inputs: tools/livery/rows/rebel.mjs, addons/LF_Everon1989/Prefabs/Vehicles/LF_Ural4320_RebelKit.et, addons/LF_Everon1989/Configs/Signs/LF_Signs_Everon1989/LF_Signs_RebelKit_Ural.conf, addons/LF_Everon1989/Language/lf_everon1989_localization.st
//
// The rebel Ural4320 carries a hand-painted number on both cab doors and a camo net over its cargo bed (#4544).
// Both ride on a hand-written kit prefab the generated `LF_Ural4320_Rebel` inherits (the generator has no
// body-plate support, and its files are shared with other livery cards). This checks the chain stays whole:
//   - the Ural row's `base` is the kit prefab, by the kit's own GUID;
//   - the kit has exactly two LF_SignComponent plates (left and right door), both naming one sign id;
//   - that sign id is a row in the pack sign config and its title is a pack string, in US English digits;
//   - the kit has at least one camo-net mesh (a CamoNet_*.xob) with a material assigned;
//   - the generated Ural prefab inherits the kit and still points the body mesh at the rebel material.
// red-proof: with the kit prefab file absent this printed "FAIL  kit prefab LF_Ural4320_RebelKit.et is missing" (2026-11-23).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
let failed = 0;
const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };
const read = (p) => readFileSync(p, "utf8");

const kitPath = join(PACK, "Prefabs", "Vehicles", "LF_Ural4320_RebelKit.et");
if (!existsSync(kitPath)) {
  fail("kit prefab LF_Ural4320_RebelKit.et is missing");
} else {
  const kit = read(kitPath);
  const meta = read(`${kitPath}.meta`);
  const guid = /Name "\{([0-9A-F]{16})\}Prefabs\/Vehicles\/LF_Ural4320_RebelKit\.et"/.exec(meta)?.[1];
  if (!guid) fail("kit prefab .meta has no Name with a GUID");

  const table = await import(pathToFileURL(join(ROOT, "tools", "livery", "table.mjs")).href);
  const row = table.LIVERIES.find((r) => r.vehicle === "Ural4320" && r.id === "Rebel");
  if (!row) fail("the table has no Ural4320 Rebel row");
  else if (guid && !row.base.startsWith(`{${guid}}Prefabs/Vehicles/LF_Ural4320_RebelKit.et`)) fail(`the Ural4320 Rebel row inherits ${row.base}, not the kit {${guid}}`);

  const signIds = new Set([...kit.matchAll(/m_sSignId "([^"]+)"/g)].map((m) => m[1]));
  const plates = (kit.match(/LF_SignComponent "/g) || []).length;
  if (plates !== 2) fail(`kit has ${plates} door plates, want 2 (left and right)`);
  if (signIds.size !== 1) fail(`kit plates name ${signIds.size} sign ids, want 1`);
  const nets = [...kit.matchAll(/Object "\{[0-9A-F]{16}\}Assets\/[^"]*CamoNet_[^"]+\.xob"/g)].length;
  if (nets < 1) fail("kit has no CamoNet mesh over the bed");
  const camoMats = (kit.match(/AssignedMaterial "\{[0-9A-F]{16}\}Assets\/Structures\/Military\/CamoNets\/Data\/[^"]+\.emat"/g) || []).length;
  if (camoMats < 2 * nets) fail(`kit has ${camoMats} camo-net materials for ${nets} net meshes, want 2 each`);

  const id = [...signIds][0];
  const conf = join(PACK, "Configs", "Signs", "LF_Signs_Everon1989", "LF_Signs_RebelKit_Ural.conf");
  const confText = existsSync(conf) ? read(conf) : "";
  const key = new RegExp(String.raw`m_sId "${id}"[\s\S]*?m_sTitle "#([^"]+)"`).exec(confText)?.[1];
  if (!key) fail(`the pack sign config has no row for sign id ${id}`);
  else {
    const st = read(join(PACK, "Language", "lf_everon1989_localization.st"));
    const text = new RegExp(String.raw`Id "${key}"\s+Target_en_us "([^"]*)"`).exec(st)?.[1];
    if (!text || !/^[0-9]{1,3}$/.test(text)) fail(`pack string ${key} is "${text}", want a 1 to 3 digit number`);
  }
}

const gen = join(PACK, "Prefabs", "Vehicles", "Liveries", "LF_Ural4320_Rebel.et");
if (!existsSync(gen)) fail("generated LF_Ural4320_Rebel.et is missing");
else {
  const g = read(gen);
  if (!/^Vehicle : "\{[0-9A-F]{16}\}Prefabs\/Vehicles\/LF_Ural4320_RebelKit\.et"/.test(g)) fail("LF_Ural4320_Rebel.et does not inherit the kit prefab");
  if (!g.includes("LF_Ural4320_Rebel.emat")) fail("LF_Ural4320_Rebel.et lost its rebel body material");
}

console.log(failed ? `FAIL  rebel-kit-selftest: ${failed} problem(s)` : "OK    rebel-kit-selftest");
process.exit(failed ? 1 : 0);
