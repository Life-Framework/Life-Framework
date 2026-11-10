import { arrive, clientBox, sessionDir } from "../session.mjs";

// #605 on a running `up --clients 1` session (the playtest town): the client stands at the robbery
// kit stand beside the bank and takes the kit, a second press adds nothing, then the vault opens with
// what the stand handed over. Confirm `[LFDebug:BankKit] granted 3 of 3` then `granted 0 of 3` in the
// server log. Also the demo for docs/playtest.md.
async function runBankKitProof() {
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
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Bank_RobKit" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "bank_kit_stand_client1" }, 25000);
  const take = on(1, { verb: "use", className: "LF_BankRobKitAction", entityName: "Roadmap_Bank_RobKit" }, 10000);
  wait(1500);
  const again = on(1, { verb: "use", className: "LF_BankRobKitAction", entityName: "Roadmap_Bank_RobKit" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "bank_kit_vault_client1" }, 25000);
  if (take.status !== "ok") fails.push(`take returned ${take.status} ${take.detail}`);
  if (again.status !== "ok") fails.push(`second take returned ${again.status} ${again.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "bank kit proof: FAIL" : "bank kit proof: PASS (confirm the server log for granted 3 of 3, then granted 0 of 3, then the vault opening)");
  return fails.length ? 1 : 0;
}

export default {
  name: "bank-kit-proof",
  usage: "bank-kit-proof",
  run: (ctx) => runBankKitProof(),
};
