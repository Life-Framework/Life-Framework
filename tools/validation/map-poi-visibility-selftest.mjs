// Red-proof for validate-map-poi-visibility: each way to publish an underworld POI is caught, the
// hidden row and a public civic row are not, and the real core and pack confs pass.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { poiConfFiles, poiRows, underworldViolations } from "./validate-map-poi-visibility.mjs";

const row = (kind, category, visibility) =>
  `  LF_MapPoiKind "{0000000000000001}" {\n   m_sKind "${kind}"\n` +
  (category ? `   m_sCategory "${category}"\n` : "") +
  `   m_sLabel "#LF-MapPoi_${kind}"\n` +
  (visibility ? `   m_eVisibility ${visibility}\n` : "") +
  `  }\n`;
const conf = (...rows) => `LF_MapPoiConfig {\n m_aKinds {\n${rows.join("")} }\n}\n`;
const check = (name, files, expected) => {
  const got = underworldViolations(files.map((text, i) => ({ name: `f${i}`, text }))).map((b) => `${b.file}:${b.kind}`).join(",");
  if (got !== expected) {
    console.log(`FAIL map-poi-visibility-selftest: ${name}: expected "${expected}" got "${got}"`);
    process.exit(1);
  }
};

check("hidden underworld and public civic pass", [conf(row("Fence", "underworld", "HIDDEN"), row("Bank", "civic", "PUBLIC"), row("Shop", "shop", ""))], "");
check("underworld PUBLIC fails", [conf(row("Fence", "underworld", "PUBLIC"), row("Bank", "civic", "PUBLIC"))], "f0:Fence");
check("underworld DISCOVER fails", [conf(row("Dealer", "underworld", "DISCOVER"))], "f0:Dealer");
check("underworld ROLE fails", [conf(row("Dealer", "underworld", "ROLE"))], "f0:Dealer");
check("underworld with no visibility (default PUBLIC) fails", [conf(row("Fence", "underworld", ""))], "f0:Fence");
check("a pack patch row making an underworld kind PUBLIC fails", [conf(row("Fence", "underworld", "HIDDEN")), conf(row("Fence", "", "PUBLIC"))], "f1:Fence");
check("a pack patch row of a civic kind is not underworld", [conf(row("Bank", "civic", "PUBLIC")), conf(row("Bank", "", "DISCOVER"))], "");
check("two bad rows are both reported", [conf(row("A", "underworld", "PUBLIC"), row("B", "underworld", ""))], "f0:A,f0:B");
if (poiRows(conf(row("A", "underworld", "HIDDEN"), row("B", "civic", ""))).length !== 2) {
  console.log("FAIL map-poi-visibility-selftest: expected 2 parsed rows");
  process.exit(1);
}

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const real = poiConfFiles(root);
if (real.length < 2 || !real.some((f) => /LF_MapPois\.conf$/.test(f.name)) || !real.some((f) => f.name.includes("LF_Everon1989"))) {
  console.log(`FAIL map-poi-visibility-selftest: expected the core and the pack POI conf, found ${real.map((f) => f.name).join(",")}`);
  process.exit(1);
}
const rows = real.reduce((n, f) => n + poiRows(f.text).length, 0);
const bad = underworldViolations(real);
if (bad.length || rows < 3) {
  console.log(`FAIL map-poi-visibility-selftest: real confs: ${rows} rows, violations ${JSON.stringify(bad)}`);
  process.exit(1);
}
console.log(`map-poi-visibility-selftest: OK (${rows} real rows)`);
