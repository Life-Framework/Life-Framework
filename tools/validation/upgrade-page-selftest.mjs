// Proves `tools\cli upgrade-page` (#294): two fixture schemas, two record-shape files, two migration
// manifests and two pack manifests go in; the section, field, type, default, range, migration, save
// version and pack-range changes come out, and the filled page has no template placeholder left that the
// generator could have known. Fixtures: tools/validation/fixtures/upgrade-page/.
// inputs: tools/**
//
// red-proof (#294): with the default comparison in `diffSchemas` (tools/upgrade/page.mjs) removed, the run
// printed "FAIL default change: shops.defaultBuyPrice" and exited 1; restored, it prints
// "upgrade-page-selftest: OK".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { diffAll, diffSchemas, parseManifest, renderPage } from "../upgrade/page.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (f) => JSON.parse(readFileSync(join(here, "fixtures", "upgrade-page", f), "utf8"));
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const s = diffSchemas(fx("from.schema.json"), fx("to.schema.json"));
const paths = (l) => l.map((x) => x.path ?? x).sort().join(",");
expect(paths(s.addedSections) === "weather", `added section: ${paths(s.addedSections)}`);
expect(paths(s.removedSections) === "legacy", `removed section: ${paths(s.removedSections)}`);
expect(paths(s.added) === "shops.newFlag", `added field: ${paths(s.added)}`);
expect(paths(s.removed) === "shops.oldFlag", `removed field: ${paths(s.removed)}`);
expect(paths(s.retyped) === "shops.taxRate" && s.retyped[0].was === "number" && s.retyped[0].now === "string", `retyped: ${JSON.stringify(s.retyped)}`);
expect(s.defaults.some((r) => r.path === "shops.defaultBuyPrice" && r.was === 100 && r.now === 120), "default change: shops.defaultBuyPrice");
expect(s.defaults.some((r) => r.path === "schemaVersion"), "default change: schemaVersion");
expect(s.ranges.some((r) => r.path === "schemaVersion" && r.now.includes("max 2")), "range change: schemaVersion");
expect(!s.added.some((r) => r.path.startsWith("weather")), "a field inside a new section is not listed twice");
expect(diffSchemas(fx("from.schema.json"), fx("from.schema.json")).defaults.length === 0, "identical schemas differ");

const was = {
  schema: fx("from.schema.json"),
  migrations: { migrations: [] },
  shapes: { saveVersion: 32, payloads: { ledger: "LF_LedgerEntry", bills: "LF_TreatmentBillRecord" } },
  manifests: { LF_Everon1989: { min: "0.1.0", below: "0.2.0" } },
};
const now = {
  schema: fx("to.schema.json"),
  migrations: { migrations: [{ from: 1, name: "shop-to-shops", description: "Renames shop.", changes: [{ op: "rename", path: "shop", to: "shops" }, { op: "default", path: "shops.newFlag", value: true }] }] },
  shapes: { saveVersion: 33, payloads: { ledger: "LF_LedgerEntry2", bills: "LF_TreatmentBillRecord", farms: "LF_FarmRow" } },
  manifests: { LF_Everon1989: { min: "0.2.0", below: "0.3.0" }, LF_New: { min: "0.2.0", below: "0.3.0" } },
};
const d = diffAll(was, now);
expect(d.migrations.length === 1 && d.migrations[0].name === "shop-to-shops", "migration added");
expect(d.shapes.from === 32 && d.shapes.to === 33 && d.shapes.payloadsAdded.join() === "farms" && d.shapes.payloadsChanged[0]?.key === "ledger", `shapes: ${JSON.stringify(d.shapes)}`);
expect(d.manifests.length === 2 && d.manifests[0].text.includes("[0.1.0, 0.2.0) -> [0.2.0, 0.3.0)"), `manifests: ${JSON.stringify(d.manifests)}`);

const template = readFileSync(join(here, "..", "..", "docs", "upgrades", "TEMPLATE.md"), "utf8");
const page = renderPage(template, { fromRef: "v0.1.0", toRef: "v0.2.0", version: "0.2.0", d });
for (const want of ["# Upgrade to 0.2.0", "Upgrading from `v0.1.0`", "`shops.defaultBuyPrice` | default 100 | default 120", "`shops.taxRate` | type number | type string",
  "Added section: `weather`", "Removed section: `legacy`", "Added: `shops.newFlag`", "Removed: `shops.oldFlag`", "`32` -> `33`", "new payload `farms`",
  "`shop-to-shops`", "rename `shop` -> `shops`", "`LF_New` new pack", "## Manual steps", "## Rollback"]) {
  expect(page.includes(want), `page lacks: ${want}`);
}
expect(!page.includes("<previous x.y.z>") && !page.includes("| `<section.key>`"), "template placeholders remain");

const quiet = renderPage(template, { fromRef: "a", toRef: "b", d: diffAll(was, { ...was }) });
expect(quiet.includes("No config changes.") && quiet.includes("No save version changes (save version 32).") && /## Migrations\n\nNone\./.test(quiet), "an unchanged release does not say so");

const old = renderPage(template, { fromRef: "a", toRef: "b", d: diffAll({ ...was, shapes: null }, now) });
expect(old.includes("has no record-shapes lock") && !old.includes("`null`"), "a ref without the shapes lock prints null");

const m = parseManifest('LF_PackManifest {\n m_sPackId "LF_X"\n m_sCoreMin "0.1.0"\n m_sCoreBelow "0.2.0"\n}');
expect(m?.id === "LF_X" && m.min === "0.1.0" && m.below === "0.2.0", "parseManifest");

if (fails.length) { for (const f of fails) console.log(`FAIL ${f}`); process.exit(1); }
console.log("upgrade-page-selftest: OK");
