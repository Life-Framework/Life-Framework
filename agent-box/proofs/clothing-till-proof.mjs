import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths, sessionUid } from "../session.mjs";

// The Saint-Philippe clothing store's till, used the way a player uses it, on a running island session
// (`tools\cli session up --mission island --wait`, one client). Reported 2026-11-19: "I can't interact with the till".
// The pay action sat on a mesh-less stock entity at the building's origin, a metre and a bit from the register; `look`
// listed it (it lists every action within 4 m) but the interact key never offered it. The proof reads what the key
// WOULD act on (`aim`, the engine's interaction handler: its current context, the offered actions and the selected one):
//   1. stand beside the shelf, take one cheap cap off it with the F-key pickup (an unpaid item the store tags)
//   2. stand at the register, `aim`: LF_StoreTillAction on the register itself must be selected or offered
//   3. scroll to it if it is only offered, confirm with the interact key, and read the server log: `paid 20 for 1 item(s)`
//      and the cash that left the wallet
const STORE = "everon_clothingstore";
const SHELF = "Fit_saint_philippe_clothing_shelf_shop_01_2";
const TILL = "Fit_saint_philippe_clothing_cashier_shop_01";
const ACTION = "LF_StoreTillAction";

// Pure: does an `aim` reply say the till's pay action is the selected one, or only one of the offered ones?
export function aimVerdict(detail) {
  const text = String(detail ?? "");
  const selected = new RegExp(`selected=${ACTION}@${TILL}\\b`).test(text);
  const offered = new RegExp(`offered=[^ ]*${ACTION}@${TILL}:1`).test(text);
  return { selected, offered, none: /^aim none/.test(text) };
}

async function runClothingTillProof() {
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
  const fails = [];
  arrive(on, 1, wait);
  wait(6000);
  const uid = sessionUid(parseState(on(1, { verb: "state" }, 10000).detail).player);
  const cashBefore = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash || 0);

  on(1, { verb: "stand", entityName: SHELF });
  wait(2500);
  const took = on(1, { verb: "pickup", item: "Hat_FlatCap" }, 15000);
  if (took.status !== "ok" || !/pickup took/.test(took.detail)) fails.push(`no cap taken off the shelf: ${took.status} ${took.detail}`);
  wait(1500);

  on(1, { verb: "stand", entityName: TILL });
  wait(2500);
  let aim = on(1, { verb: "aim", words: [] }, 10000);
  let verdict = aimVerdict(aim.detail);
  // Only offered: scroll the highlight (at most four steps) until the till is the selected action.
  for (let i = 0; i < 4 && !verdict.selected && verdict.offered; i++) {
    on(1, { verb: "interact", step: "next" }, 8000);
    wait(600);
    aim = on(1, { verb: "aim", words: [] }, 10000);
    verdict = aimVerdict(aim.detail);
  }
  if (!verdict.selected) fails.push(`the interact key does not offer ${ACTION}@${TILL} at the register: ${aim.detail}`);
  wait(1500);

  // The F key, as often as a player would press it: the handler acts on the action it settled on a frame earlier,
  // so a press in the same breath as the aim can fall on nothing.
  const paidRe = new RegExp(`store ${STORE}: ${uid} paid \\d+ for \\d+ item\\(s\\)`, "g");
  const paidBefore = (serverLog().match(paidRe) || []).length;
  let paidLines = 0;
  for (let i = 0; i < 3 && paidLines < 1; i++) {
    on(1, { verb: "interact", step: "confirm" }, 8000);
    wait(2500);
    paidLines = (serverLog().match(paidRe) || []).length - paidBefore;
  }
  const cashAfter = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash || 0);
  if (paidLines < 1) fails.push(`no '${uid} paid N for 1 item(s)' line for the clothing store in the server log`);
  if (!(cashAfter < cashBefore)) fails.push(`the wallet did not shrink (${cashBefore} -> ${cashAfter})`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "clothing till proof: FAIL" : `clothing till proof: PASS (the interact key offered the till, a cap was paid for, cash ${cashBefore} -> ${cashAfter})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "clothing-till-proof",
  usage: "clothing-till-proof",
  run: () => runClothingTillProof(),
};
