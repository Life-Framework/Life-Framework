// tools/town/stamp.selftest.mjs: `town stamp` on tools/town/fixtures/stamp/ (card #3881). No game, no server.
// The fixture place `stampville` has two buildings of one prefab (b1 at yaw 0, b2 at yaw 180) mapped to the kit's two slots, and
// a third of another prefab (b3). The kit is lifted by `town author extract` from the fixture grid layer, as a real author run does.
//   - stamping twice writes the same bytes (layers and claims);
//   - a moved kit row moves in both stamped layers, turned by each building's yaw;
//   - a stamp onto a building whose prefab differs from the kit is refused and writes nothing;
//   - a stamped row with a floating or blocks-door finding refuses the stamp and puts the previous bytes back; a contact
//     finding (not hard) does not.
// red-proof: 2026-11-21, planStamp with the prefab comparison removed printed "FAIL a stamp onto b3 (another prefab) is refused";
//   applyStamp with the restore loop removed printed "FAIL a floating finding restores the previous layer"; restored, OK.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { authorExtract } from "./author.mjs";
import { applyStamp, planStamp, stampTargets, stampVerdict } from "./stamp.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
const FIX = join(HERE, "fixtures", "stamp");
const fails = [];
const ok = (cond, msg) => {
  if (!cond) fails.push(msg);
};
const near = (a, b) => Math.abs(a - b) < 0.0015;
const now = () => new Date("2026-11-21T12:00:00Z");
const quiet = () => {};

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
const kit = authorExtract(join(FIX, "Grid_stamp_fixture.layer"), "Shop_*", { kit: "fixture-shop" });
const opts = { placesDir: join(FIX, "places"), templatesDir: join(FIX, "templates") };
const targets = stampTargets("fixture-shop", opts);
ok(targets.length === 2 && targets.every((t) => t.building), `two mapped copies found (got ${targets.length})`);

const out = scratchDir("lf-stamp-");
const po = { root: ROOT, outRoot: out, layerDir: "layers", claimsDir: "claims", skipClaims: true, now };
const coordsOf = (text, id) => {
  const m = new RegExp(`_${id} : "[^"]+" \\{[\\s\\S]*?coords ([-\\d. ]+)`).exec(text);
  return m ? m[1].trim().split(" ").map(Number) : null;
};
const read = (f) => (existsSync(join(out, f)) ? readFileSync(join(out, f), "utf8") : null);

// stamp, then stamp again: same bytes
const p1 = planStamp(kit, targets, po);
ok(p1.ok, `the fixture stamps (${p1.errors.join("; ")})`);
ok(applyStamp(p1, { root: out, log: quiet }) === 0, "first stamp writes");
const files = p1.copies.flatMap((c) => [c.layerFile, c.claimFile]);
const first = files.map(read);
const p2 = planStamp(kit, targets, { ...po, now: () => new Date("2026-12-01T00:00:00Z") });
ok(applyStamp(p2, { root: out, log: quiet }) === 0, "second stamp runs");
ok(files.every((f, i) => read(f) === first[i] && first[i] !== null), "stamping twice writes the same bytes (layers and claims, a later date kept out)");

// the composed transform: counter at building-local (2, 0.2, 3); b1 yaw 0 at 5000,20,6000; b2 yaw 180 at 5100,25.5,6050
const counterId = "counter";
const a1 = coordsOf(first[0], counterId);
const b1 = coordsOf(first[2], counterId);
ok(a1 && near(a1[0], 5002) && near(a1[1], 20.2) && near(a1[2], 6003), `counter in b1 at 5002 20.2 6003 (got ${a1})`);
ok(b1 && near(b1[0], 5098) && near(b1[1], 25.7) && near(b1[2], 6047), `counter in b2 at 5098 25.7 6047 (got ${b1})`);
ok(!/coords 1003 /.test(first[0]), "no grid coordinate leaks into a stamped layer");

// a moved kit row moves in both copies
const moved = JSON.parse(JSON.stringify(kit));
moved.rows.find((r) => r.id === counterId).offset[0] += 0.5;
const p3 = planStamp(moved, targets, po);
const a3 = coordsOf(p3.copies[0].layerText, counterId);
const b3 = coordsOf(p3.copies[1].layerText, counterId);
ok(a3 && near(a3[0], 5002.5) && near(a3[2], 6003), `the moved row moves in b1 (got ${a3})`);
ok(b3 && near(b3[0], 5097.5) && near(b3[2], 6047), `the moved row moves in b2, turned by its yaw (got ${b3})`);

// a building of another prefab is refused
const wrong = targets.map((t, i) => (i === 1 ? { ...t, building: t.dossier.buildings.find((b) => b.id === "b3") } : t));
const p4 = planStamp(kit, wrong, po);
ok(!p4.ok && p4.errors.some((e) => /b3 .*same prefab/.test(e)), `a stamp onto b3 (another prefab) is refused (${p4.errors.join("; ")})`);
ok(applyStamp(p4, { root: out, log: quiet }) === 1 && files.every((f, i) => read(f) === first[i]), "a refused plan writes nothing");

// the island audit is the verdict: floating and blocks-door refuse and restore; contact does not
const layerName = p3.copies[0].layerFile.split("/").pop();
const report = (kind) => ({ ranked: [{ name: `Fit_stampville_shop_a_${counterId}`, layer: layerName, problems: [{ kind, text: kind }] }] });
for (const kind of ["floating", "blocks-door"]) {
  ok(stampVerdict(report(kind), [layerName]).length === 1, `${kind} is a hard finding`);
  const rc = applyStamp(p3, { root: out, log: quiet, audit: () => ({ report: report(kind) }) });
  ok(rc === 1, `a ${kind} finding refuses the stamp`);
  ok(files.every((f, i) => read(f) === first[i]), `a ${kind} finding restores the previous layer`);
}
ok(stampVerdict(report("floating"), ["Place_other_slot.layer"]).length === 0, "a finding on another layer is not the stamp's");
ok(applyStamp(p3, { root: out, log: quiet, audit: () => ({ report: report("contact") }) }) === 0, "a contact finding does not refuse");
ok(read(p3.copies[0].layerFile) === p3.copies[0].layerText, "the accepted re-stamp is on disk");
ok(applyStamp(p3, { root: out, log: quiet, audit: () => ({ error: "no server" }) }) === 1, "an audit that did not run refuses");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("town stamp selftest: OK");
}
