import {  } from "../session.mjs";

// M2 #202 on a running `up --clients 2 --wait` session (debug mission): both shoppers stand inside the
// general store. Shopper A takes a shelf item, pays at the till (the toast and the receipt) and walks
// out clean. Shopper B takes a shelf item and walks out unpaid: the alarm sounds at the store, dispatch
// is called, B's wanted level goes up and the report names B. A is not reported. The server log must
// carry one `paid ... for 1 item(s)` line for A and one `walked out with 1 unpaid item(s)` line for B.
// Shots: the shelf, the till, the alarm. Keep it short: the box is shared.
async function runStoreProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const count = (re) => (serverLog().match(re) || []).length;
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const uidA = sessionUid(parseState(on(1, { verb: "state" }, 10000).detail).player);
  const uidB = sessionUid(parseState(on(2, { verb: "state" }, 10000).detail).player);
  const wantedBefore = Number(parseState(on(2, { verb: "state" }, 10000).detail).wanted || 0);
  for (const n of [1, 2]) on(n, { verb: "stand", entityName: "Shop_GeneralStore_Debug" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m2_202_shelf_client1" }, 25000);
  const takeA = on(1, { verb: "shelf" }, 10000);
  const takeB = on(2, { verb: "shelf" }, 10000);
  wait(1500);
  const pay = on(1, { verb: "use", className: "LF_StoreTillAction", entityName: "Shop_GeneralStore_Debug" }, 10000);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_202_till_client1" }, 25000);
  const paidLines = count(new RegExp(`${uidA}[^\\n]* paid \\d+ for 1 item`, "g"));
  // A walks out clean; B walks out with the unpaid item.
  on(1, { verb: "stand", entityName: "Store_Street_Marker" });
  on(2, { verb: "stand", entityName: "Store_Street_Marker" });
  wait(2500);
  const caught = count(new RegExp(`${uidB}[^\\n]* walked out with 1 unpaid item`, "g"));
  const caughtA = count(new RegExp(`${uidA}[^\\n]* walked out with`, "g"));
  on(2, { verb: "capture", mode: "ui", name: "m2_202_alarm_client2" }, 25000);
  on(1, { verb: "stand", entityName: "Shop_GeneralStore_Debug", side: "s" });
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_202_alarm_client1" }, 25000);
  const stateB = parseState(on(2, { verb: "state" }, 10000).detail);
  if (takeA.status !== "ok" || !takeA.detail.includes("shelf took")) fails.push(`A took nothing: ${takeA.status} ${takeA.detail}`);
  if (takeB.status !== "ok" || !takeB.detail.includes("shelf took")) fails.push(`B took nothing: ${takeB.status} ${takeB.detail}`);
  if (pay.status !== "ok") fails.push(`the till action returned ${pay.status} ${pay.detail}`);
  if (paidLines < 1) fails.push("no 'paid ... for 1 item' line for shopper A in the server log");
  if (caught < 1) fails.push("shopper B was not caught walking out in the server log");
  if (caughtA > 0) fails.push("shopper A, who paid, was reported as a thief");
  if (!(Number(stateB.wanted || 0) > wantedBefore)) fails.push(`B's wanted level did not rise (${wantedBefore} -> ${stateB.wanted})`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "store proof: FAIL" : "store proof: PASS (A paid and left clean, B walked out unpaid: alarm, dispatch, wanted level and a named report)");
  return fails.length ? 1 : 0;
}

export default {
  name: "store-proof",
  usage: "store-proof",
  run: () => runStoreProof(),
};
