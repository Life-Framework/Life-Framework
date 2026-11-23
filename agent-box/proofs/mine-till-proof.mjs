import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The Coalman's Brow miner's supply till after "Pay $150 cash" (first-2h re-review N2: the sheet kept "Cash in hand:
// $205" and the full order slip, so the purchase looked like it had not happened). On a running island session
// (`tools\cli session up --mission island --profile full --wait`, one client): reset to $205 cash, take the miner job at
// the mine desk, open the till, order the pickaxe, pay, and capture the sheet. The Cash line must read $55 and the slip
// must be empty; a second capture after the reply hold shows the line still at $55.
const DESK = "Fit_coalmans_brow_mine_mine_desk";
const TILL = "Fit_coalmans_brow_mine_mine_supply_till";

async function runMineTillProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "resetplayer", cash: 205 }, 15000);
  on(1, { verb: "guide", words: ["close"] });
  // The desk toggles the job: take it only when the character does not hold it yet.
  if (parseState(on(1, { verb: "state" }, 10000).detail).job !== "MINER") {
    on(1, { verb: "stand", entityName: DESK }, 10000);
    on(1, { verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK }, 10000);
    wait(1500);
  }
  on(1, { verb: "stand", entityName: TILL }, 10000);
  wait(1500);
  const open = on(1, { verb: "use", className: "LF_ShopAction", entityName: TILL }, 10000);
  if (open.status !== "ok") fails.push(`the till did not open: ${open.status} ${open.detail}`);
  wait(1500);
  on(1, { verb: "click", widget: "Item0Row_Action" }, 10000);
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "n2_till_ordered" }, 25000);
  on(1, { verb: "click", widget: "PayButton" }, 10000);
  wait(2500);
  const ui = on(1, { verb: "ui", mode: "text" }, 10000);
  on(1, { verb: "capture", mode: "ui", name: "n2_till_paid" }, 25000);
  const text = String(ui.detail ?? "");
  if (!/Cash in hand: \$55\b/.test(text)) fails.push(`the Cash line does not read $55 after paying: ${text.slice(0, 300)}`);
  if (/1 x Pickaxe/.test(text)) fails.push("the order slip still holds the pickaxe after paying");
  wait(1500);
  const later = on(1, { verb: "ui", mode: "text" }, 10000);
  if (!/Cash in hand: \$55\b/.test(String(later.detail ?? ""))) fails.push("the Cash line moved off $55 after the reply hold");
  const cash = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash || 0);
  if (cash !== 55) fails.push(`the wallet holds $${cash}, not $55`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "mine till proof: FAIL" : "mine till proof: PASS (paid $150, the sheet reads $55 and the slip is empty)");
  return fails.length ? 1 : 0;
}

export default {
  name: "mine-till-proof",
  usage: "mine-till-proof",
  run: () => runMineTillProof(),
};
