import { arrive, clientBox, sessionDir } from "../session.mjs";

// The firearm permit at the gun store (maintainer 2026-11-17), driven the way a player does it, under
// the Everon pack so its catalogue overlay puts the permit row on the gun counter:
//   node agent-box/session.mjs up --pack LF_Everon1989 --profile full --wait
//   node agent-box/session.mjs gun-store-permit-proof
//   node agent-box/session.mjs down
// Opens the annex gun counter's price list, picks the "Firearm permit" row and pays the fee. The slip
// shows the application answer and the carried cash drops by the fee. Confirm
// `[LFDebug:Shop] licence row at gun_counter for ...: APPLIED` and `[LFDebug:License] licence desk: ...
// applied for the firearm permit` in the server log.
const COUNTER = "Annex_GunCounter";
const SHOP = "LF_ShopAction";

async function runGunStorePermitProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name }, 25000);

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(4000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);

  on(1, { verb: "stand", entityName: COUNTER });
  wait(2000);
  expect(on(1, { verb: "use", className: SHOP, entityName: COUNTER }, 10000).status === "ok", "the gun counter's price list opens");
  wait(2000);
  const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/Licence0Row_Action|Firearm permit/i.test(list), `the price list shows the firearm permit row (got: ${list.slice(0, 240)})`);

  on(1, { verb: "click", widget: "Licence0Row_Action" });
  wait(800);
  shot("gun_store_permit_row");
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  const answer = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/Application taken|come back to this counter/i.test(answer), `the slip says the application was taken (got: ${answer.slice(0, 240)})`);
  shot("gun_store_permit_applied");
  on(1, { verb: "click", widget: "CloseButton" });

  if (fails.length) {
    console.log("gun-store-permit-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("gun-store-permit-proof: OK");
  return 0;
}

export default { name: "gun-store-permit-proof", usage: "gun-store-permit-proof", run: () => runGunStorePermitProof() };
