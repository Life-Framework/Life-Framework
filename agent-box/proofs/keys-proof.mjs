import { arrive, clientBox, sessionDir } from "../session.mjs";

// #613 key models on a running session: arrive, take a house key and a vehicle key (one of each
// also set on the ground ahead), then shoot the ground view and the open inventory.
async function runKeysProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive(on, 1, wait);
  wait(6000);
  const give = on(1, { verb: "keys" }, 10000);
  wait(2000);
  on(1, { verb: "capture", mode: "ui", name: "keys_ground" }, 25000);
  on(1, { verb: "input", action: "Inventory" }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "keys_inventory" }, 25000);
  on(1, { verb: "input", action: "Inventory" }, 10000);
  const ok = give.status === "ok" && give.detail.includes("house=1") && give.detail.includes("vehicle=1");
  console.log(ok ? "keys proof: PASS" : "keys proof: FAIL");
  return ok ? 0 : 1;
}

export default { name: "keys-proof", usage: "keys-proof", run: () => runKeysProof() };
