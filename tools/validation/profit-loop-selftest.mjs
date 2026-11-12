// Proves the no-profit-loop check (#686) fails on a fixture with planted loops, passes on the
// clean twin, catches a loop an owner's config.json `shops` section creates, and honours the
// baseline ratchet. Fixtures: tools/validation/fixtures/profit-loop/{good,bad}.
//
// red-proof: with the shop-loop test in findShopLoops flipped to `pr.sell < pr.buy` the bad
// fixture printed "bad: missing loop shop:Ore" and exited 1; restored, it prints
// "profit-loop-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkProfitLoops, judge, parseConf, guidOf } from "../economy/profit-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (...p) => join(here, "fixtures", "profit-loop", ...p);
const sources = (d, owner = null) => ({
  shop: fx(d, "shop.conf"), chains: fx(d, "chains.conf"), vehicles: fx(d, "vehicles.conf"),
  chop: fx(d, "chop.conf"), layers: [fx("lot.layer")], owner,
});
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

// Parser sanity: nesting, lists and a numeric property.
const tree = parseConf('A "{1}" {\n m_n 5\n m_l {\n  "x"\n  "y"\n }\n m_o B "{2}" {\n  m_v 7\n }\n}\n');
expect(tree.props.m_n === 5 && tree.props.m_l.list.length === 2 && tree.props.m_o.props.m_v === 7, "parser: nesting, list or number wrong");
expect(guidOf("{ab12}Prefabs/X.et") === "AB12", "guidOf: expected upper-case guid");

const bad = checkProfitLoops(sources("bad"));
const ids = bad.map((l) => l.id);
for (const want of ["shop:Ore", "machine:smelter", "vehicle:LF_Car:sellback", "vehicle:LF_Car:chop", "vehicle:LF_Car:chopyard"]) {
  expect(ids.includes(want), `bad: missing loop ${want} (got ${ids.join(", ")})`);
}
const smelter = bad.find((l) => l.id === "machine:smelter");
expect(smelter && smelter.cost === 20 && smelter.revenue === 80 && smelter.margin === 60, `bad: smelter margin wrong ${JSON.stringify(smelter)}`);
const yard = bad.find((l) => l.id === "vehicle:LF_Car:chopyard");
expect(yard && yard.cost === 1000 && yard.revenue === 900 + 2 * 400, `bad: chop yard cash plus parts wrong ${JSON.stringify(yard)}`);

expect(checkProfitLoops(sources("good")).length === 0, `good: expected no loops, got ${checkProfitLoops(sources("good")).map((l) => l.id)}`);

// An owner config.json that reprices a shop item below what it sells for makes a loop the defaults lack.
const owner = checkProfitLoops(sources("good", fx("owner-bad.json")));
expect(owner.some((l) => l.id === "shop:Ore" && l.margin === 7), `owner: expected shop:Ore +7, got ${owner.map((l) => `${l.id}+${l.margin}`)}`);

// Ratchet: a baselined loop passes; a bigger margin fails; a gone loop is stale; a new loop fails.
const base = bad.map((l) => ({ id: l.id, margin: l.margin, reason: "fixture" }));
const same = judge(bad, base);
expect(same.fresh.length === 0 && same.grown.length === 0 && same.stale.length === 0, "ratchet: baselined loops must pass");
expect(judge(bad, base.slice(1)).fresh.length === 1, "ratchet: a loop missing from the baseline must fail");
expect(judge(bad, base.map((b) => ({ ...b, margin: b.margin - 1 }))).grown.length === bad.length, "ratchet: a grown margin must fail");
expect(judge([], base).stale.length === base.length, "ratchet: a fixed loop must show as stale");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("profit-loop-selftest: OK (planted shop, recipe, sell-back and chop loops red, clean twin green, owner overlay and ratchet hold)");
