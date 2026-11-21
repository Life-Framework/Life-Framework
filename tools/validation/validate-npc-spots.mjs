// tools/validation/validate-npc-spots.mjs - no two characters on one spot (#3831).
// inputs: addons/**/Worlds/**, addons/**/Configs/Residents/**, addons/LF_Everon1989/Configs/Events/**, tools/lib/npc-spots.mjs
//
// The maintainer saw "two people spawned inside each other" at the cafe on an evening and three beside the
// mayor. Every character put into the world comes from data: a host placed in a layer, a resident's timetable
// line on an anchor, a scene's walker on a spawn marker. tools/lib/npc-spots.mjs reads all of them from the
// flagship world and this check fails when two that can be live together lie within 0.8 m. A resident's slots
// are compared minute by minute (two residents on one bench anchor at 19:30 collide, one at 09:00 and one at
// 21:00 do not); a host is always there; a scene's walkers meet every host and resident but not another scene.
//
// `--report` lists every spot (a pack author's map of who stands where) and always exits 0.
// red-proof: 2026-11-19, with the cafe's old_man and neighbour_mireille both on the one `bench` anchor this printed
// "FAIL resident old_man ... and resident neighbour_mireille ... 0.0 m apart 19:30-22:30" and exited 1;
// tools/validation/npc-spots-selftest.mjs holds the same fixture.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { collectSpots, collisions, describe, MIN_M, sitCollisions } from "../lib/npc-spots.mjs";

const root = process.env.LF_NPC_SPOTS_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { spots, scenes, seats } = collectSpots(root);

if (process.argv.includes("--report")) {
  for (const s of spots) console.log(describe(s));
  console.log(`${spots.length} spot(s), ${scenes.length} scene(s)`);
  process.exit(0);
}

const bad = [...collisions(spots, MIN_M), ...sitCollisions(spots, seats, MIN_M)];
if (bad.length) {
  for (const c of bad) console.log(`FAIL ${describe(c.a)} and ${describe(c.b)}: ${c.d.toFixed(1)} m apart, ${c.shared}`);
  console.log(`validate-npc-spots: ${bad.length} pair(s) closer than ${MIN_M} m. Give each its own anchor or marker; the runtime would move the second one (LF_NpcSpots), which a data fix makes unnecessary.`);
  process.exit(1);
}
console.log(`validate-npc-spots: OK (${spots.length} spot(s), ${scenes.length} scene(s))`);
