import { arrive, clientBox, sessionDir } from "../session.mjs";

// The vault rob prompt as a DEDICATED client draws it (#2418). The `use` verb runs the action's checks
// on the server, so it passed while a remote client's menu read "not ready": the vault table lives only
// on the server. `vaultoffer` reads CanBePerformedScript and the name on the client itself.
//   node agent-box/session.mjs up
//   node agent-box/session.mjs vault-offer-proof
//   node agent-box/session.mjs down
const VAULT = "Annex_Bank_Vault";

async function runVaultOfferProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const results = [];
  const check = (name, pass, detail) => {
    results.push({ name, pass, detail });
    console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`);
  };
  const offer = () => {
    const r = on(1, { verb: "vaultoffer", words: [VAULT] }, 10000);
    const m = /server=(\d) shown=(\d) perform=(\d) reason=(.*) name=(\S*)$/.exec(r.detail);
    return m ? { ok: r.status === "ok", server: m[1], shown: m[2], perform: m[3], reason: m[4].trim(), name: m[5] } : { ok: false, raw: r.detail };
  };
  const show = (o) => JSON.stringify(o);

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: VAULT });
  wait(3000);

  const bare = offer();
  check("read on a remote client", bare.ok && bare.server === "0", show(bare));
  check("no tool: refused for the tool, not 'not ready'", bare.ok && bare.perform === "0" && !/NotReady|cannot be robbed/i.test(bare.reason), show(bare));

  on(1, { verb: "drill" }, 10000);
  wait(1000);
  on(1, { verb: "hold", item: "LF_Drill" }, 10000);
  wait(1500);
  const held = offer();
  check("drill held: the prompt is performable and reads Drill", held.ok && held.perform === "1" && held.name === "#LF-Bank_Drill", show(held));

  const start = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  wait(2500);
  const drilling = offer();
  check("drilling: the client prompt reads Stop drilling", start.status === "ok" && drilling.ok && drilling.perform === "1" && drilling.name === "#LF-Bank_DrillStop", `use=${start.status} ${show(drilling)}`);

  on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  wait(2500);
  const stopped = offer();
  check("stopped: the prompt reads Drill again", stopped.ok && stopped.name === "#LF-Bank_Drill", show(stopped));

  const failed = results.filter((r) => !r.pass);
  console.log(failed.length ? `vault-offer-proof: FAIL (${failed.length} of ${results.length})` : `vault-offer-proof: PASS (${results.length} checks)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "vault-offer-proof",
  usage: "vault-offer-proof   (up, one client)",
  run: () => runVaultOfferProof(),
};
