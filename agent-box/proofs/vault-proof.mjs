import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #273 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in; client 1 is handed a stamped drill (dev gate, standing in for the
// fence), opens the bank vault with it, and the drill is left at the door. Client 2 bags it and logs
// the bag at the evidence locker, which identifies the buyer. Confirm `[LFDebug:Serials] ... bought by`
// in the server log.
async function runVaultProof() {
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
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const noTool = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  const give = on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "m2_273_vault_client1" }, 25000);
  on(2, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const bag = on(2, { verb: "use", className: "LF_BagEvidenceAction" }, 10000);
  wait(1500);
  on(2, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(2500);
  const log = on(2, { verb: "use", className: "LF_LogEvidenceAction", entityName: "Roadmap_Police_EvidenceLocker" }, 10000);
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_273_locker_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (noTool.status === "ok") fails.push(`the vault opened without a tool: ${noTool.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`drill not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (bag.status !== "ok") fails.push(`bagging returned ${bag.status} ${bag.detail}`);
  if (log.status !== "ok") fails.push(`logging returned ${log.status} ${log.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "vault proof: FAIL" : "vault proof: PASS (refused without a tool, opened with the drill, drill bagged, buyer identified at the locker; confirm the server log)");
  return fails.length ? 1 : 0;
}

export default {
  name: "vault-proof",
  usage: "vault-proof",
  run: (ctx) => runVaultProof(),
};
