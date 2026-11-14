import { arrive, clientBox, sessionDir } from "../session.mjs";

// Back room clarity (#933) on a running session (one client is enough):
//   node agent-box/session.mjs up                    (lowspec; add --profile full for pictures)
//   node agent-box/session.mjs back-room-door-proof [--shot]
//   node agent-box/session.mjs down
// 1. At the general store counter the price list opens; the shopkeeper's word about the back room shows in the shot.
// 2. At the back room stall the F key offers the shop and the price list opens (the handgun, in cash).
// 3. With --shot, the pictures of both sheets and of the prompt. The shut state needs an officer and
//    a raid: `session.mjs backroom-police-proof` does that, and the prompt then reads "Back room (shut)".
const STORE = "Shop_GeneralStore_Debug";
const BACK_ROOM = "Shop_BlackMarket_Debug";
const ACTION = "LF_ShopAction";

async function runBackRoomDoorProof(args = []) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const shot = (name) => {
    if (!args.includes("--shot")) return;
    h.setBoxes([clientBox(dir, 1)]);
    const s = h.sendCommand({ verb: "capture", mode: "ui", name }, 25000);
    wait(3000);
    console.log(`client 1: shot ${name} -> ${h.resolveCapture(s.detail) || "not written"}`);
  };

  arrive(on, 1, wait);

  // The shopkeeper's hint on the general store's sheet.
  must(on(1, { verb: "stand", entityName: STORE }), "stand at the general store");
  wait(2500);
  must(on(1, { verb: "use", className: ACTION, entityName: STORE }, 10000), "open the general store sheet");
  wait(2000);
  const storeUi = must(on(1, { verb: "ui" }, 10000), "read the general store sheet");
  // `ui` lists widget names, not text: the keeper's line is judged from the shot.
  if (!storeUi.detail.includes("Item0Row_Action")) fails.push(`the general store sheet shows no lines: ${storeUi.detail.slice(0, 200)}`);
  shot("backroom_store_hint");
  on(1, { verb: "click", widget: "CloseButton" });
  wait(1000);

  // The back room itself.
  must(on(1, { verb: "stand", entityName: BACK_ROOM }), "stand at the back room");
  wait(2500);
  const look = must(on(1, { verb: "look" }), "look at the back room");
  if (!look.detail.includes(ACTION)) fails.push(`the F key does not offer ${ACTION} at ${BACK_ROOM}: ${look.detail.slice(0, 200)}`);
  shot("backroom_prompt");
  must(on(1, { verb: "use", className: ACTION, entityName: BACK_ROOM }, 10000), "knock on the back room");
  wait(2000);
  const roomUi = must(on(1, { verb: "ui" }, 10000), "read the back room sheet");
  if (!roomUi.detail.includes("Item0Row_Action") || roomUi.detail.includes("Item1Row_Action")) fails.push(`the back room sheet is not the one-line handgun list: ${roomUi.detail.slice(0, 200)}`);
  shot("backroom_sheet");
  on(1, { verb: "click", widget: "CloseButton" });

  if (fails.length) {
    console.log("back-room-door-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("back-room-door-proof: OK");
  return 0;
}

export default { name: "back-room-door-proof", usage: "back-room-door-proof [--shot]", run: (ctx) => runBackRoomDoorProof(ctx.args) };
