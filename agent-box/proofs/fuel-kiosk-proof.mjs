import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The Saint-Philippe fuel station kiosk sells (the `fuel_kiosk` stock, LF_ShopStock_FuelKiosk.conf), driven the way a
// player does it on the island:
//   node agent-box/session.mjs up --mission island --wait
//   node agent-box/session.mjs fuel-kiosk-proof
//   node agent-box/session.mjs down
// Stands at the counter on the customer side of the kiosk till, checks the F key offers the shop action, opens the price
// list, buys the paper map of Everon (the first line, $25) and reads `[LFDebug:Shop] bought item=...LF_TouristMap` off the
// server log. Exits non-zero when a step is missing.
const SHOP_ENTITY = "Fit_saint_philippe_fuel_kiosk_shop";
const SHOP = "LF_ShopAction";
const START_CASH = 1000;
const MAP_PRICE = 25;

async function runFuelKioskProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(3000);
  on(1, { verb: "resetplayer", cash: START_CASH }, 10000);
  wait(1500);

  on(1, { verb: "stand", entityName: SHOP_ENTITY, side: "w1" });
  wait(2500);
  const look = on(1, { verb: "look" });
  expect(look.detail.includes(SHOP), `the F key offers the shop action at the kiosk counter: ${look.detail.slice(0, 200)}`);

  expect(on(1, { verb: "use", className: SHOP, entityName: SHOP_ENTITY }, 10000).status === "ok", "the kiosk shop action opens");
  wait(2000);
  const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/Item0Row_Action|map/i.test(list), `the price list opened (got: ${list.slice(0, 200)})`);

  on(1, { verb: "click", widget: "Item0Row_Action" });
  wait(600);
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  const bought = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(new RegExp(String(START_CASH - MAP_PRICE)).test(bought), `$${MAP_PRICE} taken from $${START_CASH} (got: ${bought.slice(0, 240)})`);
  expect(/bought item=\S*LF_TouristMap/.test(serverLog()), "the server log shows [LFDebug:Shop] bought item=...LF_TouristMap");
  on(1, { verb: "click", widget: "CloseButton" });

  if (fails.length) {
    console.log("fuel-kiosk-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("fuel-kiosk-proof: OK");
  return 0;
}

export default { name: "fuel-kiosk-proof", usage: "fuel-kiosk-proof", run: () => runFuelKioskProof() };
