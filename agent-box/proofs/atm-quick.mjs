import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The island ATM's in-world lines (#4098), one client on `up --mission island --profile full`: no window opens.
// The client stands at the Saint-Philippe ATM and performs each row of the machine's action list by its index
// (`use LF_ATMQuickAction__<n>`, the same rows the interact key scrolls: withdraw $20, $50, $100, $500, all, then
// deposit all). Pass: the $20 row moves exactly 20 from the bank to cash and the HUD toast reads
// "Withdrew $20. Bank ..." (a notification toast: read it in the picture, `seen` does not report it), then the deposit
// row moves all cash into the bank and the machine screen reads DEPOSITED. Pictures of the machine and the action list are captured before, after the withdraw and
// after the deposit.
//   node agent-box/session.mjs atm-quick [EntityName]
async function runAtmQuickProof(args) {
  const atm = args[0] || "Fit_saint_philippe_atms_drop_b79";
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 260)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const money = () => {
    for (let i = 0; i < 5; i++) {
      const r = on({ verb: "state" }, 10000);
      if (r.status === "ok") { const s = parseState(r.detail); return { bank: Number(s.bank), cash: Number(s.cash) }; }
      wait(1500); // the state read can answer no_session for a moment right after a deposit
    }
    return { bank: NaN, cash: NaN };
  };
  const shot = (name) => on({ verb: "capture", mode: "ui", name }, 25000);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(4000);
  // The welcome guide opens over a new character; dismiss it like a player would.
  for (let i = 0; i < 10 && !String(on({ verb: "ui" }).detail).includes("WelcomeGuideDismissButton"); i++) wait(1500);
  on({ verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1000);
  // The side the machine faces differs per ATM: take the first side whose look offers the rows.
  let faced = false;
  for (const side of ["e", "w", "n", "s"]) {
    on({ verb: "stand", entityName: atm, side });
    wait(2500);
    if (String(on({ verb: "look" }).detail).includes("LF_ATMQuickAction")) { faced = true; break; }
  }
  if (!faced) fails.push("no side of the machine offers the ATM rows");
  shot("atm_island_list");

  const before = money();
  const w = on({ verb: "use", className: "LF_ATMQuickAction" }, 10000);
  wait(1500);
  const afterWithdraw = money();
  shot("atm_island_after_withdraw20");
  console.log(`withdraw $20: bank ${before.bank} -> ${afterWithdraw.bank}, cash ${before.cash} -> ${afterWithdraw.cash}`);
  if (w.status !== "ok") fails.push(`the $20 row was refused: ${w.detail}`);
  if (before.bank - afterWithdraw.bank !== 20) fails.push(`the bank fell by ${before.bank - afterWithdraw.bank}, not 20`);
  if (afterWithdraw.cash - before.cash !== 20) fails.push(`the cash rose by ${afterWithdraw.cash - before.cash}, not 20`);

  // The deposit drawer: the row after "withdraw all" deposits all the cash on you. The last cash leaving froze the
  // client for 25-30 s (#4870): `framegap` reads the longest wall time between two harness polls across the deposit.
  on({ verb: "framegap" }, 10000);
  const d = on({ verb: "use", className: "LF_ATMQuickAction__5" }, 10000);
  wait(1500);
  const afterDeposit = money();
  wait(3000);
  // A client can answer no_session for a moment; ask again, and an answer that never comes is its own failure.
  let gapReply = "";
  for (let i = 0; i < 6 && !/max=\d+/.test(gapReply); i++) {
    const r = on({ verb: "framegap" }, 20000);
    if (r.status === "ok") gapReply = String(r.detail); else wait(2000);
  }
  const gap = Number((/max=(\d+)/.exec(gapReply) ?? [0, NaN])[1]);
  console.log(`deposit all: longest client frame gap ${Number.isNaN(gap) ? "unknown (no framegap answer)" : `${gap} ms`}`);
  if (Number.isNaN(gap)) fails.push("framegap never answered after the deposit");
  else if (gap >= 1000) fails.push(`the client stalled ${gap} ms after depositing all its cash (#4870)`);
  shot("atm_island_after_deposit");
  console.log(`deposit all: bank ${afterWithdraw.bank} -> ${afterDeposit.bank}, cash ${afterWithdraw.cash} -> ${afterDeposit.cash}`);
  if (d.status !== "ok") fails.push(`the deposit row was refused: ${d.detail}`);
  if (afterDeposit.cash !== 0) fails.push(`cash left after depositing all: ${afterDeposit.cash}`);
  if (afterDeposit.bank - afterWithdraw.bank !== afterWithdraw.cash) fails.push(`the bank rose by ${afterDeposit.bank - afterWithdraw.bank}, not the ${afterWithdraw.cash} carried`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "atm quick proof: FAIL" : "atm quick proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "atm-quick",
  usage: "atm-quick [EntityName]",
  run: (ctx) => runAtmQuickProof(ctx?.args ?? []),
};
