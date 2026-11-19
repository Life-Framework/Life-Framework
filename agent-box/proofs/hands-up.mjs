import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #3396 hands up: the target chooses. Two clients, then a late joiner.
//   node agent-box/session.mjs up --clients 2
//   node agent-box/session.mjs hands-up
//   node agent-box/session.mjs down
// Client 1 is A (aims), client 2 is B (target). The held aim is stood in for by the dev-gated
// `handsup offer`, which runs the same server offer a real aim or hold-up does. Every check reads
// what a CLIENT draws (`handsup state`: the prompt and each player's replicated flag).
// Player ids are taken from the roster: the lower id is client 1, the higher client 2.
async function runHandsUpProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const results = [];
  const check = (name, pass, detail) => {
    results.push({ name, pass, detail });
    console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`);
  };
  const read = (n) => parseState(on(n, { verb: "handsup", words: ["state"] }, 10000).detail);
  const until = (n, judge, budgetMs) => {
    const t0 = Date.now();
    let s;
    do {
      s = read(n);
      if (judge(s)) break;
      wait(1000);
    } while (Date.now() - t0 < budgetMs);
    return s;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(6000);
  const ids = Object.keys(read(1)).filter((k) => /^p\d+$/.test(k)).map((k) => Number(k.slice(1))).sort((a, b) => a - b);
  if (ids.length < 2) {
    check("two players in the session", false, `ids=${ids.join(",")}`);
    return 1;
  }
  const [aId, bId] = ids;
  const flag = (s, id) => s[`p${id}`];

  // 1. Ignoring the prompt forces nothing; the offer reaches only B.
  on(1, { verb: "handsup", words: ["offer", String(bId)] }, 10000);
  const shown = until(2, (s) => s.prompt === "shown", 8000);
  check("B's prompt shows", shown.prompt === "shown", `client 2 prompt=${shown.prompt}`);
  check("A sees no prompt", read(1).prompt === "hidden", `client 1 prompt=${read(1).prompt}`);
  check("an offer alone raises nothing", flag(read(2), bId) === "false", `p${bId}=${flag(read(2), bId)}`);
  const ignored = until(2, (s) => s.prompt === "hidden", 12000);
  check("ignoring the prompt forces nothing", ignored.prompt === "hidden" && flag(ignored, bId) === "false", `prompt=${ignored.prompt} p${bId}=${flag(ignored, bId)}`);

  // 2. A cannot raise B's hands; with no live offer B's own answer does nothing either.
  on(1, { verb: "handsup", words: ["choose", "1"] }, 10000);
  wait(1500);
  check("an answer with no offer does nothing", flag(read(2), bId) === "false" && flag(read(2), aId) === "false", `p${aId}=${flag(read(2), aId)} p${bId}=${flag(read(2), bId)}`);

  // 3. B chooses (the re-offer pause is 20 s): both clients read B up.
  wait(20000);
  on(1, { verb: "handsup", words: ["offer", String(bId)] }, 10000);
  until(2, (s) => s.prompt === "shown", 8000);
  on(2, { verb: "handsup", words: ["choose", "1"] }, 10000);
  const upB = until(2, (s) => flag(s, bId) === "true", 8000);
  const upA = until(1, (s) => flag(s, bId) === "true", 8000);
  check("B's choice raises the hands on B's client", flag(upB, bId) === "true", `client 2 p${bId}=${flag(upB, bId)}`);
  check("A sees B with hands up", flag(upA, bId) === "true", `client 1 p${bId}=${flag(upA, bId)}`);

  // 4. A late joiner reads B up.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) {
    check("late joiner connects", false, `session join 3 exited ${joined.status}`);
  } else {
    arrive(on, 3, wait);
    wait(6000);
    const late = until(3, (s) => flag(s, bId) === "true", 15000);
    check("a late joiner sees B with hands up", flag(late, bId) === "true", `client 3 p${bId}=${flag(late, bId)}`);
  }

  // 5. B walks off (more than 1.5 m): the state ends for everyone.
  on(2, { verb: "walk", words: ["4", "1"] }, 20000);
  const down = until(1, (s) => flag(s, bId) === "false", 15000);
  check("B walking off ends the state", flag(down, bId) === "false", `client 1 p${bId}=${flag(down, bId)}`);

  const failed = results.filter((r) => !r.pass);
  console.log("");
  console.log("| Check | Result | Observed |");
  console.log("|---|---|---|");
  for (const r of results) console.log(`| ${r.name} | ${r.pass ? "pass" : "FAIL"} | ${r.detail.replace(/\|/g, "/")} |`);
  console.log(failed.length ? `hands-up proof: FAIL (${failed.length} of ${results.length})` : `hands-up proof: PASS (${results.length} checks)`);
  return failed.length ? 1 : 0;
}

export default { name: "hands-up", usage: "hands-up   (up --clients 2 first; joins client 3 itself)", run: () => runHandsUpProof() };
