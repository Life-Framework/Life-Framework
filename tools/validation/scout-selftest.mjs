// Proves `tools\cli scout` offline (tools/scout/*): the place gazetteer (a label prefab's coordinates and name, the
// string table's offset Texts, "Saint Philippe" finding "Saint-Philippe"), the dump reader, the type vocabulary read off
// vanilla prefab paths, footprints turned by the yaw, entrances and interiors, junctions and the main street, slope,
// free plots that keep off roads and buildings, the markdown, the look flags scout drives, and the argument handling.
// The place is a fixture, tools/scout/fixtures/mini-place.dump.txt: a church, an office, a pub, houses, a shed and a
// ruin on a crossroads plus a T, a strip of sea in the south and ground rising to the east.
// inputs: tools/scout/**, tools/visual/look.mjs, tools/validation/scout-selftest.mjs
// red-proof: 2026-11-16, footprint() ignoring the yaw failed "a building turned 90 degrees swaps its extents";
// pickCandidates() without the two-per-prefab limit failed "a street of one shop is not six pictures";
// blockNames() reading Texts at the same index as Ids failed "the string table's offset is found, not assumed".

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { analyse, catalogBuilding, compass, facing, footprint, inQuad, junctions, largestRect, rot, roadsIn } from "../scout/analyse.mjs";
import { catalogBuildings, matchByPrefab } from "../scout/catalog.mjs";
import { formatDump, parseDump } from "../scout/dump.mjs";
import { blockNames, buildEntries, kindOf, matchPlace, normalise, parseCoords, parseLocationPrefab, placesNear, stringBlock } from "../scout/gazetteer.mjs";
import { renderDossier } from "../scout/render.mjs";
import { areaName, areaSpec, buildDossier, parseScoutArgs, pickCandidates, pickInteriors, resolveTarget, slugOf } from "../scout/scout.mjs";
import { BUILDING_RULES, classify, typeTable } from "../scout/types.mjs";
import { parseArgs, planText, resolveArea } from "../visual/look.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};
const near = (a, b, tol = 0.6) => Math.abs(a - b) <= tol;

// ------------------------------------------------------------------ the gazetteer

const locPrefab = `GenericEntity : "{EB8683B514FBE62D}Prefabs/World/Locations/Common/Urban/LocationUrbanTown.et" {
 ID "5278CCE99D8927EE"
 components {
  SCR_EditableCommentComponent "{529AB059519FE609}" {
   m_UIInfo SCR_EditableCommentUIInfo "{5298E609432D192D}" {
    Name "#AR-MapLocation_StPhillipe"
   }
  }
  SCR_MapDescriptorComponent "{527892B6ED748CEB}" {
   DisplayName "#AR-MapLocation_StPhillipe"
  }
 }
 coords 4502.756 14.702 10771.987
}`;
const lp = parseLocationPrefab(locPrefab);
check("a label prefab's key, parent and coordinates", lp.key === "AR-MapLocation_StPhillipe" && lp.parent === "Prefabs/World/Locations/Common/Urban/LocationUrbanTown.et" && lp.coords[0] === 4502.756 && lp.coords[2] === 10771.987);
check("a prefab with no coords of its own says so (it inherits them)", parseLocationPrefab(`GenericEntity : "{AAAAAAAAAAAAAAAA}Prefabs/X.et" {\n}`).coords === null);
check("kind from the folder", kindOf("Prefabs/World/Locations/Eden/Urban/X.et") === "urban" && kindOf("Prefabs/World/Locations/Eden/Aquatic/X.et") === "aquatic" && kindOf("Prefabs/World/Locations/Eden/St_Philippe.et") === "other");

// A runtime string table whose Texts run 3 ahead of Ids (the real one is 1525 ahead in the AR-MapLocation block).
const table = `StringTableRuntime {
 Ids {
  "AR-A_One"
  "AR-MapLocation_Arleville"
  "AR-MapLocation_Montignac"
  "AR-MapLocation_StPhillipe"
  "AR-MapLocation_Gorey"
 }
 Texts {
  "extra one"
  "extra two"
  "extra three"
  "Intro"
  "Arleville"
  "Montignac"
  "Saint-Philippe"
  "Gorey"
 }
}`;
check("string blocks read quoted strings and undo escapes", stringBlock(`X {\n Ids {\n  "a \\"b\\""\n  "c"\n }\n}`, "Ids").join("|") === 'a "b"|c');
const names = blockNames(table);
check("the string table's offset is found, not assumed", names["AR-MapLocation_StPhillipe"] === "Saint-Philippe" && names["AR-MapLocation_Arleville"] === "Arleville" && names["AR-MapLocation_Gorey"] === "Gorey");
const entries = buildEntries(
  [
    { path: "Prefabs/World/Locations/Eden/Urban/StPhilippe_StPhilippe_01.et", key: "AR-MapLocation_StPhillipe", coords: [4502.756, 14.7, 10771.987] },
    { path: "Prefabs/World/Locations/Eden/St_Philippe.et", key: "AR-MapLocation_StPhillipe", coords: [4428.3, 12.1, 10713.8] },
    { path: "Prefabs/World/Locations/Eden/Urban/Gorey_01.et", key: "AR-MapLocation_Gorey", coords: [4839, 20, 8094] },
    { path: "Prefabs/World/Locations/Eden/Aquatic/St_Philippe_Bay_01.et", key: "AR-MapLocation_CharletBay", coords: [4067, 0, 10948] },
  ],
  { ...names, "AR-MapLocation_CharletBay": "Charlet Bay" },
);
check("Saint Philippe finds Saint-Philippe", matchPlace(entries, "Saint Philippe")[0]?.name === "Saint-Philippe");
check("St. Phillipe (the map's own spelling) finds it too", matchPlace(entries, "St. Phillipe")[0]?.name === "Saint-Philippe" && matchPlace(entries, "AR-MapLocation_StPhillipe")[0]?.key === "AR-MapLocation_StPhillipe");
check("the urban label ranks before the unfiled one", matchPlace(entries, "saint philippe")[0].kind === "urban" && matchPlace(entries, "saint philippe")[1].kind === "other");
check("a substring finds a place", matchPlace(entries, "gore").length === 1 && matchPlace(entries, "nowhere").length === 0);
check("normalise folds saints, hyphens and doubled letters", normalise("Saint-Philippe") === normalise("st phillipe"));
check("x,z and x,z,r parse", parseCoords("4502,10771")?.x === 4502 && parseCoords("4502.5 10771 90")?.radius === 90 && parseCoords("Gorey") === null);
check("places near a point, nearest first", placesNear(entries, 4500, 10770, 200).map((p) => p.name).join() === "Saint-Philippe,Saint-Philippe");

// ------------------------------------------------------------------ the dump

const fixture = readFileSync(join(here, "..", "scout", "fixtures", "mini-place.dump.txt"), "utf8");
const dump = parseDump(fixture);
check("the dump has its request", dump.request.name === "xz-1000-2000" && dump.request.radius === 100);
check("the dump has 13 entities, 14+1 trees, 3 roads, doors on their owner", dump.entities.length === 13 && dump.trees.length === 15 && dump.roads.length === 3 && dump.entities[0].doors.length === 2 && dump.entities[5].doors.length === 0);
check("the dump's ground grid is 41 x 41 with a sea strip", dump.grid.nx === 41 && dump.grid.h.length === 41 && dump.grid.water[0][0] === true && dump.grid.water[10][0] === false);
check("a dump survives format and parse", JSON.stringify(parseDump(formatDump(dump)).entities.map((e) => [e.prefab, e.x, e.doors.length])) === JSON.stringify(dump.entities.map((e) => [e.prefab, e.x, e.doors.length])));
check("a spline the plugin could not read leaves its variable names", parseDump("RV RoadEntity Points Width").roadVars.RoadEntity.join() === "Points,Width");

// ------------------------------------------------------------------ the type vocabulary

const t = (p) => classify(`{0123456789ABCDEF}${p}`);
check("a church", t("Prefabs/Structures/Cultural/Churches/Church_01_white.et").type === "church" && t("Prefabs/Structures/Cultural/Churches/Church_01_white.et").civic);
check("the municipal office, its school conversion and a village house", t("Prefabs/Structures/Services/Government/MunicipalOffice_E_01.et").type === "government" && t("Prefabs/Structures/Services/Government/MunicipalOffice_E_01_school.et").type === "government" && t("Prefabs/Structures/Houses/Village/House_Village_E_1I01.et").type === "house" && t("Prefabs/Structures/Houses/Village/House_Village_E_1I01.et").home);
check("the catalog's names: shop, police villa, town house, fuel station", t("Prefabs/Structures/Commercial/Shops/ShopHouse_E_2I01t_clothesstore.et").type === "shop" && t("Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et").type === "police" && t("Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01.et").type === "townhouse" && t("Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building.et").type === "fuel");
check("a house addon garage is a garage, quay pieces are counted not listed, a ruin is flagged", t("Prefabs/Structures/Houses/Village/HouseAddon_Garage_E_01.et").type === "garage" && t("Prefabs/Structures/Infrastructure/Naval/BreakwaterStone_01_stack_slope.et").group === "infrastructure" && t("Prefabs/Structures/Ruins/House_Ruin_1I01/House_Ruin_1I01_V1.et").ruin);
check("props: pier, fuel pump, bench", t("Prefabs/Structures/Infrastructure/Naval/Pier_Wooden_01.et").type === "pier-or-dock" && t("Prefabs/Structures/BuildingParts/Industrial/FuelPump_01.et").type === "fuel-pump" && t("Prefabs/Props/Civilian/BenchStreet/StreetBench_01/BenchStreet_01_green.et").type === "bench");
check("a kitchen bench is not a street bench", t("Prefabs/Props/Furniture/BenchKitchen_01/BenchKitchen_01_Red.et").type !== "bench");
check("a door and a fence are not buildings", t("Prefabs/Structures/BuildingParts/Doors/Door_House_Town_3I01/Door_X.et").group === "other" && t("Prefabs/Structures/Walls/Wooden/WoodenFenceOld_01_4m.et").group === "wall");
check("every rule names a distinct type once per family", typeTable().length === BUILDING_RULES.length + typeTable().filter((r) => r.kind === "prop" || r.kind === "alias").length);

// ------------------------------------------------------------------ footprints, entrances, interiors

const box = { min: [-6, 0, -10], max: [6, 12, 10] }; // 12 x 20, model space
const e0 = { x: 100, y: 5, z: 200, yaw: 0, scale: 1, min: [94, 5, 190], max: [106, 17, 210] };
const fp0 = footprint(e0, box);
check("a building at yaw 0 is its model's 12 x 20", fp0.w === 12 && fp0.d === 20 && fp0.area === 240 && fp0.source === "measure");
const e90 = { ...e0, yaw: 90, min: [90, 5, 194], max: [110, 17, 206] };
const fp90 = footprint(e90, box);
const xs = fp90.corners.map((c) => c[0]);
const zs = fp90.corners.map((c) => c[1]);
check("a building turned 90 degrees swaps its extents", near(Math.max(...xs) - Math.min(...xs), 20) && near(Math.max(...zs) - Math.min(...zs), 12) && fp90.source === "measure");
check("the model's off-centre origin turns with it", near(footprint({ ...e0, yaw: 90, min: [100, 5, 190], max: [104, 10, 200] }, { min: [0, 0, 0], max: [10, 5, 4] }).cx, 100 + rot(90, 5, 2)[0]) && near(footprint({ ...e0, yaw: 90, min: [100, 5, 190], max: [104, 10, 200] }, { min: [0, 0, 0], max: [10, 5, 4] }).cz, 200 + rot(90, 5, 2)[1]));
check("no measured model falls back to the world bounds", footprint(e0, null).source === "world-bounds" && footprint(e0, null).w === 12);
check("engine bounds much larger than the model win, said so", footprint({ ...e0, min: [90, 5, 180], max: [110, 17, 220] }, box).source === "world-bounds" && /larger/.test(footprint({ ...e0, min: [90, 5, 180], max: [110, 17, 220] }, box).note));
check("inside and outside a quad, with a margin", inQuad(fp0.corners, 100, 200) && !inQuad(fp0.corners, 107, 200) && inQuad(fp0.corners, 107, 200, 2));
check("compass points", compass(0, 1) === "N" && compass(1, 0) === "E" && compass(-1, -1) === "SW" && facing(90) === "E" && facing(270) === "W");

const measured = {
  "Prefabs/Structures/Cultural/Churches/Church_01_white.et": { min: [-6, 0, -10], max: [6, 12, 10] },
  "Prefabs/Structures/Services/Government/MunicipalOffice_E_01.et": { min: [-5, 0, -8], max: [5, 9, 8] },
  "Prefabs/Structures/Commercial/Pubs/PubVillage_E_1L01.et": { min: [-6, 0, -4], max: [6, 7, 4] },
  "Prefabs/Structures/Houses/Village/House_Village_E_1I01.et": { min: [-4.5, 0, -3.5], max: [4.5, 7, 3.5] },
};
const centre = { x: 1000, z: 2000 };
const a = analyse(dump, centre, 100, measured);
const bt = (type) => a.buildings.filter((b) => b.type === type);
const church = bt("church")[0];
const office = bt("government")[0];
check("13 entities sort into 7 buildings and 4 notable props (+2 lamps counted)", a.buildings.length === 7 && a.summary.props === 5 && a.propCounts["street-lamp"] === 2 && a.propCounts.bench === 1 && a.propCounts["pier-or-dock"] === 1 && a.propCounts["fuel-pump"] === 1);
check("the fence is counted, not listed", a.summary.walls === 1);
check("buildings are nearest first with ids", a.buildings[0].id === "b01" && a.buildings.every((b, i) => i === 0 || b.distance >= a.buildings[i - 1].distance));
check("the church footprint is 12 x 20 from the measure", church.footprint.w === 12 && church.footprint.d === 20 && church.footprint.source === "measure" && church.civic);
check("the office at yaw 90 spreads east-west", near(Math.max(...office.footprint.corners.map((c) => c[0])) - Math.min(...office.footprint.corners.map((c) => c[0])), 16) && office.facing === "E");
check("the shed has no measured model: world bounds", bt("shed")[0].footprint.source === "world-bounds");
check("a door on the church's front is a front entrance facing north, its inner door is not an entrance", church.doors === 2 && church.entrances.length === 1 && church.entrances[0].side === "front" && church.entrances[0].facing === "N");
check("the office door is on its front too, facing east", office.entrances[0].side === "front" && office.entrances[0].facing === "E");
check("interior: door means yes, a pub with none is unknown, a shed no, a ruin no", church.interior.value === "yes" && bt("pub")[0].interior.value === "unknown" && bt("shed")[0].interior.value === "no" && bt("ruin")[0].interior.value === "no");
check("the summary counts", a.summary.homes === 2 && a.summary.civic === 3 && a.summary.ruins === 1 && a.summary.enterable === 4 && a.summary.entrances === 4 && a.summary.doors === 5);

// ------------------------------------------------------------------ the catalog as the source of buildings

const cat = JSON.parse(readFileSync(join(here, "..", "scout", "fixtures", "mini-catalog.json"), "utf8"));
const placeFile = JSON.parse(readFileSync(join(here, "..", "scout", "fixtures", "mini-place.json"), "utf8"));
const cb = catalogBuildings(cat, 1000, 2000, 100);
check("the catalog's buildings within the radius: 8 of 9 instances (one is 4 km away)", cb.length === 8 && cb.every((b) => b.res && b.type && b.footprint));
check("a building comes back with its type, extents and floor levels", cb[0].type === "church" && cb[0].footprint[0] === 12 && cb[0].floorLevelsY[1] === 3.4 && cb[0].enterable);
check("matching by prefab and position", matchByPrefab(placeFile.buildings, cb[0].path, 1020, 2020) !== null && matchByPrefab(placeFile.buildings, cb[0].path, 1100, 2020) === null && matchByPrefab(placeFile.buildings, "Prefabs/Other.et", 1020, 2020) === null);
const ac = analyse(dump, centre, 100, {}, { catalog: { buildings: cb, placeFile } });
const cchurch = ac.buildings.find((b) => b.type === "church");
check("with the catalog, 7 buildings are listed and the pier piece is counted", ac.buildings.length === 7 && ac.summary.portPieces === 1 && !ac.buildings.some((b) => b.type === "port"));
check("their footprints say catalog, with the catalog's extents", cchurch.footprint.source === "catalog" && cchurch.footprint.w === 12 && cchurch.footprint.d === 20);
check("floors, first floor and interior come from the catalog", cchurch.floors === 2 && cchurch.firstFloor === 0.2 && cchurch.interior.value === "yes" && /catalog/.test(cchurch.interior.basis));
check("a doorless building the catalog calls not enterable is no, a shed with a door but no floor is unknown", ac.buildings.find((b) => b.type === "pub").interior.value === "no" && ac.buildings.find((b) => b.type === "shed").interior.value === "no");
check("the dump adds the door positions to the matched buildings", cchurch.matched && cchurch.entrances.length === 1 && cchurch.entrances[0].side === "front" && cchurch.doors === 2);
check("a building with no entity in the dump has no entrances and says so", (() => {
  const lone = analyse({ ...dump, entities: dump.entities.filter((e) => !/Church/.test(e.prefab)) }, centre, 100, {}, { catalog: { buildings: cb, placeFile } }).buildings.find((b) => b.type === "church");
  return lone.matched === false && lone.entrances.length === 0 && lone.entrancesKnown === false && lone.interior.value === "yes";
})());
check("the place file adds what stands inside", cchurch.furnitureInside === 66 && /BenchChurch/.test(cchurch.topProps));
check("catalog buildings have the same civic, home and ruin flags", cchurch.civic && ac.buildings.find((b) => b.type === "house").home && ac.buildings.find((b) => b.type === "ruin").ruin && ac.summary.civic === 3 && ac.summary.homes === 2);
check("the notes name the source", ac.notes.some((n) => /read from the Everon catalog/.test(n) && /of 7 matched/.test(n)));
const shifted = catalogBuilding(cb[0], { ...dump.entities[0], min: [1015, 12, 2010], max: [1027, 24, 2030] }, centre);
check("a model offset from its origin is found from the engine's bounds", near(shifted.footprint.cx, 1021) && /centred on the engine/.test(shifted.footprint.note) && catalogBuilding(cb[0], dump.entities[0], centre).footprint.cx === 1020);
check("with no catalog the dump's entities are typed by path as before", analyse(dump, centre, 100, measured).buildings.length === 7 && analyse(dump, centre, 100, measured).buildings[0].footprint.source !== "catalog");

// ------------------------------------------------------------------ roads

check("three roads, all inside the circle", a.roads.length === 3 && a.roads[0].length === 200);
const jn = a.junctions;
check("the crossroads has four arms, the lane makes a T", jn.length === 2 && jn[0].arms === 4 && jn[0].kind === "crossroads" && Math.abs(jn[0].x - 1000) < 1 && jn[1].arms === 3 && jn[1].kind === "t-junction" && Math.abs(jn[1].x - 1050) < 1);
check("an unnamed road is named by the way it runs", roadsIn([{ cls: "RoadEntity", name: "", points: [[900, 0, 2000], [1100, 0, 2000]] }], centre, 150)[0].name === "E-W road r01" && roadsIn([{ cls: "RoadEntity", name: "", points: [[1000, 0, 1900], [1000, 0, 2100]] }], centre, 150)[0].name === "N-S road r01");
check("a road with one point inside is dropped", roadsIn([{ cls: "RoadEntity", name: "far", points: [[5000, 0, 5000], [5100, 0, 5000]] }], centre, 100).length === 0);
check("two parallel roads do not meet", junctions([{ id: "a", points: [[0, 0], [100, 0]] }, { id: "b", points: [[0, 10], [100, 10]] }]).length === 0);
check("a road ending on another's side is a T", junctions([{ id: "a", points: [[0, 0], [100, 0]] }, { id: "b", points: [[50, 1], [50, 60]] }])[0]?.arms === 3);
check("the main street is the road the buildings stand on", a.mainStreet && a.mainStreet.buildingsAlong >= 3 && /MainRoad|ChurchStreet/.test(a.mainStreet.road) && ["E-W", "N-S"].includes(a.mainStreet.runs));

// ------------------------------------------------------------------ terrain

const tr = a.terrain;
check("ground rises from 10 to 20 m", near(tr.min, 10.4, 1.5) && near(tr.max, 20, 1.5));
check("the ground falls toward the west (it rises to the east)", tr.tilt.fallsToward === "W" && near(tr.tilt.deg, 2.9, 0.4));
check("the sea is to the south", tr.nearestWater?.direction.includes("S") && tr.waterShare > 0 && tr.waterShare < 0.4);
check("terrain notes are sentences", tr.notes.length >= 3 && tr.notes.every((n) => typeof n === "string"));

// ------------------------------------------------------------------ free plots

const plots = a.freePlots.plots;
check("free plots found", plots.length >= 3 && plots.length <= 8);
const roadHalf = 4.5;
const noClash = plots.every((p) => {
  const ok = [[-1, -1], [1, -1], [1, 1], [-1, 1]].every(([sx, sz]) => {
    const x = p.x + (sx * p.width) / 2 * 0.98;
    const z = p.z + (sz * p.depth) / 2 * 0.98;
    return a.buildings.every((b) => !inQuad(b.footprint.corners, x, z, 2)) && a.roads.every((r) => Math.min(...r.points.slice(1).map((q, i) => distSeg([x, z], r.points[i], q))) > roadHalf - 0.5);
  });
  return ok;
});
function distSeg(p, a1, b1) {
  const [dx, dz] = [b1[0] - a1[0], b1[1] - a1[1]];
  const l2 = dx * dx + dz * dz;
  const tt = l2 ? Math.max(0, Math.min(1, ((p[0] - a1[0]) * dx + (p[1] - a1[1]) * dz) / l2)) : 0;
  return Math.hypot(p[0] - (a1[0] + tt * dx), p[1] - (a1[1] + tt * dz));
}
check("no plot corner is in a building or on a road", noClash);
check("no plot is in the sea", plots.every((p) => p.z - p.depth / 2 > 1925));
check("plots do not overlap and run biggest first", plots.every((p, i) => i === 0 || p.area <= plots[i - 1].area + 1) && plots.every((p, i) => plots.slice(0, i).every((q) => Math.abs(p.x - q.x) >= (p.width + q.width) / 2 - 0.1 || Math.abs(p.z - q.z) >= (p.depth + q.depth) / 2 - 0.1)));
check("a plot names its nearest road and building", plots.every((p) => p.nearestRoad && p.nearestBuilding));
const blocked = Array.from({ length: 6 }, (_, j) => Uint8Array.from({ length: 6 }, (_, i) => (i === 2 && j === 3 ? 1 : 0)));
const lr = largestRect(blocked, 6);
check("the largest rectangle skips a blocked cell", lr.w * lr.h === 18 && !(lr.i <= 2 && 2 < lr.i + lr.w && lr.j <= 3 && 3 < lr.j + lr.h));
check("no grid, no plots, and the note says why", /no ground grid/.test(analyse({ ...dump, grid: null }, centre, 100, measured).freePlots.note));

// ------------------------------------------------------------------ the dossier

check("slugs fold accents and punctuation", slugOf("Saint-Philippe") === "saint-philippe" && slugOf("Étoupe") === "etoupe" && slugOf("La  Valette!") === "la-valette");
const target = resolveTarget(["Saint", "Philippe"], entries);
check("a name resolves to the label's coordinates", target.name === "Saint-Philippe" && target.slug === "saint-philippe" && target.x === 4502.756 && target.source === "gazetteer" && target.others.length === 1);
const tc = resolveTarget(["1000,2000"], entries);
check("coordinates resolve to an xz slug", tc.slug === "xz-1000-2000" && tc.source === "coordinates" && tc.x === 1000);
let msg = "";
try {
  resolveTarget(["Atlantis"], entries);
} catch (e) {
  msg = e.message;
}
check("an unknown place says how to find one", /no named place matches "Atlantis"/.test(msg) && /scout list/.test(msg));
const args = parseScoutArgs(["Saint", "Philippe", "--radius", "90", "--quick", "--interiors", "1"]);
check("args: the name is positional, flags take values", args.pos.join(" ") === "Saint Philippe" && args.flags.radius === "90" && args.flags.quick === true && args.flags.interiors === "1");

const cands = pickCandidates(a.buildings, 6);
check("civic candidates are the church, office and pub, nearest first, with a view radius", cands.map((b) => b.type).sort().join() === "church,government,pub" && cands.every((b) => b.view >= 14 && b.view <= 40));
const shops = Array.from({ length: 6 }, (_, i) => ({ id: `b0${i}`, stem: "ShopHouse_E_2I01t_foodstore", type: "shop", civic: true, ruin: false, distance: i, footprint: { w: 10, d: 8 } }));
check("a street of one shop is not six pictures", pickCandidates(shops, 6).length === 2);
const stackedPair = [{ id: "b01", stem: "Church_01_red", type: "church", civic: true, ruin: false, x: 10, z: 10, distance: 5, footprint: { w: 20, d: 33.8, area: 676 } }, { id: "b02", stem: "Church_01_crypt_attic", type: "church", civic: true, ruin: false, x: 10, z: 10, distance: 5, footprint: { w: 16.5, d: 28, area: 462 } }];
check("two models on one spot are one picture, the bigger one", pickCandidates(stackedPair, 6).map((b) => b.id).join() === "b01" && pickCandidates([...stackedPair].reverse(), 6).map((b) => b.id).join() === "b01");
const ins = pickInteriors(cands, 5);
check("interiors: only buildings with a door entity, the church first, with a room to stand in", ins.length === 2 && ins[0].type === "church" && ins[0].room.radius >= 3 && near(ins[0].room.x, church.footprint.cx));
check("the look area spec and name agree", areaSpec(1000.04, 2000.46, 30) === "1000,2000.5,30" && areaName(1000.04, 2000.46) === "xz-1000-2001");

const dossier = buildDossier({ target, radius: 100, analysis: a, pictures: [{ role: "overview", file: "img/overview-top.jpg", caption: "from above" }, { role: "civic", building: church.id, file: "img/civic-x-wide.jpg", caption: "church" }, { role: "interior", building: church.id, file: null, caption: "inside", missing: "no pictures" }], build: "Arma Reforger build 1", world: "Worlds/CoreOnEveron/CoreOnEveron.ent", at: "2026-11-16", command: 'tools\\cli scout "Saint-Philippe"', nearby: [{ name: "Charlet Bay", kind: "aquatic", distance: 440, direction: "W" }] });
check("dossier.json carries the schema and every section", dossier.schema === 1 && ["summary", "terrain", "mainStreet", "roads", "junctions", "buildings", "props", "freePlots", "pictures", "notes"].every((k) => k in dossier) && JSON.parse(JSON.stringify(dossier)).buildings.length === 7);
const md = renderDossier(dossier);
check("the markdown has its sections", ["# Saint-Philippe: location dossier", "## At a glance", "## Terrain", "## Roads and junctions", "## Civic candidates", "## All buildings", "## Notable props", "## Free plots", "## Limits of this dossier", "## Prefab references"].every((h) => md.includes(h)));
check("the markdown links the pictures and names the missing one", md.includes("![from above](img/overview-top.jpg)") && /picture `interior b\d\d` missing: no pictures/.test(md));
check("the markdown cites the prefab as {GUID}path", md.includes("`{AAAAAAAAAAAAAAAA}Prefabs/Structures/Cultural/Churches/Church_01_white.et`"));
check("the same JSON renders the same text", renderDossier(dossier) === md);

// ------------------------------------------------------------------ the look flags scout drives

const areas = [resolveArea("1000,2000,60", [], undefined)];
check("look takes x,z,r: a radius of its own", areas[0].radius === 60 && areas[0].name === "xz-1000-2000");
check("look's plan carries a dump line only when asked", !/^dump /m.test(planText([], areas, { world: "W.ent" })) && /^dump xz-1000-2000 1000 2000 120 5$/m.test(planText([], areas, { world: "W.ent", dump: { radius: 120, step: 5 } })));
check("look takes --dump and --dumpstep with values", parseArgs(["1,2", "--dump", "120", "--dumpstep", "4"]).flags.dump === "120" && parseArgs(["1,2", "--dump", "120", "--dumpstep", "4"]).flags.dumpstep === "4");
check("a dump with no radius of its own uses each area's", /^dump xz-1000-2000 1000 2000 60 5$/m.test(planText([], areas, { world: "W.ent", dump: { radius: undefined, step: 5 } })));

if (failures.length) {
  for (const f of failures) console.error(`FAIL  scout-selftest: ${f}`);
  process.exit(1);
}
console.log("scout-selftest: OK (gazetteer, dump reader, type vocabulary, footprints, entrances, interiors, junctions, main street, slope, free plots, dossier markdown, look flags, args)");
