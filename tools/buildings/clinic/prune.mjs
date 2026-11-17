// Which vanilla props of House_Town_E_2I02's furniture (262 children) the clinic keeps. Everything here is a base-game
// prop. The house is a flat; the clinic takes the ground floor (a waiting room and reception in the east wing, a
// treatment room in the old bedroom, a ward in the tiled west room) and leaves the first floor as the doctor's flat.
//
// Building space (x, z) on the ground floor (y < 3.9). The attic (y >= 7.0: crates and dead lamp meshes) and every
// light switch go, as the other fit-outs do, to spend the entity budget on the rooms people use.
import { floorOf } from "./furniture-lib.mjs";
import { RACKS_UNTIL } from "./layout.mjs";

export const WARD = { x: [-9.5, -4.25], z: [-3.6, 5.6] };
export const WAITING = { x: [4.0, 9.2], z: [0.3, 5.7] };
export const TREATMENT = { x: [-0.65, 3.5], z: [0.3, 5.7] };
export const CORRIDOR = { x: [-4.25, -0.65], z: [-1.5, 5.7] };

const inside = (r, e) => e.x >= r.x[0] && e.x <= r.x[1] && e.z >= r.z[0] && e.z <= r.z[1];
// (the khaki phone stood on the living room's dresser, which goes; the suitcases and the kindling stood a little off
// the first floor's slab)
const DROP_EVERYWHERE = /LightSwitch_02|TvAntenna|Typewriter_01_orange|PhoneTable_01_khaki|Suitcase_01|WoodKindling_01\.et/;
// the ward is cleared down to its racks, wall lamps and the electrics
const WARD_STAYS = /Rack_01|LightWall|UtilityBox|RadiatorCastIron/;
// reception and the waiting room lose the living room's desk corner and sofa
const WAITING_GONE = /Desk_01_Base|Chair_02_red|LampInterior_01|PhoneTable_01_khaki|Sofa_01|Dresser_01|PlantRoom_01/;
// the treatment room loses the bedroom: bed, wardrobe, carpet and bookshelf
const TREATMENT_GONE = /Bed_01|Wardrobe_01|Carpet_01|Bookshelf_01|Nightstand/;
// the corridor loses its stacked boxes
const CORRIDOR_GONE = /BoxWooden_01|CrateWooden_01_plain|Crate_01/;
const BATH_GONE = /Bathtub_01|ShowerDrain/;

export const keepFor = (pass) => (e) => {
  const name = e.path ?? "";
  const floor = floorOf(e.y);
  if (floor === 2) return false;
  if (DROP_EVERYWHERE.test(name)) return false;
  if (floor === 0) {
    if (inside(WARD, e) && !WARD_STAYS.test(name)) return false;
    if (pass > RACKS_UNTIL && inside(WARD, e) && /Rack_01/.test(name)) return false;
    if (inside(WAITING, e) && WAITING_GONE.test(name)) return false;
    if (inside(TREATMENT, e) && TREATMENT_GONE.test(name)) return false;
    if (inside(CORRIDOR, e) && CORRIDOR_GONE.test(name)) return false;
    if (BATH_GONE.test(name)) return false;
  }
  return true;
};
