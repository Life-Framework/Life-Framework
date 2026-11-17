// Which vanilla props of Office_E_01's furniture the bank keeps. Everything here is a base-game prop.
//
// The office is a block of small rooms. The bank takes the ground-floor front-centre room as its banking hall and
// the front-right room as the manager's office; both are cleared of desks, shelves, benches and the sink so the
// bank's own props (layout.mjs) stand in a bare room. Beds and the bath go from every floor (nobody lives in the
// bank). Light switches, radiators, wall lamps and the rest of the rooms stay as the game has them.
// Rects are building space (x, z) on the ground floor (y < 3.2).
export const HALL = { x: [-1.4, 3.4], z: [1.3, 6.6] };
export const MANAGER = { x: [3.55, 7.7], z: [1.3, 6.6] };

const GONE_EVERYWHERE = /BunkBed|Bathtub|Boiler_01|Toilet_01|Bed_0/;
const STAYS_IN_CLEARED = /LightSwitch|Radiator|LightWall|Ladder/;
const inside = (r, e) => e.x >= r.x[0] && e.x <= r.x[1] && e.z >= r.z[0] && e.z <= r.z[1];

export const keep = (e) => {
  const name = e.path ?? "";
  if (GONE_EVERYWHERE.test(name)) return false;
  if (e.y < 3.2 && (inside(HALL, e) || inside(MANAGER, e)) && !STAYS_IN_CLEARED.test(name)) return false;
  return true;
};
