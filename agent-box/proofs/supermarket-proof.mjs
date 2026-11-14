import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #1143 on a running `up --profile full --mission playtest` session: the pack supermarket
// (Village_Supermarket, building origin 76 0.1 96.45, front on -Z). One shopper:
//   1. pictures of the stocked store, the shopkeeper and the alarm box;
//   2. talks to the shopkeeper (the conversation opens, "how do I pay" is answered);
//   3. takes a torch, crackers and a water (the copy nearest where they stand), pays at a till,
//      walks out: no alarm;
//   4. takes a field dressing and walks out: the shopkeeper's line, the alarm on the shop-front box,
//      dispatch and a wanted level.
// The server log must carry the paid line, the walk-out line, the staff line and the alarm spot.
// Stock across a restart is `session.mjs restart` after this, then the `restored` line.
const B = [76, 0.1, 96.45];
const at = (x, y, z) => [B[0] + x, B[1] + y, B[2] + z].map((v) => +v.toFixed(3));

async function runSupermarketProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const prefix = args[args.indexOf("--name") + 1] && args.includes("--name") ? args[args.indexOf("--name") + 1] : "sm";
  const on = (spec, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const count = (re) => (serverLog().match(re) || []).length;
  // The character stands at target + offset, facing the target; a picture is taken.
  const stand = (name, target, offset) => on({ verb: "capture", mode: "world", name: `${prefix}_${name}`, x: target[0], y: target[1], z: target[2], ox: offset[0], oy: offset[1], oz: offset[2] }, 30000);
  const fails = [];
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(4000);
  // The first frames after a teleport draw the building before it streams in: one throwaway shot.
  stand("warmup", at(0, 3.1, -7), [-8, -1.5, -9]);
  wait(6000);

  // 1. Pictures.
  stand("street", at(0, 3.1, -7), [-8, -1.5, -9]);
  stand("alarm_box", at(-2.4, 2.3, -7), [1.5, -2.2, -5]);
  stand("hall", at(-1.5, 1.0, 2.5), [1.5, -0.9, -9]);
  stand("fruit", at(-5.2, 0.9, -1.5), [1.8, -0.8, -2.2]);
  stand("dry_goods", at(0.8, 0.9, -1.5), [1.6, -0.8, -2.2]);
  stand("cooler", at(-8.0, 0.75, -3.6), [1.6, -0.65, 0.6]);

  // 2. The shopkeeper.
  stand("shopkeeper", at(7.83, 1.4, 0.35), [-1.0, -1.3, 0]);
  const talk = on({ verb: "use", className: "LF_DialogueTalkAction", entityName: "Village_Supermarket_Shopkeeper" });
  wait(2500);
  const open = on({ verb: "dialogue", arg: "", value: "" });
  on({ verb: "capture", mode: "ui", name: `${prefix}_talk` }, 25000);
  on({ verb: "dialogue", arg: "pick", value: "0" });
  wait(3000);
  const answered = on({ verb: "dialogue", arg: "", value: "" });
  on({ verb: "capture", mode: "ui", name: `${prefix}_talk_pay` }, 25000);
  on({ verb: "dialogue", arg: "skip", value: "" });
  wait(1500);
  on({ verb: "dialogue", arg: "pick", value: "2" });
  wait(3000);
  on({ verb: "dialogue", arg: "skip", value: "" });
  wait(1500);
  const closed = on({ verb: "dialogue", arg: "", value: "" });

  // 3. Buy and walk out.
  const cash0 = Number(parseState(on({ verb: "state" }).detail).cash || 0);
  stand("take_torch", at(3.29, 0.9, -0.3), [-0.8, -0.8, 0]);
  const torch = on({ verb: "shelf" });
  stand("take_crackers", at(0.75, 0.9, -2.8), [0.75, -0.8, 0]);
  const crackers = on({ verb: "shelf" });
  stand("take_water", at(-8.04, 0.8, -5.5), [1.3, -0.7, 0]);
  const water = on({ verb: "shelf" });
  stand("till", at(8.174, 1.14, -5.394), [-1.1, -1.04, 0]);
  const pay = on({ verb: "use", className: "LF_StoreTillAction", entityName: "Village_Supermarket" });
  wait(1500);
  const cash1 = Number(parseState(on({ verb: "state" }).detail).cash || 0);
  stand("out_clean", at(0, 1.4, -10.5), [0, -1.3, -2]);
  wait(3000);
  const clean = parseState(on({ verb: "state" }).detail);

  // 4. Walk out with a dressing.
  stand("take_dressing", at(3.29, 0.9, -2.8), [-0.9, -0.8, 0]);
  const dressing = on({ verb: "shelf" });
  stand("out_thief", at(0, 1.4, -10.5), [0, -1.3, -2]);
  wait(3000);
  const caught = parseState(on({ verb: "state" }).detail);
  stand("alarm", at(-2.4, 2.3, -7), [1.5, -2.2, -5]);

  const log = serverLog();
  const taken = [torch, crackers, water, dressing].map((r) => /item=\S*\/([^/ ]+)\.et/.exec(r.detail)?.[1] || r.detail);
  console.log(`taken: ${taken.join(", ")}`);
  if (talk.status !== "ok") fails.push(`talking to the shopkeeper returned ${talk.status} ${talk.detail}`);
  if (!/open node=greet/.test(open.detail)) fails.push(`no shopkeeper conversation on screen: ${open.detail}`);
  if (!/node=pay/.test(answered.detail)) fails.push(`"How do I pay?" was not answered: ${answered.detail}`);
  if (!/^closed/.test(closed.detail)) fails.push(`the conversation did not end on Goodbye: ${closed.detail}`);
  if (taken[0] !== "Flashlight_Soviet_01") fails.push(`the torch gondola gave ${taken[0]}`);
  if (taken[1] !== "LF_Crackers") fails.push(`the crackers gondola gave ${taken[1]}`);
  if (taken[2] !== "WaterBottle") fails.push(`the cooler gave ${taken[2]}`);
  if (taken[3] !== "FieldDressing_US_01") fails.push(`the dressing gondola gave ${taken[3]}`);
  if (pay.status !== "ok" || !(cash0 - cash1 > 0)) fails.push(`the till took nothing (${cash0} -> ${cash1}, ${pay.status} ${pay.detail})`);
  if (!/everon_supermarket: \S+ paid \d+ for 3 item\(s\)/.test(log)) fails.push("no 'paid ... for 3 item(s)' line for the supermarket");
  if (Number(clean.wanted || 0) !== 0 || (clean.alarm && clean.alarm !== "none")) fails.push(`walking out after paying set wanted=${clean.wanted} alarm=${clean.alarm}`);
  if (!(Number(caught.wanted || 0) > 0)) fails.push(`walking out unpaid left wanted=${caught.wanted}`);
  if (!/everon_supermarket: \S+ walked out with 1 unpaid item/.test(log)) fails.push("no walk-out line for the supermarket");
  if (count(/everon_supermarket: 1 member\(s\) of staff called/g) < 1) fails.push("the shopkeeper did not react to the walk-out");
  // The dispatch logs the alarm just before the store logs the walk-out: the last alarm of the log.
  const spot = [...log.matchAll(/alarm sounding at <([^>]+)>/g)].pop()?.[1];
  const box = at(-2.4, 2.3, -7);
  const near = spot && spot.split(/[ ,]+/).map(Number).every((v, i) => Math.abs(v - box[i]) < 1.5);
  if (!near) fails.push(`the alarm did not sound on the shop-front box (spot ${spot}, box ${box.join(" ")})`);
  console.log(`cash ${cash0} -> ${cash1}; after paying wanted=${clean.wanted} alarm=${clean.alarm}; unpaid walk-out wanted=${caught.wanted} alarm=${caught.alarm}; alarm at ${spot}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "supermarket proof: FAIL" : "supermarket proof: PASS (talked to the shopkeeper, paid for a torch, crackers and water and left clean, walked out with a dressing: the shopkeeper's line, the alarm on the shop front, dispatch and a wanted level)");
  return fails.length ? 1 : 0;
}

export default {
  name: "supermarket-proof",
  usage: "supermarket-proof [--name <shot prefix>]   (up --profile full --mission playtest)",
  run: (ctx) => runSupermarketProof(ctx?.args || []),
};
