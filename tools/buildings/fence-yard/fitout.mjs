// The fence yard's systems (pass 1 BUILD): every interactable is an existing system, nothing new.
//
//   node tools/buildings/fence-yard/fitout.mjs          write the kit prefab and the grid layer rows
//   node tools/buildings/fence-yard/fitout.mjs --check  exit 1 when the files are not what this script writes
//
// Run prune.mjs first: it writes FenceYard_Kit.et.meta and hangs the kit on FenceYard.et (child "Kit").
//
// FenceYard_Kit.et      the three room lights (bay, office, back room), shadowless, short range (small budget:
//                       3 lights, 0 shadow casters). The washroom borrows the office light through its door.
// Grid_fence_yard.layer rows FenceYard_*: the fence's stall (LF_FenceStall, LF_ShopComponent m_bFence, stock
//                       "fence") and the fence himself (LF_Fence) behind it, both in the annex back room. Nothing
//                       on the street or the map names the fence (D-M5): no sign row, no map place.
//
// Space: building frame (front +z, spec.json host). The annex stands at spec host.annex (x -3.26, z -6.45, yaw
// -90), so an annex point (ax, ay, az) is (-3.26 - az, ay, -6.45 + ax) here. The grid places the building at
// Grid_fence_yard_Main with a yaw, so a building point (bx, by, bz) at yaw a is pivot + (bx cos a + bz sin a, by,
// -bx sin a + bz cos a) in the world.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
const REL = "Prefabs/Buildings/FenceYard";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...REL.split("/"));
const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_fence_yard.layer");
const SPEC = JSON.parse(readFileSync(join(HERE, "spec.json"), "utf8"));
const CHECK = process.argv.includes("--check");
const PREFIX = "FE1F";
const ROW = "FenceYard_";

const num = (n) => String(+(+n).toFixed(3));
const id = (s) => PREFIX + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();
const A = SPEC.host.annex;
const fromAnnex = ([ax, ay, az]) => [A.x - az, ay, A.z + ax];
const FLOOR = SPEC.host.floor_y;

const P = {
  stall: "{4855AC571C89CA6D}Prefabs/Props/Commercial/LF_FenceStall.et",
  fence: "{12FCE81FDD79F2B4}Prefabs/Characters/Npcs/LF_Fence.et",
};

// Annex furniture (FenceYard_Side_furniture_01.et, annex frame): the vanilla workbench and vice on the +z (window)
// wall, the metal cabinet on the +x wall at z 0.03, the stove, armchair and table along -z. The stall (2.59 x 0.97 m,
// `tools\cli measure`) stands across the room with its long side along the annex z, 0.6 m in front of the cabinet;
// the fence stands between it and the cabinet's south end, facing the customer side. Turned into the building frame
// the stall's yaw is 0 and the fence faces -z (180).
export const PLAN = {
  stall: { at: fromAnnex([0.9, FLOOR, 0]), yaw: 0 },
  fence: { at: fromAnnex([1.65, FLOOR, -1.0]), yaw: 180 },
  // One light per room, under the ceiling (main surfaces 3.039 / 2.793, annex lamp at y 3.169).
  lights: [
    { room: "bay", at: [2.75, 2.6, 0.0], lv: 4, radius: 4.5 },
    { room: "office", at: [-3.2, 2.5, 1.5], lv: 3.5, radius: 3.5 },
    { room: "backroom", at: fromAnnex([-0.35, 2.9, 0.12]), lv: 3, radius: 3.2 },
  ],
  color: "1 0.9 0.75 1",
};

function kitGuid() {
  const meta = join(DIR, "FenceYard_Kit.et.meta");
  if (!existsSync(meta)) throw new Error("FenceYard_Kit.et.meta is missing: run node tools/buildings/fence-yard/prune.mjs first");
  return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))[1];
}

function kit() {
  const lines = ["GenericEntity {", ` ID "${id("kit")}"`, " {"];
  for (const l of PLAN.lights) {
    lines.push(
      "  LightEntity {",
      `   ID "${id(`light|${l.room}`)}"`,
      `   coords ${l.at.map(num).join(" ")}`,
      `   Color ${PLAN.color}`,
      `   LV ${l.lv}`,
      "   ClipEV -10",
      "   CastShadow 0",
      `   Radius ${l.radius}`,
      "   NearPlane 0.3",
      "   LensFlareType Disabled",
      "  }",
    );
  }
  lines.push(" }", "}", "");
  return lines.join("\n");
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
  const main = /^SCR_DestructibleBuildingEntity Grid_fence_yard_Main [^\n]*\n coords (\S+) (\S+) (\S+)(?:\n angleY (\S+))?$/m.exec(text);
  if (!main) throw new Error("no Grid_fence_yard_Main in the layer (tools\\cli grid place fence-yard ...)");
  const o = [+main[1], +main[2], +main[3]];
  const yaw = +(main[4] ?? 0);
  const r = (yaw * Math.PI) / 180;
  const c = Math.round(Math.cos(r) * 1e9) / 1e9;
  const s = Math.round(Math.sin(r) * 1e9) / 1e9;
  const w = ([bx, by, bz]) => [o[0] + bx * c + bz * s, o[1] + by, o[2] - bx * s + bz * c].map(num).join(" ");
  const turn = (a) => {
    let v = (((a + yaw) % 360) + 360) % 360;
    if (v > 180) v -= 360;
    return num(v);
  };
  const stall = [`GenericEntity ${ROW}Stall : "${P.stall}" {`, ` coords ${w(PLAN.stall.at)}`, ` angleY ${turn(PLAN.stall.yaw)}`, "}"].join("\n");
  const fence = [`LF_NpcBaseEntity ${ROW}Fence : "${P.fence}" {`, ` coords ${w(PLAN.fence.at)}`, ` angleY ${turn(PLAN.fence.yaw)}`, "}"].join("\n");
  const kept = blocks(text).filter((b) => !(b.head && new RegExp(`^\\w+ ${ROW}`).test(b.head)));
  const base = kept.map((b) => b.lines.join("\n")).join("\n").replace(/\n+$/, "");
  return `${base}\n${stall}\n${fence}\n`;
}

kitGuid();
const outs = [
  [join(DIR, "FenceYard_Kit.et"), kit()],
  [LAYER, layer(readFileSync(LAYER, "utf8"))],
];
let drift = false;
for (const [file, text] of outs) {
  if (existsSync(file) && readFileSync(file, "utf8") === text) continue;
  drift = true;
  if (CHECK) console.log(`  differs: ${file}`);
  else writeFileSync(file, text);
}
console.log(`fence-yard fitout: kit (${PLAN.lights.length} shadowless lights), layer (stall, fence)${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
if (CHECK && drift) process.exit(1);
