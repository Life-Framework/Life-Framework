// tools/livery/generate.mjs - the `tools\cli livery build` generator (WO-L2, #4232): one livery row in,
// the repainted variant's files and its credit row out. Pure: it reads vanilla prefab text through the
// `vanilla(rel)` callback (the game paks in the command, fixture files in the selftest) and writes nothing.
//
// Method (docs/architecture/livery-spike.md, proven by the spike #4226):
//   1. A body material that inherits the vanilla colour variant and overrides the paint layers.
//   2. A variant prefab that inherits the row's base vehicle and points the body mesh slot at it.
//   3. On a vehicle whose doors are separate parts (UAZ469), a door prefab and a door-top prefab per door
//      on the same material, with the vehicle's door slots and each door's DoorTop slot pointing at them.
//      Component and assign GUIDs are read from the vanilla door prefabs, never typed.
//   4. Lettering is layer 2 of our own mask (R channel), stamped from a spec by the mask step; the material
//      then turns layer 2 on with the offset that makes it show (MaskOffset_2 0.95, MaskSharpness_2 10).
//
// Every file lands in the pack (colours are what this server is). GUIDs are derived from the livery id and
// the part name, so a rebuild writes the same GUIDs and two liveries never share one (checkUnique).
import { createHash } from "node:crypto";
import { PANELS, MASK_SIZE } from "./panels.mjs";

export const PACK = "addons/LF_Everon1989";
const ASSETS = "Assets/Liveries";
const PREFABS = "Prefabs/Vehicles/Liveries";

// What the generator needs to know about each vanilla vehicle. Mesh and assign GUIDs are the vanilla
// body MeshObject's (the spike read them from the vanilla prefabs; they are the same on every variant).
export const VEHICLES = {
  UAZ469: {
    baseEmat: "{D1F1DD9AE3DC919D}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_BLUE.emat",
    mesh: "51DAA09FEFBFC0E7",
    assign: "64F09BE9246E252A",
    source: "UAZ469_Body",
    vanillaMask: "Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_GLOBAL_MASK.edds",
    doors: {
      vehicle: "Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et",
      names: ["L01", "R01", "L02", "R02"],
      door: (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_blue.et`,
      top: (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_top_blue.et`,
    },
  },
  S1203: {
    baseEmat: "{0078643DF969CA01}Assets/Vehicles/Wheeled/S1203/Data/S1203_body.emat",
    mesh: "51DAA09FEFBFC0E7",
    assign: "64DA70D3831058D9",
    source: "S1203_body",
    vanillaMask: null,
    doors: null,
  },
};

export function guidFor(id, part) {
  return createHash("sha1").update(`lf-livery:${id}:${part}`).digest("hex").slice(0, 16).toUpperCase();
}

const color = (c) => {
  if (!Array.isArray(c) || c.length < 3 || c.some((v) => typeof v !== "number")) throw new Error(`a colour must be [r, g, b(, a)], got ${JSON.stringify(c)}`);
  return [...c.slice(0, 3), c[3] ?? 1].join(" ");
};

function checkRow(row) {
  if (!row || !/^[A-Z][A-Za-z0-9]*$/.test(row.id || "")) throw new Error(`livery id must be PascalCase letters and digits, got "${row?.id}"`);
  if (!VEHICLES[row.vehicle]) throw new Error(`${row.id}: unknown vehicle "${row.vehicle}" (known: ${Object.keys(VEHICLES).join(", ")})`);
  if (!/^\{[0-9A-F]{16}\}Prefabs\/.+\.et$/.test(row.base || "")) throw new Error(`${row.id}: base must be a {GUID}Prefabs/...et reference`);
  const c = row.credit;
  if (!c || !c.source || !c.license || !c.purpose) throw new Error(`${row.id}: every livery needs a credit { source, license, purpose } (docs/credits.md "Liveries")`);
  if (!row.colors || !row.colors[1]) throw new Error(`${row.id}: colors[1] (the body colour) is required`);
}

// The mask spec mask.py stamps: { text: [{ x, y, h, text }] } in pixels of the 512 px mask.
export function maskSpec(row) {
  const L = row.lettering;
  const W = 6; // a 5x7 glyph plus one column of space, in glyph pixels
  if (L.mode === "tiled") {
    const h = L.height || 14, step = h + 3, word = `${L.text} `.repeat(Math.ceil(MASK_SIZE / ((L.text.length + 1) * W * (h / 7))) + 1).trim();
    const text = [];
    for (let r = 0, y = 2; y < MASK_SIZE; r++, y += step) text.push({ x: -((r * 23) % 92), y, h, text: word });
    return { text };
  }
  if (L.mode !== "panels") throw new Error(`${row.id}: lettering mode must be "tiled" or "panels"`);
  const panels = PANELS[row.vehicle] || {};
  return {
    text: L.items.map((it) => {
      const p = panels[it.panel];
      if (!p) throw new Error(`${row.id}: panel ${it.panel} has no UV-2 rectangle in tools/livery/panels.mjs (measure it first)`);
      const s = Math.max(1, Math.floor(Math.min((p.h * 0.6) / 7, p.w / (it.text.length * W - 1))));
      const w = (it.text.length * W - 1) * s, hh = 7 * s;
      const item = { x: Math.round(p.x + (p.w - w) / 2), y: Math.round(p.y + (p.h - hh) / 2), h: hh, text: it.text };
      if (p.rot) item.rot = p.rot;
      if (p.flipx) item.flipx = true;
      return item;
    }),
  };
}

function bodyEmat(row, v, maskRef) {
  const lines = [];
  if (maskRef) lines.push(` MaskMap "${maskRef}"`, " Enabled_2 1", " Mask_2 \"\"", " MaskOffset_2 0.95", " MaskSharpness_2 10");
  for (const k of [2, 3, 4]) if (row.enabled && k in row.enabled && !(maskRef && k === 2)) lines.push(` Enabled_${k} ${row.enabled[k] ? 1 : 0}`);
  for (const k of [4, 3, 2, 1]) if (row.colors[k]) lines.push(` Color_${k} ${color(row.colors[k])}`);
  return `MatPBRMulti : "${v.baseEmat}" {\n${lines.join("\n")}\n}\n`;
}

const meshOverride = (meshGuid, assignGuid, source, emat) => `  MeshObject "{${meshGuid}}" {
   Materials {
    MaterialAssignClass "{${assignGuid}}" {
     SourceMaterial "${source}"
     AssignedMaterial "${emat}"
    }
   }
  }
`;

function partMesh(text, source, what) {
  const mesh = /MeshObject "\{([0-9A-F]{16})\}"/.exec(text);
  const assign = new RegExp(`MaterialAssignClass "\\{([0-9A-F]{16})\\}" \\{\\s*SourceMaterial "${source}"\\s`).exec(text);
  if (!mesh || !assign) throw new Error(`no ${source} material slot in vanilla ${what}`);
  return [mesh[1], assign[1]];
}

const strip = (rel) => rel.slice(PACK.length + 1);
const ref = (f) => `{${f.guid}}${strip(f.rel)}`;

// planLivery(row, vanilla) -> { id, files: [{ rel, guid, text, cls, role }], mask, credit }
export function planLivery(row, vanilla) {
  checkRow(row);
  const v = VEHICLES[row.vehicle];
  const id = `${row.vehicle}_${row.id}`;
  const g = (part) => guidFor(id, part);
  const files = [];
  const add = (role, rel, guid, text, cls = "EntityTemplateResourceClass") => { const f = { role, rel: `${PACK}/${rel}`, guid, text, cls }; files.push(f); return f; };

  let mask = null;
  if (row.lettering) {
    if (!v.vanillaMask) throw new Error(`${row.id}: ${row.vehicle} has no vanilla mask to letter on`);
    const base = `${ASSETS}/${row.vehicle}/LF_${id}_MASK`;
    mask = { guid: g("mask"), tif: `${PACK}/${base}.tif`, edds: `${PACK}/${base}.edds`, vanilla: v.vanillaMask, spec: maskSpec(row) };
    mask.meta = maskMeta(`${base}.edds`, mask.guid);
  }
  const emat = add("material", `${ASSETS}/${row.vehicle}/LF_${id}.emat`, g("emat"), bodyEmat(row, v, mask && `{${mask.guid}}${strip(mask.edds)}`), "EMATResourceClass");
  const ematRef = ref(emat);

  const slots = [];
  let vehSlots = "";
  if (v.doors) {
    const veh = vanilla(v.doors.vehicle);
    const slotMgr = /SlotManagerComponent "\{([0-9A-F]{16})\}"/.exec(veh)?.[1];
    if (!slotMgr) throw new Error(`no SlotManagerComponent in vanilla ${v.doors.vehicle}`);
    for (const d of v.doors.names) {
      const doorRel = v.doors.door(d), topRel = v.doors.top(d);
      const vDoor = vanilla(doorRel), vTop = vanilla(topRel);
      const doorGuid = new RegExp(`Prefab "\\{([0-9A-F]{16})\\}${doorRel.replace(/[.]/g, "\\.")}"`).exec(veh)?.[1];
      const topGuid = new RegExp(`Prefab "\\{([0-9A-F]{16})\\}${topRel.replace(/[.]/g, "\\.")}"`).exec(vDoor)?.[1];
      const doorMgr = /SlotManagerComponent "\{([0-9A-F]{16})\}"/.exec(vDoor)?.[1];
      if (!doorGuid || !topGuid || !doorMgr) throw new Error(`vanilla door ${d}: slot or prefab reference not found`);
      const top = add("door-top", `${PREFABS}/${row.vehicle}_${row.id}_door_${d}_top.et`, g(`top-${d}`),
        `GenericEntity : "{${topGuid}}${topRel}" {\n ID "${g(`top-${d}-id`)}"\n components {\n${meshOverride(...partMesh(vTop, v.source, topRel), v.source, ematRef)} }\n}\n`);
      const door = add("door", `${PREFABS}/${row.vehicle}_${row.id}_door_${d}.et`, g(`door-${d}`),
        `GenericEntity : "{${doorGuid}}${doorRel}" {\n ID "${g(`door-${d}-id`)}"\n components {\n${meshOverride(...partMesh(vDoor, v.source, doorRel), v.source, ematRef)}  SlotManagerComponent "{${doorMgr}}" {\n   Slots {\n    RegisteringComponentSlotInfo DoorTop {\n     Prefab "${ref(top)}"\n    }\n   }\n  }\n }\n}\n`);
      slots.push(`    RegisteringComponentSlotInfo Door_${d} {\n     Prefab "${ref(door)}"\n    }\n`);
    }
    vehSlots = `  SlotManagerComponent "{${slotMgr}}" {\n   Slots {\n${slots.join("")}   }\n  }\n`;
  }
  const cls = row.entity || "Vehicle";
  add("vehicle", `${PREFABS}/LF_${id}.et`, g("vehicle"),
    `${cls} : "${row.base}" {\n ID "${g("vehicle-id")}"\n components {\n${meshOverride(v.mesh, v.assign, v.source, ematRef)}${vehSlots} }\n}\n`);

  const c = row.credit;
  const credit = `| LF_${id} | ${c.source} (${c.license}) | ${c.purpose} | \`${PACK}/${ASSETS}/${row.vehicle}/\` |`;
  return { id, files, mask, credit };
}

// Two plans that share a GUID or a path are a table error (a copied row, a renamed id that collides).
export function checkUnique(plans) {
  const seen = new Map();
  for (const p of plans) {
    for (const [k, what] of [...p.files.map((f) => [f.guid, f.rel]), ...(p.mask ? [[p.mask.guid, p.mask.edds]] : [])]) {
      if (seen.has(k)) throw new Error(`GUID ${k} twice: ${seen.get(k)} and ${what}`);
      seen.set(k, what);
    }
  }
  return seen.size;
}

const PLATFORMS = ["XBOX_ONE", "XBOX_SERIES", "PS4", "PS5", "HEADLESS"];
export function metaText(f) {
  return `MetaFileClass {\n Name "${ref(f)}"\n Configurations {\n  ${f.cls} PC {\n  }\n${PLATFORMS.map((p) => `  ${f.cls} ${p} : PC {\n  }\n`).join("")} }\n}\n`;
}

// The mask texture: the TextureMask preset (BC7, linear), as the vanilla GLOBAL_MASK textures use.
const MASK_CONF = { PC: "2231C67ACC4BB0AC", XBOX_ONE: "3B78A666261DD8B4", XBOX_SERIES: "9A79E1F659B378E7", PS4: "5CB64ED6EEBDE368", HEADLESS: "0B025DFAF55852CD" };
function maskMeta(rel, guid) {
  const row = (p) => `  TIFFResourceClass ${p} : "{${MASK_CONF[p]}}Configs/System/ResourceTypes/${p}/TextureMask.conf" {\n  }\n`;
  return `MetaFileClass {\n Name "{${guid}}${rel}"\n Configurations {\n${Object.keys(MASK_CONF).map(row).join("")}  TIFFResourceClass PS5 : PC {\n  }\n }\n}\n`;
}
