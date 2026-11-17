// The clinic's layer entities: the doctor, the till and the medicine cabinet, written into the pack grid layer
// (Everon1989Grid_Layers/Grid_clinic.layer) beside the building `grid place` put there.
//
//   node tools/buildings/clinic/layer.mjs          write the rows (replacing earlier Clinic_* rows)
//   node tools/buildings/clinic/layer.mjs --check  exit 1 when the layer is not what this script writes
//
// The till and the cabinet are the hospital site's own entities (`Grid_hospital.layer`, `LF_HospitalTillComponent`,
// `LF_HospitalCabinetComponent`) copied under new names and target ids, so the clinic's robbery and breach flows
// run on the existing code. Positions are building space (layout.mjs `placed`, front +z); the building entity's
// coords and angleY in the layer turn them into world space.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { placed } from "./layout.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_clinic.layer");
const HOSPITAL = join(ROOT, "addons", "LifeFramework", "Worlds", "GridWorld", "GridWorld_Layers", "Grid_hospital.layer");
const num = (v) => String(+(+v).toFixed(3));

// Top-level blocks of a layer: a block starts at column 0 and ends at the line `}`.
export function blocks(text) {
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

export function placement(text, entity) {
  const b = blocks(text).find((x) => x.head && new RegExp(`^\\w+ ${entity} `).test(x.head));
  if (!b) throw new Error(`no entity ${entity} in the layer`);
  const body = b.lines.join("\n");
  const c = /^ coords (\S+) (\S+) (\S+)$/m.exec(body);
  const a = /^ angleY (\S+)$/m.exec(body);
  return { x: +c[1], y: +c[2], z: +c[3], yaw: a ? +a[1] : 0 };
}

export function toWorld(p, o) {
  const r = (p.yaw * Math.PI) / 180;
  const s = Math.sin(r);
  const c = Math.cos(r);
  return { x: p.x + o.x * c + o.z * s, y: p.y + o.y, z: p.z - o.x * s + o.z * c, yaw: p.yaw + (o.yaw ?? 0) };
}

const COUNTER = '{2D6DB9D5AFEAB643}Assets/Props/Services/Post/CounterPostOffice_01/CounterPostOffice_01.xob';
const SHOP_COUNTER = '{3200A3508CA2E09B}Assets/Props/Commercial/CounterShop_01/CounterShop_01.xob';
const fromHospital = (text, entity, name, w) => {
  const b = blocks(text).find((x) => x.head?.startsWith(`GenericEntity ${entity} `));
  if (!b) throw new Error(`no ${entity} in Grid_hospital.layer`);
  const body = b.lines
    .join("\n")
    .replaceAll(entity, name)
    .replace(COUNTER, SHOP_COUNTER)
    .replace(/^ coords .*$/m, ` coords ${num(w.x)} ${num(w.y)} ${num(w.z)}`)
    .replace(/^ angleY .*$/m, w.yaw ? ` angleY ${num(w.yaw)}` : " angleY 0");
  return body;
};

export function rows(layerText) {
  const p = placement(layerText, "Grid_clinic_Main");
  const hospital = readFileSync(HOSPITAL, "utf8");
  const d = placed.doctor;
  const wd = toWorld(p, d);
  const doctor = `${d.cls} ${d.name} : "${d.prefab}" {\n coords ${num(wd.x)} ${num(wd.y)} ${num(wd.z)}\n angleY ${num(wd.yaw)}\n}`;
  const till = fromHospital(hospital, "Annex_Hospital_Till", placed.till.name, toWorld(p, placed.till));
  const cabinet = fromHospital(hospital, "Annex_Hospital_Cabinet", placed.cabinet.name, toWorld(p, placed.cabinet));
  return [doctor, till, cabinet];
}

export function withRows(text) {
  const kept = blocks(text).filter((b) => !(b.head && /^\w+ Clinic_/.test(b.head)));
  const base = kept.map((b) => b.lines.join("\n")).join("\n").replace(/\n+$/, "");
  return `${base}\n${rows(base).join("\n")}\n`;
}

if (!existsSync(LAYER)) {
  console.error("clinic layer: Grid_clinic.layer is missing (tools\\cli grid place clinic <Clinic_Kit>)");
  process.exit(1);
}
const cur = readFileSync(LAYER, "utf8");
const next = withRows(cur);
if (process.argv.includes("--check")) {
  console.log(next === cur ? "clinic layer: current" : "clinic layer: DRIFT (run node tools/buildings/clinic/layer.mjs)");
  process.exit(next === cur ? 0 : 1);
}
if (next !== cur) writeFileSync(LAYER, next);
console.log(`clinic layer: ${rows(cur.replace(/\n*$/, "\n")).length} row(s) written`);
