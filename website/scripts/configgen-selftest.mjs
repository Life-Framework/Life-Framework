// Selftest for src/lib/configgen.ts against the real docs/config/config.schema.json.
// Run: node scripts/configgen-selftest.mjs (Node 22.6+ strips the types).
import fs from "node:fs";
import assert from "node:assert/strict";
import { defaultOf, minimalDiff, validate } from "../src/lib/configgen.ts";

const schema = JSON.parse(fs.readFileSync(new URL("../../docs/config/config.schema.json", import.meta.url), "utf8"));
const base = () => defaultOf(schema);

assert.deepEqual(minimalDiff(schema, base()), { schemaVersion: 1 }, "defaults produce only schemaVersion");
assert.deepEqual(validate(schema, base()), {}, "defaults validate");

const v = base();
v.shops.defaultBuyPrice = 70;
v.shops.priceRules["Prefabs/Items/Food/"] = { prefab: "", buyPrice: 10, sellPrice: 50, hidden: false };
v.shops.priceRules["Prefabs/Weapons/"] = { prefab: "", buyPrice: 100, sellPrice: 50, hidden: true };
v.shops.priceRules["Prefabs/Other/"] = { prefab: "", buyPrice: 100, sellPrice: 50, hidden: false };
v.shops.extraItemPrefabs = ["Prefabs/X.et"];
assert.deepEqual(minimalDiff(schema, v), {
  schemaVersion: 1,
  shops: {
    defaultBuyPrice: 70,
    priceRules: { "Prefabs/Items/Food/": { buyPrice: 10 }, "Prefabs/Weapons/": { hidden: true }, "Prefabs/Other/": {} },
    extraItemPrefabs: ["Prefabs/X.et"],
  },
});

v.shops.defaultBuyPrice = -1;
v.shops.priceRules["Prefabs/Items/Food/"].sellPrice = 1.5;
v.vehicles.buyPercent = 2000000;
const errs = validate(schema, v);
assert.ok(errs[".shops.defaultBuyPrice"]?.includes("at least 0"));
assert.ok(errs[".shops.priceRules.Prefabs/Items/Food/.sellPrice"]?.includes("whole"));
assert.equal(errs[".vehicles.buyPercent"], undefined, "no max declared, so no error");
console.log("configgen-selftest: OK");
