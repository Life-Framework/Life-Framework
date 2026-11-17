// The fuel station's systems (pass 1 BUILD): every interactable is an existing system, nothing new.
//
//   node tools/buildings/fuel-station/fitout.mjs          write the kit prefab and the grid layer rows
//   node tools/buildings/fuel-station/fitout.mjs --check  exit 1 when the files are not what this script writes
//
// FuelStation_Kit.et  child of FuelStation.et (prune.mjs adds it): the one canopy light (shadowless), the alarm
//                     bell box on the shop front (LF_AlarmMountComponent: the site alarm LF_SiteAlarm_Fuel shows here),
//                     and the pump hum between the pumps (LF_FuelPumpHum, pulsed by LF_AmbientFuelPump).
// Grid_fuel_station.layer rows FuelStation_*: the till on the shop counter (LF_FuelTill_Attended, the dialogue-only
//                     robbery of #936, a dispatch source) and the clerk behind it (LF_Npc_FuelClerk_1, dialogue
//                     fuel_clerk, which finds the till by distance). NPCs and dispatch places live in the layer.
//
// Space: station frame (the vanilla composition's, street front -z, spec.json host.front). The shop building sits at
// spec host.parts.building turned 180 degrees, so a point (bx, by, bz) in the building's own frame is
// (at.x - bx, at.y + by, at.z - bz) here. The layer rows add the station entity's coords (no yaw in the grid).
import { createHash, randomBytes } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const REL = "Prefabs/Buildings/FuelStation";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...REL.split("/"));
const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_fuel_station.layer");
const SPEC = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), "spec.json"), "utf8"));
const CHECK = process.argv.includes("--check");
const PREFIX = "F5F1";
const ROW = "FuelStation_";

const num = (n) => String(+(+n).toFixed(3));
const id = (s) => PREFIX + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();
const B = SPEC.host.parts.building.at;
const fromBuilding = ([bx, by, bz]) => [B[0] - bx, B[1] + by, B[2] - bz];

const P = {
  till: "{9D3A51E060EFE982}Prefabs/Props/Commercial/LF_FuelTill_Attended.et",
  clerk: "{4C624B7D84E8BAFA}Prefabs/Characters/Npcs/Outfits/LF_Npc_FuelClerk_1.et",
  hum: "{D88D0B05DF83772F}Prefabs/Atmosphere/LF_FuelPumpHum.et",
  bell: "{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et",
};

// Building frame (front +z), from the vanilla furniture: the register stands at (-5.074, 1.139, -0.838) on the
// counter at (-4.679, 0.1, -0.645); the clerk stands between the counter and the armchair (-5.03, -2.228), facing
// the door (+z in the building, -z in the station). The bell box hangs on the shop's street wall (collider z max
// 2.515, `tools\cli measure`), west of the door, facing the street.
export const PLAN = {
  till: fromBuilding([-5.074, 1.139, -0.838]),
  clerk: { at: fromBuilding([-4.75, 0.1, -1.5]), yaw: 180 },
  bell: { at: fromBuilding([-5.2, 2.4, 2.52]), yaw: 180 },
  // between the two pumps (spec host.parts.pumps), on the slab: within LF_AmbientFuelPump.REACH_M (8 m) of both
  hum: [-2.78, 0.2, -0.7],
  // one light under the canopy, between its two fluorescent lamps (y 6.076), shadowless (medium budget: 1 caster)
  light: { at: [-0.89, 5.75, -0.2], color: "0.86 0.94 1 1", lv: 5.5, radius: 12 },
};

function metaGuid(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  const confs = ["PC {", "XBOX_ONE : PC {", "XBOX_SERIES : PC {", "PS4 : PC {", "PS5 : PC {", "HEADLESS : PC {"];
  const body = confs.map((c) => `  EntityTemplateResourceClass ${c}\n  }`).join("\n");
  writeFileSync(meta, `MetaFileClass {\n Name "{${guid}}${REL}/${rel}"\n Configurations {\n${body}\n }\n}\n`);
  return guid;
}

function kit() {
  const { light, bell } = PLAN;
  const [hx, hy, hz] = PLAN.hum;
  return [
    "GenericEntity {",
    ` ID "${id("kit")}"`,
    " {",
    "  LightEntity {",
    `   ID "${id("light|canopy")}"`,
    `   coords ${light.at.map(num).join(" ")}`,
    `   Color ${light.color}`,
    `   LV ${light.lv}`,
    "   ClipEV -10",
    "   CastShadow 0",
    `   Radius ${light.radius}`,
    "   NearPlane 0.3",
    "   LensFlareType Disabled",
    "  }",
    `  GenericEntity : "${P.bell}" {`,
    `   ID "${id("bell")}"`,
    "   components {",
    `    LF_AlarmMountComponent "{${id("bell|mount")}}" {`,
    "     m_vOffset 0 0.167 0.065",
    "    }",
    "   }",
    `   coords ${bell.at.map(num).join(" ")}`,
    `   angleY ${num(bell.yaw)}`,
    "  }",
    `  GenericEntity : "${P.hum}" {`,
    `   ID "${id("hum")}"`,
    `   coords ${num(hx)} ${num(hy)} ${num(hz)}`,
    "  }",
    " }",
    "}",
    "",
  ].join("\n");
}

// Top-level blocks of a layer: a block starts at column 0 and ends at the line `}`.
function blocks(text) {
  const out = [];
  let cur = null;
  for (const line of text.split("\n")) {
    if (cur === null) {
      if (/^[A-Za-z_$]/.test(line) && line.trimEnd().endsWith("{")) cur = [line];
      else if (line.trim() !== "") out.push({ head: null, lines: [line] });
    } else {
      cur.push(line);
      if (line === "}") {
        out.push({ head: cur[0], lines: cur });
        cur = null;
      }
    }
  }
  return out;
}

function layer(text) {
  const main = /^SCR_DestructibleBuildingEntity Grid_fuel_station_Main [^\n]*\n coords (\S+) (\S+) (\S+)$/m.exec(text);
  if (!main) throw new Error("no Grid_fuel_station_Main in the layer (tools\\cli grid place fuel-station ...)");
  const o = [+main[1], +main[2], +main[3]];
  const w = (p) => p.map((v, i) => num(v + o[i])).join(" ");
  const till = [
    `GenericEntity ${ROW}Till : "${P.till}" {`,
    " components {",
    `  LF_PlaceComponent "{${id("till|place")}}" {`,
    '   m_sKind "DispatchSource"',
    `   m_sId "${ROW}Till"`,
    "  }",
    " }",
    " Flags 0x403 0",
    ` coords ${w(PLAN.till)}`,
    "}",
  ].join("\n");
  const clerk = [`LF_NpcBaseEntity ${ROW}Clerk : "${P.clerk}" {`, ` coords ${w(PLAN.clerk.at)}`, ` angleY ${num(PLAN.clerk.yaw)}`, "}"].join("\n");
  const kept = blocks(text).filter((b) => !(b.head && new RegExp(`^\\w+ ${ROW}`).test(b.head)));
  const base = kept.map((b) => b.lines.join("\n")).join("\n").replace(/\n+$/, "");
  return `${base}\n${till}\n${clerk}\n`;
}

metaGuid("FuelStation_Kit.et");
const outs = [
  [join(DIR, "FuelStation_Kit.et"), kit()],
  [LAYER, layer(readFileSync(LAYER, "utf8"))],
];
let drift = false;
for (const [file, text] of outs) {
  if (existsSync(file) && readFileSync(file, "utf8") === text) continue;
  drift = true;
  if (CHECK) console.log(`  differs: ${file}`);
  else writeFileSync(file, text);
}
console.log(`fuel-station fitout: kit (1 light, bell box, pump hum), layer (till, clerk)${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
if (CHECK && drift) process.exit(1);
