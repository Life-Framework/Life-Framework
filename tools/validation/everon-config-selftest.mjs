// Selftest for validate-everon-config.mjs (#1760): the stubs pass, a fixture that expects an offer or a rank
// the overlays lack fails, and the merge applies add, patch, remove and m_bRemoved the way the game layers.
// inputs: tools/**, addons/LF_Everon1989/**, addons/LifeFramework/Configs/**
import { join } from "node:path";
import { checkEveron, checkFixture, mergeLayers, parseConf, ROOT } from "./validate-everon-config.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const live = checkEveron(ROOT, join(ROOT, "tools/validation/fixtures/everon"));
expect(live.fails.length === 0, `the repo and its live fixtures must pass: ${live.fails.join("; ")}`);
const red = checkEveron(ROOT, join(ROOT, "tools/validation/fixtures/everon/red"));
expect(red.fails.some((f) => f.includes("gun_counter does not offer pistol_m1911")), "the red fixture must fail on the missing offer");

const cat = (body) => parseConf(`LF_ItemCatalogueConfig {\n m_aItems {\n }\n m_aOffers {\n${body}\n }\n}\n`);
const offer = (outlet, item, extra = "") => `  LF_OutletOffer "{AAAAAAAAAAAAAAA1}" {\n   m_sOutlet "${outlet}"\n   m_sItem "${item}"\n${extra}  }`;
const base = { file: "core.conf", root: cat(offer("shop", "a", '   m_sLicence "L"\n')) };

let merged = mergeLayers([base, { file: "o.conf", root: cat(offer("shop", "b")) }]);
expect(merged.offers.length === 2, "an overlay adds an offer");
merged = mergeLayers([base, { file: "o.conf", root: cat(offer("shop", "a", '   m_iMinRank 2\n')) }]);
expect(merged.offers.length === 1 && merged.offers[0].m_sLicence === "L" && merged.offers[0].m_iMinRank === "2", "an overlay patches only what it writes");
merged = mergeLayers([base, { file: "o.conf", root: cat(offer("shop", "a", '   m_sOp "remove"\n')) }]);
expect(merged.offers.length === 0, "the remove verb takes the offer out");
merged = mergeLayers([base, { file: "o.conf", root: cat(offer("shop", "a", '   m_bRemoved 1\n')) }]);
expect(merged.offers.length === 0, "m_bRemoved hides the offer");

merged = mergeLayers([base]);
expect(checkFixture("f", { kind: "outlets", outlet: "shop", offers: [{ item: "a", licence: "L" }] }, merged).length === 0, "a matching offer and gate pass");
expect(checkFixture("f", { kind: "outlets", outlet: "shop", offers: [{ item: "a", licence: "X" }] }, merged).length === 1, "a wrong gate fails");
expect(checkFixture("f", { kind: "outlets", outlet: "shop", offers: [{ item: "a", present: false }] }, merged).length === 1, "an offer that must be absent fails when present");
expect(checkFixture("f", { kind: "outlets", outlet: "shop", offers: [{ item: "zz" }] }, merged).length === 1, "a missing offer fails");
expect(checkFixture("f", { kind: "ranks", count: 3 }, merged).length === 1, "a wrong rank count fails");
expect(checkFixture("f", { kind: "bogus" }, merged).length === 1, "an unknown kind fails");

if (fails.length) {
  for (const f of fails) console.error(`FAIL ${f}`);
  process.exit(1);
}
console.log("everon-config-selftest: ok");
