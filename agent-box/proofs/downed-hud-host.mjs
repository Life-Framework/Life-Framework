import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The downed HUD line on a LISTEN HOST (#5172): the server sets the downed flag on the host's own
// character, so the host's client never gets a replication callback for it and the line used to stay off.
// Client 1 is the host (`up --host`), client 2 a joiner. Client 2 downs the host; the host must read the
// "Bleeding out" line, its clock must run, and after the give-up delay it must read "Hold X to give up".
//   node agent-box/session.mjs up --host --clients 1 --mission island --profile full --hold 20
//   node agent-box/session.mjs downed-hud-host
//   node agent-box/session.mjs down
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";

async function runDownedHudHost() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const hostLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const hud = (n) => /downedhud=(\S+)/.exec(on(n, { verb: "seen" }, 10000).detail)?.[1] ?? "";
  const bleedOf = (line) => {
    const m = /Bleeding_out:_(\d+):(\d+)/.exec(line);
    return m ? Number(m[1]) * 60 + Number(m[2]) : -1;
  };

  need(sessionPaths(dir).server === sessionPaths(dir).clients[0].profile, "this session is not an `up --host` one (the server profile is not client 1's)");
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(4000);

  // The spawn square refuses damage; the clinic desk is outside it (as in downed-flow-proof).
  on(1, { verb: "stand", entityName: DESK });
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  const hostId = parseState(on(1, { verb: "state" }, 10000).detail).player;
  need(!!hostId, "the host reports no player id");
  need(hud(1) === "" || hud(1) === "none" || !/Bleeding_out/.test(hud(1)), "the host shows a downed line before it is downed");
  const shot = on(2, { verb: "shoot", playerId: Number(hostId) }, 10000);
  need(/downed=1/.test(shot.detail), `the harness shot did not down the host: ${shot.detail}`);
  wait(3000);

  let first = hud(1);
  for (let i = 0; i < 5 && !/Bleeding_out/.test(first); i++) {
    wait(1500);
    first = hud(1);
  }
  on(1, { verb: "capture", mode: "ui", name: "downed_hud_host" }, 25000);
  need(/Bleeding_out/.test(first), `the host reads no downed HUD line: "${first}"`);
  need(/give_up_in/.test(first), `the host line is not the wait text right after the fall: "${first}"`);
  const joinerSees = parseState(on(2, { verb: "state" }, 10000).detail);
  need(joinerSees.player !== undefined, "the joiner reads no state");

  // The clock runs on the host's own client, and the wait ends with the replicated seconds downed.
  wait(36000);
  const later = hud(1);
  need(/Bleeding_out/.test(later), `the host's line went away while it is still downed: "${later}"`);
  need(bleedOf(later) >= 0 && bleedOf(first) >= 0 && bleedOf(later) < bleedOf(first), `the host's bleed clock did not run (${bleedOf(first)}s then ${bleedOf(later)}s)`);
  need(/Hold_X_to_give_up/.test(later), `the host never reached the give-up text after the delay: "${later}"`);

  const log = hostLog();
  need(!/VME|SCRIPT \(E\)/.test(log.split("\n").filter((l) => /LF_Downed|LFDebug:(Downed|BleedOut|Medic)/.test(l)).join("\n")), "a VME or SCRIPT (E) from the downed feature in the host log");

  console.log("");
  console.log(`host hud right after the fall: ${first}`);
  console.log(`host hud 36 s later:           ${later}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed hud host: FAIL" : "downed hud host: PASS (the listen host reads its own downed line and clock)");
  return fails.length ? 1 : 0;
}

export default {
  name: "downed-hud-host",
  usage: "downed-hud-host   (up --host --clients 1 --mission island --profile full --hold 20)",
  run: () => runDownedHudHost(),
};
