import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// The session-fixtures proof, on `up --clients 2 --mission playtest --roster 2`:
//  1. the roster written by `up` survived the boot: client 2 clocks in at the police desk (a refusal
//     is `NotOnRoster`, the roster file never reached the server);
//  2. client 2 quits and rejoins (`session rejoin 2`): the server logs a NEW player id for it, and the
//     harness sees the SAME identity (the session slot), so the account is the returning one:
//     the server logs `spawning player <new id>`, not a registration form;
//  3. client 2 still stands on the police roster: it clocks in again after the rejoin.
async function runRejoinProof() {
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
  const clockIn = () => {
    on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    wait(2500);
    on(2, { verb: "look" });
    const r = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
    wait(3000);
    return { r, state: parseState(on(2, { verb: "state" }, 10000).detail) };
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const first = clockIn();
  if (String(first.state.faction).toLowerCase() !== "police" || first.state.duty !== "1") fails.push(`before the rejoin client 2 is not an officer on duty (faction=${first.state.faction} duty=${first.state.duty}): the roster did not reach the server`);

  const s0 = readSession(dir);
  const was = s0?.clients?.find((c) => c.n === 2)?.identity;
  if (!was) fails.push("client 2 has no recorded identity before the rejoin");

  // The rejoin runs where the session runs: this proof already does (a remote session relays it).
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "rejoin", "2"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
  process.stdout.write(r.stdout ?? "");
  if (r.status !== 0) fails.push(`session rejoin 2 exited ${r.status}: the rejoined client is not the same identity`);

  const now = readSession(dir)?.clients?.find((c) => c.n === 2)?.identity;
  if (was && now) {
    if (now.playerId === was.playerId) fails.push(`client 2 kept player id ${now.playerId}: it did not really rejoin`);
    if (now.uid !== was.uid) fails.push(`client 2 is ${now.uid}, was ${was.uid}`);
    const log = newestLog(join(readSession(dir).serverProfile, "logs"));
    const text = log ? readFileSync(log, "utf8") : "";
    if (!new RegExp(`spawning player ${now.playerId}\\b`).test(text)) fails.push(`the server did not log "spawning player ${now.playerId}" (a returning account)`);
    if (new RegExp(`registration form sent to player ${now.playerId}\\b`).test(text)) fails.push(`the server sent player ${now.playerId} a registration form: it was a new account`);
  }

  // A returning account spawns without the registration form; give it time to load, then clock in again.
  wait(15000);
  let after = clockIn();
  if (String(after.state.faction).toLowerCase() !== "police") {
    wait(8000);
    after = clockIn();
  }
  if (String(after.state.faction).toLowerCase() !== "police") fails.push(`after the rejoin client 2 is faction=${after.state.faction}, not police (clock in: ${after.r.status} ${after.r.detail})`);
  console.log(`rejoin proof: identity ${was?.uid} -> ${now?.uid}; client 2 after the rejoin: faction=${after.state.faction} duty=${after.state.duty}`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "rejoin proof: FAIL" : "rejoin proof: PASS (roster seeded by up, same identity after the rejoin, returning account)");
  return fails.length ? 1 : 0;
}

export default {
  name: "rejoin-proof",
  usage: "rejoin-proof   (session up --clients 2 --mission playtest --roster 2)",
  run: () => runRejoinProof(),
};
