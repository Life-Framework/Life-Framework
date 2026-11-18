// tools/town/doors.selftest.mjs - acceptance for slice #3202 (`town doors`: every enterable building has a purpose and a carrier).
//
// A house left open by accident is an enterable building with no purpose row, or a purpose row nothing in the world carries. `town doors`
// (tools/town/doors.mjs) lists every enterable building of a place's dossiers and fails on either. Run: node tools/town/doors.selftest.mjs
// The fixture (tools/town/fixtures/doors/) is two surveys of a made-up town: a shop, a barn, a shed beside the shop, a wall with no door,
// a district hut, and the district survey's second record of the shop.
//   - the barn with no marker fails (the acceptance), and passes once the lock marker (LF_BuildingLockComponent) stands in its footprint;
//   - --planned makes a planned row without a carrier a warning, but a row marked `exists` stays an error;
//   - an enterable building with no purpose row fails, with or without --planned;
//   - the district survey's second record of the shop counts once, and a wall with no door is not enterable;
//   - a purpose row naming a building the dossiers do not have fails;
//   - a marker three metres outside the footprint still counts, one far away does not, and a marker serves only its nearest building;
//   - a row with `partOf` takes its parent's carrier;
//   - the committed Saint-Philippe place lists 130 buildings and fails only for carriers (`--planned` makes it pass).
// red-proof: with LF_BuildingLockComponent missing from CARRIERS this printed "FAIL barn [lock marker present]: expected pass, got fail" (2026-11-18).
// red-proof: with assignMarkers taking every marker inside the slack the "marker serves only its nearest building" case printed FAIL (2026-11-18).

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "town", "fixtures", "doors");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};
const read = (f) => readFileSync(join(FIX, f), "utf8");
const json = (f) => JSON.parse(read(f));

// The `town` dispatcher imports every tools/town/*.mjs to list its steps: run only when launched directly.
if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const { checkDoors, layerMarkers, distToPolygon, enterableBuildings, loadPlace } = await import(pathToFileURL(join(ROOT, "tools", "town", "doors.mjs")).href);

  const surveys = [
    { prefix: "sp", buildings: json("dossier.json").buildings },
    { prefix: "di", buildings: json("dossier-districts.json").buildings },
  ];
  const fitted = layerMarkers(read("fitted.layer"), "fitted.layer");
  const lock = layerMarkers(read("lock.layer"), "lock.layer");
  const house = layerMarkers(read("house.layer"), "house.layer");
  const purpose = json("purpose.json");
  const run = (o) => checkDoors({ purpose, surveys, markers: [], planned: false, ...o });
  const has = (list, sub) => list.some((m) => m.includes(sub));
  const row = (res, id) => res.rows.find((r) => r.id === id);

  // ---------------------------------------------------------------- the readers
  check(fitted.length === 1 && fitted[0].comps[0] === "LF_FittedBuildingComponent" && fitted[0].x === 101 && fitted[0].z === 99, `layerMarkers reads the fitted marker (got ${JSON.stringify(fitted)})`);
  check(lock.length === 1 && lock[0].comps[0] === "LF_BuildingLockComponent", "layerMarkers reads the lock marker");
  check(distToPolygon([[0, 0], [10, 0], [10, 10], [0, 10]], 5, 5) === 0, "a point inside a polygon is at distance 0");
  check(Math.abs(distToPolygon([[0, 0], [10, 0], [10, 10], [0, 10]], 13, 5) - 3) < 1e-9, "a point 3 m outside is at distance 3");
  const ids = enterableBuildings(surveys).map((b) => b.id).sort().join(",");
  check(ids === "di:b10,sp:b01,sp:b02,sp:b04", `enterable buildings count once and skip the wall (got ${ids})`);

  // ---------------------------------------------------------------- the acceptance: the barn
  let res = run({ markers: fitted.concat(house) });
  check(res.errors.length > 0 && has(res.errors, "sp:b02") && has(res.errors, "no carrier"), `barn [no marker]: expected an error naming sp:b02, got ${JSON.stringify(res.errors)}`);
  check(res.rows.length === 4, `barn [no marker]: expected 4 buildings listed, got ${res.rows.length}`);
  res = run({ markers: fitted.concat(house, lock) });
  check(res.errors.length === 0, `barn [lock marker present]: expected pass, got fail ${JSON.stringify(res.errors)}`);
  check(row(res, "sp:b02")?.carriers[0]?.kind === "lock", "barn [lock marker present]: carrier kind is lock");
  check(row(res, "sp:b01")?.carriers[0]?.kind === "fitted" && row(res, "di:b10")?.carriers[0]?.kind === "house", "shop is carried by its fitted marker, the hut by its house marker");

  // ---------------------------------------------------------------- --planned
  res = run({ markers: fitted.concat(house), planned: true });
  check(res.errors.length === 0 && has(res.warnings, "sp:b02"), `--planned: a planned row with no carrier is a warning, got ${JSON.stringify(res)}`);
  const exists = { ...purpose, buildings: purpose.buildings.map((r) => (r.id === "sp:b02" ? { ...r, status: "exists" } : r)) };
  res = run({ markers: fitted.concat(house), planned: true, purpose: exists });
  check(has(res.errors, "sp:b02"), "--planned: a row marked exists with no carrier stays an error");

  // ---------------------------------------------------------------- no purpose row, orphan row
  const noRow = { ...purpose, buildings: purpose.buildings.filter((r) => r.id !== "di:b10") };
  for (const planned of [false, true]) {
    res = run({ markers: fitted.concat(house, lock), purpose: noRow, planned });
    check(has(res.errors, "di:b10") && has(res.errors, "no purpose row"), `hut with no purpose row [planned=${planned}]: expected an error, got ${JSON.stringify(res.errors)}`);
  }
  const orphan = { ...purpose, buildings: purpose.buildings.concat([{ id: "sp:b99", purpose: "e", use: "ghost", status: "planned" }]) };
  res = run({ markers: fitted.concat(house, lock), purpose: orphan });
  check(has(res.errors, "sp:b99"), "a purpose row for a building the dossiers lack is an error");

  // ---------------------------------------------------------------- marker distance and ownership
  const at = (x, z) => [{ file: "x", line: 1, name: "M", x, z, comps: ["LF_BuildingLockComponent"] }];
  res = run({ markers: at(208, 100) });
  check(row(res, "sp:b02")?.carriers.length === 1, "a marker 3 m outside the footprint still counts");
  res = run({ markers: at(209, 100) });
  check(row(res, "sp:b02")?.carriers.length === 0, "a marker 4 m outside the footprint does not count");
  // A marker at z 106 is 1 m from the shop footprint and 2 m from the shed beside it: both inside the slack, it serves the nearer.
  const shed = { ...json("dossier.json").buildings[3], footprint: { corners: [[95, 108], [105, 108], [105, 118], [95, 118]] }, z: 113 };
  const near = checkDoors({
    purpose,
    surveys: [{ prefix: "sp", buildings: [json("dossier.json").buildings[0], shed] }],
    markers: at(100, 106),
    planned: true,
  });
  check(row(near, "sp:b01")?.carriers.length === 1 && row(near, "sp:b04")?.carriers.length === 1, "marker serves only its nearest building [shop gets it, the shed falls back to partOf]");
  check(row(near, "sp:b04")?.carriers[0]?.via === "sp:b01", "marker serves only its nearest building [the shed's carrier is the parent's, not its own]");

  // ---------------------------------------------------------------- partOf
  res = run({ markers: fitted.concat(house, lock) });
  check(row(res, "sp:b04")?.carriers[0]?.via === "sp:b01" && !has(res.errors, "sp:b04"), "a partOf row takes its parent's carrier");
  const noParent = run({ markers: house.concat(lock) });
  check(has(noParent.errors, "sp:b04") && has(noParent.errors, "sp:b01"), "a partOf row has no carrier when its parent has none");

  // ---------------------------------------------------------------- the committed place
  const real = loadPlace(ROOT, "saint-philippe");
  const planned = checkDoors({ ...real, planned: true });
  check(planned.rows.length === 130, `Saint-Philippe lists 130 buildings, got ${planned.rows.length}`);
  check(planned.errors.length === 0, `Saint-Philippe --planned has no errors, got ${planned.errors.length}: ${planned.errors.slice(0, 3).join(" | ")}`);
  const strict = checkDoors({ ...real, planned: false });
  check(strict.rows.every((r) => r.purpose), "every Saint-Philippe building has a purpose row");

  if (failed) {
    console.log(`\n${failed} check(s) failed`);
    process.exit(1);
  }
  console.log("town doors selftest: all passed");
}
