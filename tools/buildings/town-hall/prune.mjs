// Which vanilla props the town hall keeps, and the one it swaps. Everything here is a base-game prop of
// MunicipalOffice_E_01's furniture set (242 children). The performance budget (about 150 decorative entities in a
// large building) is why most of the repeats go: the set is cut to what the rooms read from, in file order.
//
// Building space: the front is +z, the entrance at x 0, z 7.7; the post counter runs along z 4.1 from x 4.9 to
// 10.6 (customers at z > 4.1, benches at z 6.8; clerks behind it); the stair is in the back wing (x 0..3, z -7.4..-3).
import { floorOf, short } from "./furniture-lib.mjs";

// Props that never stay: electric switches (20 tiny meshes), the hydrant box, the roof aerial, the infirmary
// (the building was a clinic in the base game; here it is the registry), the post office sign (the post office
// is another building) and the small bathroom mirrors' duplicates are limited below.
const DROP = /LightSwitch_02|HydrantBox|TvAntenna|BedMedical|CabinetMedical|ScaleMedical|BinMedical|LampMedical|SignWall_01_90x45_PostOffice|BuildingParts\/Heating\/RadiatorCastIron_01\/RadiatorCastIron_01_65|Wardrobe_01|Notebook_01_Stack|Broom_01|Bucket_01|LampFluorescent_02/;

// At most N of a prop per floor, in file order. Unlisted props stay.
const LIMIT = [
  [/RadiatorCastIron_01_95/, 3],
  [/FlagHolder_01/, 2],
  [/Urinal_01/, 2],
  [/Chair_01_base/, 5],
  [/Desk_01_Base/, 5],
  [/BenchOffice_01_Brown/, 3],
  [/BenchOffice_01_Base/, 3],
  [/Cupboard_01_/, 4],
  [/CabinetCardFile/, 3],
  [/PlantRoom_0/, 3],
  [/BinInterior_01/, 2],
  [/Bookshelf_01/, 1],
  [/CoatRack_0/, 1],
  [/ClockWall_01/, 1],
  [/WallMap_01/, 1],
  [/NoticeBoardCork_01/, 2],
  [/PaintingBig_0|PaintingSmall_01/, 2],
  [/Armchair_01/, 2],
  [/LampFluorescent_01/, 4],
  [/MirrorBathroom_01|Sink_01|Faucet_01|LightWall_01|LightTable_0/, 2],
  [/CardboardBoxStack_01|Stool_02|Chair_01_white|PhoneTable_01/, 1],
];

const near = (e, x, z, tol = 0.35) => Math.abs(e.x - x) < tol && Math.abs(e.z - z) < tol;

// Ground-floor props the town hall moves out of the way of its own: the meeting table and its two office chairs
// (the lobby is the arrival hall), the two clerk desks and one chair behind the counter (the clerk stands there),
// and the second counter window (the licence desk takes its place).
const GROUND_OUT = [
  [/MeetingTable_01/, 2.4, 4.1],
  [/ChairOffice_01/, 2.8, 3.5],
  [/ChairOffice_01/, 3.0, 4.6],
  [/Notebook_01/, 2.5, 3.6],
  [/Notebook_01/, 2.2, 5.1],
  [/Desk_01_short/, 8.9, 3.7],
  [/Desk_01_short/, 7.2, 3.7],
  [/ChairOffice_01/, 8.3, 3.0],
];

export function makeKeep() {
  const count = new Map();
  return (e) => {
    const name = short(e.path);
    const floor = floorOf(e.y);
    if (floor === 2) return false; // the attic: only dead lamp meshes and an aerial
    if (DROP.test(e.path)) return false;
    if (floor === 0 && GROUND_OUT.some(([rx, x, z]) => rx.test(name) && near(e, x, z))) return false;
    for (const [rx, max] of LIMIT) {
      if (!rx.test(e.path)) continue;
      const key = `${rx.source}@${floor}`;
      const n = (count.get(key) ?? 0) + 1;
      count.set(key, n);
      if (n > max) return false;
    }
    return true;
  };
}

// The licence desk is a post counter window module with the licence action: it stands where the second window
// module stood (x 9.1, z 4.1).
const LIT = "{51611E400CB6FB8A}Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat";
export function transform(e) {
  if (/LampFluorescent_01/.test(e.path)) {
    const id = (/ID "([0-9A-F]+)"/.exec(e.node.lines.find((l) => l.startsWith("ID ")) ?? "")?.[1] ?? "0").padStart(16, "0");
    const assign = { head: `MaterialAssignClass "{DC1F${id.slice(4)}}"`, lines: ["SourceMaterial \"LampFluorescent_01_cover\"", `AssignedMaterial "${LIT}"`], kids: [] };
    const mesh = { head: "MeshObject \"{5872F0EB7FA40561}\"", lines: [], kids: [{ head: "Materials", lines: [], kids: [assign] }] };
    return { ...e, node: { ...e.node, kids: [...e.node.kids, { head: "components", lines: [], kids: [mesh] }] } };
  }
  if (floorOf(e.y) === 0 && /CounterPostOffice_01_window/.test(e.path) && near(e, 9.1, 4.1, 0.1)) {
    return { ...e, cls: "GenericEntity", guid: "{0970D6C0210F9612}", path: "Prefabs/Props/Commercial/LF_LicenceDesk.et" };
  }
  return e;
}
