// The interior lighting preset (tools\cli building, export stage), applied to every building.
//
// What the first exports showed (docs/building-pipeline/polish.md "Interior brightness"): the ceiling
// lamps ArchBuilder places are plain meshes (LampFluorescent_*, LightRectangle_01 carry no light in the
// base game either), so an exported hall has no light of its own and a few dead lamps hung where
// ArchBuilder's furniture rules put them, two in a 17 x 9 m hall. This preset swaps them for fixtures
// placed from the room sizes: the rooms come from the building's geometry (blender/lf_rooms.py), each
// gets a grid of fixtures about `spacing` metres apart, every fixture is the base game's ceiling panel
// plus a spot LightEntity under it (an inline one: the base game's Lamp_LightEntity_base is off by day and by default).
import { createHash } from "node:crypto";

// Base-game resources only; the prefab stage checks each GUID against the game's resource database.
export const LIGHTING = {
  fixture: "{DF9B306707F2BD15}Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_02.et",
  spacing: 4.5, // metres between fixtures in a room
  max: 12, // fixtures per room
  drop: 0.2, // the light hangs this far under the ceiling
  color: "1 0.96 0.88 1",
  lv: 3.5,
  radius: 6,
  angle: 140,
};

// The ceiling lamps ArchBuilder's furniture rules place; the preset replaces them.
const CEILING_LAMP = /^ {2}\w+ : "\{[0-9A-F]{16}\}[^"]*\/(?:LampFluorescent_0\d|LightRectangle_01|LightCeiling_0\d|LampInterior_0\d)\.et" \{\n(?: {3,}[^\n]*\n)*? {2}\}\n/gm;

// The fixtures for a list of rooms ({x0, x1, y0, y1, z}, Blender metres: prefab x, prefab z, ceiling height).
export function fixturePlan(rooms, opts = {}) {
  const o = { ...LIGHTING, ...opts };
  const out = [];
  for (const r of rooms) {
    const w = r.x1 - r.x0;
    const d = r.y1 - r.y0;
    let nx = Math.max(1, Math.round(w / o.spacing));
    let nz = Math.max(1, Math.round(d / o.spacing));
    while (nx * nz > o.max) {
      if (nx >= nz) nx--;
      else nz--;
    }
    for (let i = 0; i < nx; i++) {
      for (let j = 0; j < nz; j++) out.push({ x: +(r.x0 + ((i + 0.5) * w) / nx).toFixed(3), z: +(r.y0 + ((j + 0.5) * d) / nz).toFixed(3), y: r.z });
    }
  }
  return out;
}

// The furniture prefab with ArchBuilder's ceiling lamps taken out and the preset's fixtures added.
export function applyLighting(text, rooms, slug = "", opts = {}) {
  const o = { ...LIGHTING, ...opts };
  if (!rooms.length) return { text, fixtures: 0, removed: 0 }; // no rooms found: leave ArchBuilder's lamps
  if (text.lastIndexOf("\n }\n}") < 0) throw new Error("the furniture prefab has no child list to add ceiling lights to");
  let removed = 0;
  const kept = text.replace(CEILING_LAMP, () => {
    removed++;
    return "";
  });
  const plan = fixturePlan(rooms, o);
  const id = (i, kind) => createHash("sha1").update(`${slug}|light|${i}|${kind}`).digest("hex").slice(0, 16).toUpperCase();
  const rows = plan.flatMap((f, i) => [
    `  GenericEntity : "${o.fixture}" {\n   ID "${id(i, "lamp")}"\n   coords ${f.x} ${+(f.y - 0.001).toFixed(3)} ${f.z}\n  }`,
    `  LightEntity {\n   ID "${id(i, "light")}"\n   coords ${f.x} ${+(f.y - o.drop).toFixed(3)} ${f.z}\n   angles -90 0 0\n   Color ${o.color}\n   LV ${o.lv}\n   ClipEV -10\n   Type Spot\n   Radius ${o.radius}\n   NearPlane 0.3\n   SpotAngle ${o.angle}\n   LensFlareType Disabled\n  }`,
  ]);
  const at = kept.lastIndexOf("\n }\n}");
  return { text: `${kept.slice(0, at)}\n${rows.join("\n")}${kept.slice(at)}`, fixtures: plan.length, removed };
}
