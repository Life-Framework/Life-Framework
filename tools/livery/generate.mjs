// tools/livery/generate.mjs - the `tools\cli livery build` generator (WO-L2, #4232): one livery row in,
// the repainted variant's files, its sign rows and its credit row out. Pure: it reads vanilla prefab text
// through the `vanilla(rel)` callback (the game paks in the command, fixture files in the selftest) and
// writes nothing.
//
// Method (docs/architecture/livery-spike.md, proven by the spike #4226):
//   1. A body material that inherits the vanilla colour variant and overrides the paint layers. Colours only:
//      lettering stamped into the shared 512 px mask came out as scrambled fragments over the whole body
//      (the mask is tiled across UV islands), so words never go in the paint.
//   2. A variant prefab that inherits the row's base vehicle and points the body mesh slot at it.
//   3. On a vehicle whose doors are separate parts (UAZ469), a door prefab and a door-top prefab per door
//      on the same material, with the vehicle's door slots and each door's DoorTop slot pointing at them.
//      Component and assign GUIDs are read from the vanilla door prefabs, never typed.
//   4. Words are a painted plate on each front door (`plate`): a child of the generated door prefab, so it
//      swings with the door. It is the SignWall_01_90x45 plate with LF_SignComponent (the postal van's
//      method), its title a pack sign row and a pack string. No read action, so it adds nothing to the
//      door's action list.
//   5. Words on a vehicle whose doors are part of the body mesh (S105, S1203, UAZ452) are plates on fixed
//      panels (`panels`): children of the variant prefab itself, each with its own position, yaw and scale.
//      The build writes them, so a rebuild keeps them; livery-selftest fails when a generated file on disk
//      differs from what the build writes (a hand edit the next build would drop).
//
// Every file lands in the pack (colours are what this server is). GUIDs are derived from the livery id and
// the part name, so a rebuild writes the same GUIDs and two liveries never share one (checkUnique).
import { createHash } from "node:crypto";
import { existsSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));

export const PACK = "addons/LF_Everon1989";
const ASSETS = "Assets/Liveries";
const PREFABS = "Prefabs/Vehicles/Liveries";
const SIGNS = "Configs/Signs/LF_Signs_Everon1989";

// The plate: SignWall_01_90x45 (0.9 x 0.45 m, face toward +z, pivot on its back), painted like LF_Sign_PlateSmall.
const PLATE_XOB = "{8825082D2DEB3C8C}Assets/Structures/BuildingsParts/Signs/SignWall_01/SignWall_01_90x45.xob";
const PLATE_SOURCE = "SignWall_01_90X45";
const SIGN_FACE = "{355E4F25429C2EF1}Assets/Signs/Data/LF_SignFace.emat";
const SIGN_LAYOUT = "{5783648244305C88}UI/Layouts/Signs/LF_SignPaintFace.layout";

// What the generator needs to know about each vanilla vehicle: one file per vehicle in tools/livery/vehicles/
// (the file name is the key a row names in `vehicle`; `export default { baseEmat, mesh, assign, source, doors }`).
// See vehicles/UAZ469.mjs for the fields. A new vehicle is a new file, no list here changes.
export async function loadVehicles(dir = join(HERE, "vehicles")) {
  if (!existsSync(dir)) throw new Error(`livery vehicles folder ${dir} is missing`);
  const out = {};
  for (const name of readdirSync(dir).filter((n) => n.endsWith(".mjs") && !n.startsWith("_")).sort()) {
    const mod = await import(pathToFileURL(join(dir, name)).href);
    const v = mod.default;
    if (!v || typeof v !== "object" || !v.baseEmat || !v.mesh || !v.assign || !v.source || !("doors" in v)) throw new Error(`${name}: a vehicle file must export default { baseEmat, mesh, assign, source, doors }`);
    out[name.slice(0, -".mjs".length)] = v;
  }
  return out;
}

export const VEHICLES = await loadVehicles();

export function guidFor(id, part) {
  return createHash("sha1").update(`lf-livery:${id}:${part}`).digest("hex").slice(0, 16).toUpperCase();
}

const color = (c, alpha = true) => {
  if (!Array.isArray(c) || c.length < 3 || c.some((v) => typeof v !== "number")) throw new Error(`a colour must be [r, g, b(, a)], got ${JSON.stringify(c)}`);
  return (alpha ? [...c.slice(0, 3), c[3] ?? 1] : c.slice(0, 3)).join(" ");
};
const num = (n) => String(Math.round(n * 1000) / 1000);

function checkRow(row) {
  if (!row || !/^[A-Z][A-Za-z0-9]*$/.test(row.id || "")) throw new Error(`livery id must be PascalCase letters and digits, got "${row?.id}"`);
  if (!VEHICLES[row.vehicle]) throw new Error(`${row.id}: unknown vehicle "${row.vehicle}" (known: ${Object.keys(VEHICLES).join(", ")})`);
  if (!/^\{[0-9A-F]{16}\}Prefabs\/.+\.et$/.test(row.base || "")) throw new Error(`${row.id}: base must be a {GUID}Prefabs/...et reference`);
  const c = row.credit;
  if (!c || !c.source || !c.license || !c.purpose) throw new Error(`${row.id}: every livery needs a credit { source, license, purpose } (docs/credits.md "Liveries")`);
  if (!row.colors || !row.colors[1]) throw new Error(`${row.id}: colors[1] (the body colour) is required`);
  if (row.lettering) throw new Error(`${row.id}: mask lettering is gone (it scrambles across UV islands); use plate { text, paint, ink, rim }`);
  if (row.plate) {
    const p = row.plate;
    if (!VEHICLES[row.vehicle].doors?.plate) throw new Error(`${row.id}: ${row.vehicle} has no door plate positions`);
    if (!/^[A-Z0-9 .'-]{1,16}$/.test(p.text || "")) throw new Error(`${row.id}: plate.text must be 1-16 capitals (US English), got "${p.text}"`);
    for (const k of ["paint", "ink", "rim"]) color(p[k] || []);
  }
  if (row.panels) checkPanels(row);
}

// panels { sign, paint, ink, rim, at: [{ name, coords: [x, y, z], yaw, scale, tilt }] }: plates on fixed body panels,
// in the vehicle's frame. `sign` names an existing sign row; without it the row's door plate sign is used.
// `tilt` (degrees, optional) leans the plate about its own horizontal axis to lie on a side that slopes in at the top.
function checkPanels(row) {
  const p = row.panels;
  if (!p.sign && !row.plate) throw new Error(`${row.id}: panels.sign is required when the row has no door plate`);
  if (p.sign && !/^[a-z0-9_]+$/.test(p.sign)) throw new Error(`${row.id}: panels.sign must be a sign id (lower case, digits, _), got "${p.sign}"`);
  for (const k of ["paint", "ink", "rim"]) color(p[k] || []);
  if (!Array.isArray(p.at) || !p.at.length) throw new Error(`${row.id}: panels.at must list at least one panel`);
  const names = new Set();
  for (const a of p.at) {
    if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(a.name || "") || names.has(a.name)) throw new Error(`${row.id}: each panel needs a unique entity name, got "${a.name}"`);
    names.add(a.name);
    if (!Array.isArray(a.coords) || a.coords.length !== 3 || a.coords.some((n) => typeof n !== "number")) throw new Error(`${row.id}: panel ${a.name} coords must be [x, y, z]`);
    if (typeof a.yaw !== "number" || typeof a.scale !== "number" || a.scale <= 0) throw new Error(`${row.id}: panel ${a.name} needs a number yaw and a positive scale`);
    if ("tilt" in a && (typeof a.tilt !== "number" || Math.abs(a.tilt) > 45)) throw new Error(`${row.id}: panel ${a.name} tilt must be a number of degrees, at most 45 either way`);
  }
}

function bodyEmat(row, v) {
  const lines = [];
  for (const k of [2, 3, 4]) if (row.enabled && k in row.enabled) lines.push(` Enabled_${k} ${row.enabled[k] ? 1 : 0}`);
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

// One plate child, in its parent's frame (a door, or the vehicle for a panel). g(part) gives its stable GUIDs.
function plateChild(p, at, scale, signId, g, name = "") {
  return `  GenericEntity ${name ? `${name} ` : ""}{
   ID "${g("id")}"
   components {
    Hierarchy "{${g("hier")}}" {
    }
    MeshObject "{${g("mesh")}}" {
     Object "${PLATE_XOB}"
     Materials {
      MaterialAssignClass "{${g("mat")}}" {
       SourceMaterial "${PLATE_SOURCE}"
       AssignedMaterial "${SIGN_FACE}"
      }
      MaterialAssignClass "{${g("mlod")}}" {
       SourceMaterial "${PLATE_SOURCE}_MLOD"
       AssignedMaterial "${SIGN_FACE}"
      }
     }
    }
    LF_SignComponent "{${g("sign")}}" {
     m_sSignId "${signId}"
     m_bFaceOnOwner 1
     m_sFaceLayout "${SIGN_LAYOUT}"
     m_vTitlePos 60 56 0
     m_vTitleSize 904 400 0
     m_iTitleFontMax 220
     m_vFaceArea 1024 512 0
     m_fRimWidth 26
     m_vRimColor ${color(p.rim, false)}
     m_vPaintColor ${color(p.paint, false)}
     m_vTextColor ${color(p.ink, false)}
    }
   }
   coords ${at.coords.map(num).join(" ")}
   angles ${num(at.tilt || 0)} ${at.yaw} 0
   scale ${num(scale)}
  }
`;
}

const strip = (rel) => rel.slice(PACK.length + 1);
const ref = (f) => `{${f.guid}}${strip(f.rel)}`;

// planLivery(row, vanilla) -> { id, files: [{ rel, guid, text, cls, role }], strings: [{ id, guid, text, comment }], credit }
export function planLivery(row, vanilla) {
  checkRow(row);
  const v = VEHICLES[row.vehicle];
  const id = `${row.vehicle}_${row.id}`;
  const g = (part) => guidFor(id, part);
  const files = [];
  const strings = [];
  const add = (role, rel, guid, text, cls = "EntityTemplateResourceClass") => { const f = { role, rel: `${PACK}/${rel}`, guid, text, cls }; files.push(f); return f; };

  const emat = add("material", `${ASSETS}/${row.vehicle}/LF_${id}.emat`, g("emat"), bodyEmat(row, v), "EMATResourceClass");
  const ematRef = ref(emat);

  let signId = null;
  if (row.plate) {
    signId = `livery_${id.toLowerCase()}`;
    const key = `LF-Sign_Livery_${id}_Title`;
    strings.push({ id: key, guid: g("string"), text: row.plate.text, comment: `Livery plate on the front doors of the ${row.vehicle} ${row.id} (written by tools\\cli livery build).` });
    add("signs", `${SIGNS}/LF_Signs_Livery_${id}.conf`, g("signs"),
      `LF_SignConfig {\n m_aSigns {\n  LF_SignDef "{${g("signdef")}}" {\n   m_sOp "add"\n   m_sId "${signId}"\n   m_sTitle "#${key}"\n  }\n }\n}\n`, "CONFResourceClass");
  }

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
      const at = signId && v.doors.plate[d];
      const children = at ? ` {\n${plateChild(row.plate, at, v.doors.plateScale, signId, (k) => g(`plate-${d}-${k}`))} }\n` : "";
      const door = add("door", `${PREFABS}/${row.vehicle}_${row.id}_door_${d}.et`, g(`door-${d}`),
        `GenericEntity : "{${doorGuid}}${doorRel}" {\n ID "${g(`door-${d}-id`)}"\n components {\n${meshOverride(...partMesh(vDoor, v.source, doorRel), v.source, ematRef)}  SlotManagerComponent "{${doorMgr}}" {\n   Slots {\n    RegisteringComponentSlotInfo DoorTop {\n     Prefab "${ref(top)}"\n    }\n   }\n  }\n }\n${children}}\n`);
      slots.push(`    RegisteringComponentSlotInfo Door_${d} {\n     Prefab "${ref(door)}"\n    }\n`);
    }
    vehSlots = `  SlotManagerComponent "{${slotMgr}}" {\n   Slots {\n${slots.join("")}   }\n  }\n`;
  }
  const cls = row.entity || "Vehicle";
  let panels = "";
  if (row.panels) {
    const p = row.panels;
    panels = ` {\n${p.at.map((a) => plateChild(p, a, a.scale, p.sign || signId, (k) => g(`panel-${a.name}-${k}`), a.name)).join("")} }\n`;
  }
  add("vehicle", `${PREFABS}/LF_${id}.et`, g("vehicle"),
    `${cls} : "${row.base}" {\n ID "${g("vehicle-id")}"\n components {\n${meshOverride(v.mesh, v.assign, v.source, ematRef)}${vehSlots} }\n${panels}}\n`);

  const c = row.credit;
  const credit = `| LF_${id} | ${c.source} (${c.license}) | ${c.purpose} | \`${PACK}/${ASSETS}/${row.vehicle}/\` |`;
  return { id, files, strings, credit };
}

// Two plans that share a GUID or a path are a table error (a copied row, a renamed id that collides).
export function checkUnique(plans) {
  const seen = new Map();
  for (const p of plans) {
    for (const [k, what] of [...p.files.map((f) => [f.guid, f.rel]), ...p.strings.map((s) => [s.guid, s.id])]) {
      if (seen.has(k)) throw new Error(`GUID ${k} twice: ${seen.get(k)} and ${what}`);
      seen.set(k, what);
    }
  }
  return seen.size;
}

const PLATFORMS = ["XBOX_ONE", "XBOX_SERIES", "PS4", "PS5", "HEADLESS"];
export function metaText(f) {
  const plats = f.cls === "CONFResourceClass" ? PLATFORMS.filter((p) => p !== "PS5") : PLATFORMS;
  return `MetaFileClass {\n Name "${ref(f)}"\n Configurations {\n  ${f.cls} PC {\n  }\n${plats.map((p) => `  ${f.cls} ${p} : PC {\n  }\n`).join("")} }\n}\n`;
}

// One string-table row in the pack table's format.
export function stringItem(s) {
  return `    CustomStringTableItem "{${s.guid}}" {\n     Id "${s.id}"\n     Target_en_us "${s.text}"\n     Comment "${s.comment}"\n    }\n`;
}
