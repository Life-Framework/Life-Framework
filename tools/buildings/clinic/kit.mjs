// Clinic_Kit.et: the clinic as one placeable prefab. It inherits Clinic.et (the house with its furniture) and adds the
// interactables as children: the two hospital beds (respawn and "put patient in bed"), the medic duty desk, the medic
// dispatch board, the bell box for the site alarm and, from POLISH 1, the fascia sign and the facade glow. All of it is
// existing systems; nothing here is a new component. The doctor, the till and the medicine cabinet stand in the grid
// layer (layer.mjs), as the town hall's clerk does.
import { createHash } from "node:crypto";
import { alarmBox, beds, board, dutyDesk, facade, P, sign } from "./layout.mjs";

const id = (s) => createHash("sha1").update(`clinic-kit|${s}`).digest("hex").slice(0, 16).toUpperCase();
const n = (v) => String(+(+v).toFixed(3));
const ALARM = "{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et";
const FASCIA = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";

const child = (cls, ref, key, body, o) =>
  [`  ${cls} : "${ref}" {`, `   ID "${id(key)}"`, ...body.map((l) => `   ${l}`), `   coords ${n(o.x)} ${n(o.y)} ${n(o.z)}`, ...(o.yaw ? [`   angleY ${n(o.yaw)}`] : []), "  }"].join("\n");

export function kitText(pass, buildingGuid) {
  const rows = [
    child("GenericEntity", P.dutyDesk, "duty-desk", [], dutyDesk),
    child("GenericEntity", P.board, "board", [], board),
    ...beds.map((b) => child("GenericEntity", P.bed, b.name, [], b)),
    child("GenericEntity", ALARM, "alarm", ["components {", ' LF_AlarmMountComponent "{9ACA0BDAD2FE2665}" {', "  m_vOffset 0 0.167 0.065", "  m_fReach 12", " }", "}"], alarmBox),
  ];
  if (pass >= 1) {
    rows.push(child("GenericEntity", FASCIA, "sign", ["components {", ' LF_SignComponent "{6E3FF3980D87CACE}" {', '  m_sSignId "hospital"', "  m_vRimColor 0.1 0.3 0.18", "  m_vPaintColor 0.94 0.94 0.9", "  m_vTextColor 0.08 0.4 0.2", " }", "}"], sign));
    // the facade glow: one warm point light over the entrance, no shadow
    rows.push(["  LightEntity {", `   ID "${id("facade-glow")}"`, `   coords ${n(facade.x)} ${n(facade.y)} ${n(facade.z)}`, "   Color 1 0.82 0.58 1", "   LV 3.5", "   ClipEV -10", "   CastShadow 0", "   Radius 6", "   NearPlane 0.3", "   LensFlareType Disabled", "  }"].join("\n"));
  }
  return `SCR_DestructibleBuildingEntity : "{${buildingGuid}}Prefabs/Buildings/Clinic/Clinic.et" {\n ID "${id("root")}"\n {\n${rows.join("\n")}\n }\n}\n`;
}
