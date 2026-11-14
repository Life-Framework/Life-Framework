import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #716 on a running `up --clients 2` session: an account change replicates to its own player only,
// through the owner index, and nobody's view changes. Client 1 clocks in at the hospital desk
// (faction and duty published), both clients withdraw 100 at the bank teller, and each client's own
// replicated view (`seen`: faction/duty/wanted and carried cash) must match the server (`state`).
// A late joiner (client 3, `session join 3`) reads its own state and client 1 still reads MEDIC.
// Client 1 then clocks out and reads CIVILIAN again. `--no-late-join` skips client 3.
async function runAccountPublishProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const seen = (n) => {
    const m = /me=(\w+)\/(\d)\/(\d) cash=(-?\d+)/.exec(on(n, { verb: "seen" }, 10000).detail);
    if (!m) return null;
    return { faction: m[1], duty: m[2], wanted: m[3], cash: m[4] };
  };
  const fails = [];
  const agree = (n, label) => {
    const server = state(n);
    const client = seen(n);
    if (!client) {
      fails.push(`${label}: client ${n} has no seen line`);
      return { server, client };
    }
    if (!String(server.faction ?? "").includes(client.faction)) fails.push(`${label}: client ${n} reads faction ${client.faction}, server says ${server.faction}`);
    if (String(server.cash) !== client.cash) fails.push(`${label}: client ${n} reads cash ${client.cash}, server says ${server.cash}`);
    return { server, client };
  };
  const desk = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const withdraw100 = (n) => {
    on(n, { verb: "stand", entityName: "Bank_Teller", side: "s" });
    wait(2500);
    on(n, { verb: "look" });
    const use = on(n, { verb: "use", className: "LF_ATMAction", entityName: "Bank_Teller" }, 10000);
    if (use.status !== "ok") fails.push(`client ${n}: the teller returned ${use.status} ${use.detail}`);
    wait(1500);
    for (const key of ["Key1Button", "Key0Button", "Key0Button"]) on(n, { verb: "click", widget: key });
    wait(300);
    on(n, { verb: "click", widget: "WithdrawButton" });
    wait(2000);
    on(n, { verb: "click", widget: "CloseButton" });
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  for (const n of [1, 2]) agree(n, "start");

  desk(1);
  const medic = agree(1, "clocked in");
  if (!medic.client || medic.client.faction !== "MEDIC" || medic.client.duty !== "1") fails.push(`client 1 does not read itself on duty as MEDIC: ${JSON.stringify(medic.client)}`);
  const other = agree(2, "peer clocked in");
  if (!other.client || other.client.faction !== "CIVILIAN") fails.push(`client 2's own state changed when client 1 clocked in: ${JSON.stringify(other.client)}`);

  // Clocking in stashes a medic's civilian cash, so each withdrawal is judged against the state just before it.
  for (const n of [1, 2]) {
    const s0 = state(n);
    withdraw100(n);
    const after = agree(n, "after withdraw");
    if (Number(after.server.bank) !== Number(s0.bank) - 100) fails.push(`client ${n}: bank ${s0.bank} -> ${after.server.bank}, expected -100`);
    if (Number(after.server.cash) !== Number(s0.cash) + 100) fails.push(`client ${n}: cash ${s0.cash} -> ${after.server.cash}, expected +100`);
  }

  if (process.argv.includes("--no-late-join")) {
    console.log("late join skipped (--no-late-join)");
  } else {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 3 did not join late");
    } else {
      arrive(on, 3, wait);
      wait(8000);
      agree(3, "late joiner");
      const still = seen(1);
      if (!still || still.faction !== "MEDIC") fails.push(`after the late join client 1 reads ${JSON.stringify(still)}`);
    }
  }

  desk(1);
  const off = agree(1, "clocked out");
  if (!off.client || off.client.faction !== "CIVILIAN" || off.client.duty !== "0") fails.push(`client 1 does not read itself off duty: ${JSON.stringify(off.client)}`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "account publish proof: FAIL" : "account publish proof: PASS (each client's replicated faction, duty and cash match the server through clock-in, withdrawals, a late join and clock-out)");
  return fails.length ? 1 : 0;
}

export default {
  name: "account-publish-proof",
  usage: "account-publish-proof [--no-late-join]",
  run: () => runAccountPublishProof(),
};
