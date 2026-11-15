import { hostPid, joinPlainClient, readState, registerClient, selftestScratchHost, startScratchHost } from "./_scratch-host.mjs";

// #1588: the setup a crash-then-rejoin proof (#1563) runs on. A scratch host under `host watch` on a
// pool session lane, a plain client (no -scrDefine) on a scratch profile, registered through the
// harness `ui`, `type` and `click` verbs, and its state read back. Everything is torn down by pid.
//   crash-host-setup [--no-host]   --no-host is the red run: no watch, so the client never joins.
// LF_SCRATCH_KEEP=1 keeps the scratch dir for reading.
async function runCrashHostSetup(args) {
  const fails = selftestScratchHost();
  if (fails.length) {
    for (const f of fails) console.log(`FAIL  ${f}`);
    return 1;
  }
  let h = null;
  try {
    h = startScratchHost({ noHost: args.includes("--no-host") });
    if (!args.includes("--no-host")) console.log(`host: pid ${hostPid(h)} under watch pid ${h.watchPid}, port ${h.port}, lane ${h.lock}`);
    joinPlainClient(h, { joinS: args.includes("--no-host") ? 120 : 240 });
    const ui = registerClient(h);
    console.log(`registered: last ui = ${ui || "(none)"}`);
    const r = readState(h);
    console.log(`state: ${r.state.status} ${r.state.detail}`);
    if (r.parsed) {
      console.log(`crash-host-setup: PASS faction=${r.parsed.faction} cash=${r.parsed.cash} (harness state)`);
      return 0;
    }
    // A hosted header closes LF_DevGate for a non-admin: `state` answers dev_gate_closed by design.
    const l = r.fromLog;
    console.log(`host log: player 1 uid=${l?.uid || "?"} faction=${l?.faction || "?"} bank=${l?.bank || "?"} cash-topup=${l?.cashTopup || "?"}`);
    if (r.state.detail === "dev_gate_closed" && l?.uid && l.faction) {
      console.log(`crash-host-setup: PASS faction=${l.faction} bank=${l.bank} cash-topup=${l.cashTopup} (host log; harness state refused by the dev gate)`);
      return 0;
    }
    console.log(`crash-host-setup: FAIL no state (${r.state.status} ${r.state.detail}) and the host log names no faction for player 1`);
    return 1;
  } catch (e) {
    console.log(`crash-host-setup: FAIL ${e.message}`);
    return 1;
  } finally {
    if (h ?? null) h.teardown();
  }
}

export default {
  name: "crash-host-setup",
  noReset: true,
  usage: "crash-host-setup [--no-host]   (no session needed: boots its own scratch host under host watch on a pool session lane)",
  run: (ctx) => runCrashHostSetup(ctx.args ?? []),
};
