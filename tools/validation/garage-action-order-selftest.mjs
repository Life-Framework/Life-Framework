// tools/validation/garage-action-order-selftest.mjs: the garage counter lists "Take out your car" first.
// The vanilla action list sorts by each action's "Sort Priority", lowest first (the lock row is 5, "Get in" is 1), and
// rows with no priority come in no promised order, so a player stood at the counter read the ledger instead of taking
// the car out (first-2-hours review A15). The kit source and the fitted Saint-Philippe layer give the garage counter's
// four actions the order Take out, Store, Read the ledger, Buy a slot.
// inputs: addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street_Vehicles.layer, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_carlot.layer
// red-proof: with the Sort Priority lines removed from the layer, the "Take out is first" case prints FAIL (2026-11-22).
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FILES = [
  ["kit source", "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street_Vehicles.layer", /^ Roadmap_Garage \{/],
  ["island layer", "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_carlot.layer", /^GenericEntity Fit_saint_philippe_carlot_garage/],
];
const ORDER = ["LF_VehicleRetrieveAction", "LF_VehicleStoreAction", "LF_GarageLedgerAction", "LF_GarageBuySlotAction"];
let failed = 0;
const check = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

for (const [label, rel, startRx] of FILES) {
  const lines = readFileSync(join(root, rel), "utf8").split("\n");
  const start = lines.findIndex((l) => startRx.test(l));
  check(start >= 0, `${label}: the garage counter is in the file`);
  if (start < 0) continue;
  const prio = {};
  for (const cls of ORDER) {
    const at = lines.findIndex((l, i) => i > start && new RegExp(`^\\s*${cls} "`).test(l));
    check(at >= 0, `${label}: ${cls} is on the counter`);
    if (at < 0) continue;
    const indent = lines[at].match(/^\s*/)[0];
    let end = at + 1;
    while (lines[end] !== `${indent}}`) end++;
    const m = lines.slice(at, end).join("\n").match(/"Sort Priority" (\d+)/);
    prio[cls] = m ? Number(m[1]) : null;
  }
  check(ORDER.every((c) => prio[c] !== null && prio[c] !== undefined), `${label}: every garage action has a Sort Priority`);
  check(ORDER.every((c, i) => i === 0 || prio[c] > prio[ORDER[i - 1]]), `${label}: Take out is first, then Store, Read the ledger, Buy a slot (Take out is first)`);
}

if (failed) {
  console.log(`SUMMARY garage-action-order-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY garage-action-order-selftest ok");
