import { arrive, clientBox, sessionDir } from "../session.mjs";

// #590 on a running `up --wait` session (one client): the 1989 bank teller. The character stands in the bank
// lobby (Bank_Teller behind the vanilla post-office counter inside the building, #931), a world shot shows the teller, the player uses
// "Talk to the teller" (LF_ATMAction on the teller), keys in 100 and withdraws it. `state` before and
// after shows cash up and bank down by 100; the server log carries `[LFDebug:ATM] withdraw -100`.
// Demo for the playtest town: run it, or walk in through the bank door to the lobby counter at (175.4, 262.4) and press the interact key at the teller.
async function runBankTellerProof() {
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
  wait(6000);
  on(1, { verb: "capture", mode: "world", name: "bank_teller_world", x: 175.4, y: 1.4, z: 264.4, ox: 0.6, oy: 0.2, oz: -2.6 }, 25000);
  const before = on(1, { verb: "state" }, 10000);
  on(1, { verb: "stand", entityName: "Bank_Teller", side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const use = on(1, { verb: "use", className: "LF_ATMAction", entityName: "Bank_Teller" }, 10000);
  if (use.status !== "ok") fails.push(`talking to the teller returned ${use.status} ${use.detail}`);
  wait(1500);
  on(1, { verb: "click", widget: "Key1Button" });
  on(1, { verb: "click", widget: "Key0Button" });
  on(1, { verb: "click", widget: "Key0Button" });
  wait(300);
  on(1, { verb: "capture", mode: "ui", name: "bank_teller_screen" }, 25000);
  on(1, { verb: "click", widget: "WithdrawButton" });
  wait(2000);
  const after = on(1, { verb: "state" }, 10000);
  console.log(`before: ${before.detail}\nafter:  ${after.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "bank teller proof: FAIL" : "bank teller proof: PASS (compare cash and bank above; confirm the server log withdraw line)");
  return fails.length ? 1 : 0;
}

export default {
  name: "bank-teller-proof",
  usage: "bank-teller-proof",
  run: () => runBankTellerProof(),
};
