import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The ATM's in-world lines on a running `up --mission grid --profile full` session: no window opens. Client 1 stands
// in front of the grid ATM (Grid_slice_3352_R0), looks down at the CASH slot, scrolls the list and presses the use key;
// the bank falls and the cash rises by the same listed amount. It looks lower at the DEPOSIT drawer and deposits;
// the cash falls and the bank rises by the same amount. The live figures on the machine's screen and the action list are
// captured before and after for the PR.
//   node agent-box/session.mjs atm-quick [side] [cashPitch] [depositPitch]
async function runAtmQuickProof(args) {
  const side = args[0] || "w";
  const cashPitch = args[1] || "-34"; // third person, from the stand: reaches the CASH slot (calibrated 2026-11-22)
  const depositPitch = args[2] || "-4"; // further down to the DEPOSIT drawer: not yet calibrated
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const money = () => {
    const s = parseState(on({ verb: "state" }, 10000).detail);
    return { bank: Number(s.bank), cash: Number(s.cash) };
  };
  const shot = (name) => on({ verb: "capture", mode: "ui", name }, 25000);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  // The welcome guide opens over a new character; close it like a player would.
  for (let i = 0; i < 10 && !on({ verb: "ui" }).detail.includes("WelcomeGuideCloseButton"); i++) wait(1500);
  on({ verb: "click", widget: "WelcomeGuideCloseButton" });
  wait(1000);
  // Stand until the server has really moved the character beside the machine.
  for (let i = 0; i < 6; i++) {
    on({ verb: "stand", entityName: "Grid_slice_3352_R0", side });
    wait(2500);
    const pos = (parseState(on({ verb: "state" }, 10000).detail).pos || "").split(",").map(Number);
    if (Math.abs(pos[0] - 258.5) < 3 && Math.abs(pos[2] - 52) < 3) break;
  }
  on({ verb: "input", action: "CharacterTurnUp", value: cashPitch });
  wait(1500);
  on({ verb: "look" });
  shot("atm_quick_cash_list");

  const before = money();
  // The cash slot lists $20, $50, $100, $500, all: two steps down is $100.
  on({ verb: "interact", step: "prev" });
  on({ verb: "interact", step: "prev" });
  wait(500);
  shot("atm_quick_cash_100");
  on({ verb: "interact", step: "confirm" });
  wait(2500);
  shot("atm_quick_after_withdraw");
  const afterWithdraw = money();
  console.log(`withdraw: bank ${before.bank} -> ${afterWithdraw.bank}, cash ${before.cash} -> ${afterWithdraw.cash}`);
  const took = before.bank - afterWithdraw.bank;
  if (![20, 50, 100, 500].includes(took)) fails.push(`the bank fell by ${took}, not a listed amount`);
  if (afterWithdraw.cash - before.cash !== took) fails.push(`the cash rose by ${afterWithdraw.cash - before.cash}, not the ${took} the bank paid`);

  on({ verb: "input", action: "CharacterTurnUp", value: depositPitch });
  wait(1500);
  shot("atm_quick_deposit_list");
  // The deposit drawer lists all cash, $50, $100, $500: one step down is $50.
  on({ verb: "interact", step: "prev" });
  wait(500);
  on({ verb: "interact", step: "confirm" });
  wait(2500);
  shot("atm_quick_after_deposit");
  const afterDeposit = money();
  console.log(`deposit: bank ${afterWithdraw.bank} -> ${afterDeposit.bank}, cash ${afterWithdraw.cash} -> ${afterDeposit.cash}`);
  const put = afterDeposit.bank - afterWithdraw.bank;
  if (put <= 0) fails.push(`the bank rose by ${put}, nothing was deposited`);
  if (afterWithdraw.cash - afterDeposit.cash !== put) fails.push(`the cash fell by ${afterWithdraw.cash - afterDeposit.cash}, not the ${put} the bank took`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "atm quick proof: FAIL" : "atm quick proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "atm-quick",
  usage: "atm-quick [side] [cashPitch] [depositPitch]",
  run: (ctx) => runAtmQuickProof(ctx?.args ?? []),
};
