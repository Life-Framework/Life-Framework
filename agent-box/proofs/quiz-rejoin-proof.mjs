import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { ROOT, arrive, clientBox, clientSeesPeer, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// Chapter #1006 beat B4 (#1490): a new arrival passes the rules quiz, and a rejoin never asks again.
//   node agent-box/session.mjs up --pack LF_Everon1989 --mission playtest --clients 1 --wait
//   node agent-box/session.mjs quiz-rejoin-proof
//   node agent-box/session.mjs down
// The quiz questions come from the flagship's mission header (m_aLF_ConfigOverlays), so the session
// boots that header with --pack; the DebugWorld and the bare playtest header carry no questions and
// the quiz is off there. Client 1 registers and holds a receipt in hand. Client 2 then joins late
// (`session join 2`, a fresh profile, so a new account): the quiz comes before the form, it answers
// right, registers and spawns, and its own client reads client 1's body (`peers`). It quits and
// rejoins (`session rejoin 2`, same identity, new player id): the server must spawn it with no quiz
// and no form. Logs are the proof; `quiz-proof` and `rejoin-proof` cover the screens and the slot.
//
// The server log is the judge. `judgeQuizRejoin` is pure; `--selftest` feeds it a good log and the
// log a cleared quiz-passed flag leaves (the rejoin is sent the quiz again): that one fails at
// "no second quiz". The flag lives in the server's memory and progression store; `--clear-pass` sends
// the harness verb `quizclear` (#1567) for client 2 before the rejoin, so the live run is the red run
// too: the plain run passes, `--clear-pass` fails at "no second quiz" (#1568).
const NO_SECOND_QUIZ = "no second quiz";
const RECEIPT = "{3E8A5C1D7B2F4096}Prefabs/Items/Roleplay/LF_Receipt.et";

export function judgeQuizRejoin(text, first, second) {
  const fails = [];
  const has = (re) => re.test(text);
  if (!first?.playerId || !second?.playerId) return ["no player id recorded for client 2 before or after the rejoin"];
  if (first.playerId === second.playerId) fails.push(`client 2 kept player id ${first.playerId}: it did not really rejoin`);
  if (first.uid && second.uid && first.uid !== second.uid) fails.push(`client 2 is ${second.uid} after the rejoin, was ${first.uid}`);
  if (!has(new RegExp(`quiz sent to player ${first.playerId}\\b`))) fails.push(`the new arrival (player ${first.playerId}) was never sent the quiz`);
  if (!has(new RegExp(`quiz passed by ${first.uid}\\b`))) fails.push(`the server never logged "quiz passed by ${first.uid}"`);
  if (!has(new RegExp(`registration form sent to player ${first.playerId}\\b`))) fails.push(`the new arrival (player ${first.playerId}) was never sent the registration form`);
  if (!has(new RegExp(`spawning player ${first.playerId}\\b`))) fails.push(`the new arrival (player ${first.playerId}) never spawned`);
  if (has(new RegExp(`quiz sent to player ${second.playerId}\\b`))) fails.push(`${NO_SECOND_QUIZ}: the server sent player ${second.playerId} the quiz again`);
  if (has(new RegExp(`registration form sent to player ${second.playerId}\\b`))) fails.push(`no second form: the server sent player ${second.playerId} the registration form again`);
  if (!has(new RegExp(`spawning player ${second.playerId}\\b`))) fails.push(`the rejoined player ${second.playerId} was never spawned`);
  return fails;
}

export function selftestQuizRejoin() {
  const f = [];
  const first = { playerId: 2, uid: "00bbbddd-5e55-0000-0000-000000000002" };
  const second = { playerId: 3, uid: first.uid };
  const arrival = [
    `[LFDebug:Rules] quiz sent to player 2 (${first.uid}), 3 attempt(s) left`,
    `[LFDebug:Rules] quiz passed by ${first.uid}: 5 of 5 right (needed 4)`,
    "[LFDebug:CharacterCreation] registration form sent to player 2",
    "[LFDebug:CharacterCreation] spawning player 2",
  ];
  const good = [...arrival, "[LFDebug:CharacterCreation] spawning player 3"].join("\n");
  const cleared = [...arrival, `[LFDebug:Rules] quiz sent to player 3 (${first.uid}), 3 attempt(s) left`].join("\n");
  const clearedFull = [...arrival, `[LFDebug:Rules] quiz sent to player 3 (${first.uid}), 3 attempt(s) left`, "[LFDebug:CharacterCreation] spawning player 3"].join("\n");
  const goodFails = judgeQuizRejoin(good, first, second);
  if (goodFails.length) f.push(`a good rejoin failed: ${goodFails.join("; ")}`);
  // red-proof 2026-11-15: with the "quiz sent to player <rejoin id>" check removed, the cleared-flag log below passed.
  if (!judgeQuizRejoin(clearedFull, first, second).some((x) => x.startsWith(NO_SECOND_QUIZ))) f.push(`a second quiz did not fail at "${NO_SECOND_QUIZ}"`);
  if (!judgeQuizRejoin(cleared, first, second).length) f.push("a rejoin that was never spawned passed");
  if (!judgeQuizRejoin(arrival.slice(1).join("\n"), first, second).some((x) => x.includes("was never sent the quiz"))) f.push("an arrival with no quiz passed");
  if (!judgeQuizRejoin(good, first, { ...second, playerId: 2 }).some((x) => x.includes("did not really rejoin"))) f.push("a rejoin with the same player id passed");
  return f;
}

async function runQuizRejoinProof(ctx) {
  if (ctx?.args?.includes("--selftest")) {
    const f = selftestQuizRejoin();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "quiz-rejoin selftest: FAIL" : "quiz-rejoin selftest: OK");
    return f.length ? 1 : 0;
  }
  const clearPass = !!ctx?.args?.includes("--clear-pass");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const cli = (...args) => spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", ...args], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });

  // Client 1 is in town with a receipt in hand before anyone else joins.
  arrive(on, 1, wait);
  wait(8000);
  const given = on(1, { verb: "itemgive", prefab: RECEIPT }, 10000);
  if (given.status !== "ok") fails.push(`client 1 could not be given the receipt: ${given.status} ${given.detail}`);
  wait(1500);
  const held = on(1, { verb: "hold", item: "LF_Receipt" }, 10000);
  if (held.status !== "ok") fails.push(`client 1 could not hold the receipt: ${held.status} ${held.detail}`);
  wait(3000);
  const inHand = on(1, { verb: "heldxf" }, 10000);
  if (inHand.status !== "ok" || !/item=LF_Receipt/.test(inHand.detail)) fails.push(`client 1 reports no receipt in hand: ${inHand.status} ${inHand.detail}`);

  // Client 2 joins late on a fresh profile and meets the quiz first.
  const joined = cli("join", "2");
  process.stdout.write(joined.stdout ?? "");
  if (joined.status !== 0) {
    fails.push(`session join 2 exited ${joined.status}: client 2 did not join late`);
  } else {
    wait(6000);
    const ui = on(2, { verb: "ui" });
    if (!ui.detail.includes("QuizOption0Button")) fails.push("the late joiner does not see the quiz first");
    if (ui.detail.includes("SignButton")) fails.push("the registration form is open before the quiz is passed");
    arrive(on, 2, wait);
    wait(10000);
    const st = parseState(on(2, { verb: "state" }, 10000).detail);
    if (String(st.faction ?? "").toLowerCase() !== "civilian") fails.push(`client 2 did not spawn as a civilian after the quiz and form (faction=${st.faction})`);
  }

  // Client 2 reads client 1's body: it is streamed in and alive while client 1 holds the receipt.
  const sees = clientSeesPeer(on, wait, 2, 1, (peer) => (peer.life === "ALIVE" ? [] : [`player 1 life=${peer.life}, expected ALIVE`]), { budgetMs: 15000, label: "late joiner" });
  for (const x of sees.fails) fails.push(x);
  // The peer's held item is not readable as text by any harness verb (peers lists no hand item), so
  // the item itself is client 1's own `heldxf` above; a text verb for it is the follow-up card.
  console.log("NOTE  client 1's held item is checked on client 1 only; no harness verb reads a peer's hand item");

  const s0 = readSession(dir);
  const was = s0?.clients?.find((c) => c.n === 2)?.identity;
  if (!was) fails.push("client 2 has no recorded identity before the rejoin");

  // --clear-pass is the live red run (#1568): the server forgets client 2's quiz pass first (`quizclear`,
  // #1567, LF_HARNESS build only), so the rejoin must be asked the quiz again and the judge fails at
  // "no second quiz". Without it the proof is the green run.
  if (clearPass) {
    const cleared = on(2, { verb: "quizclear" }, 10000);
    if (cleared.status !== "ok") fails.push(`quizclear was refused for client 2: ${cleared.status} ${cleared.detail}`);
    else if (!/had=1\b/.test(cleared.detail)) fails.push(`quizclear found no quiz pass to forget for client 2: ${cleared.detail}`);
    wait(1500);
  }

  // Client 2 leaves and rejoins: same account, new player id.
  const re = cli("rejoin", "2");
  process.stdout.write(re.stdout ?? "");
  if (re.status !== 0) fails.push(`session rejoin 2 exited ${re.status}`);
  wait(15000);
  const s1 = readSession(dir);
  const now = s1?.clients?.find((c) => c.n === 2)?.identity;
  if (!now) fails.push("client 2 has no recorded identity after the rejoin");

  if (was && now) {
    const log = newestLog(join(s1.serverProfile, "logs"));
    const text = log ? readFileSync(log, "utf8") : "";
    if (!text) fails.push("no server log to read");
    else for (const x of judgeQuizRejoin(text, was, now)) fails.push(x);
  }
  // On its own screen: no quiz sheet, and a living character.
  const after = on(2, { verb: "ui" });
  if (after.detail.includes("QuizOption0Button")) fails.push(`${NO_SECOND_QUIZ}: client 2 is shown the quiz after the rejoin`);
  const st2 = parseState(on(2, { verb: "state" }, 10000).detail);
  if (String(st2.faction ?? "").toLowerCase() !== "civilian") fails.push(`after the rejoin client 2 is faction=${st2.faction}, expected civilian (spawned, not on a form)`);

  console.log(`quiz-rejoin proof: client 2 player ${was?.playerId} -> ${now?.playerId}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  if (clearPass) console.log(fails.some((x) => x.startsWith(NO_SECOND_QUIZ)) ? `quiz-rejoin --clear-pass: red at "${NO_SECOND_QUIZ}", as the cleared pass should` : `quiz-rejoin --clear-pass: NOT red at "${NO_SECOND_QUIZ}": the cleared pass did not bring the quiz back`);
  console.log(fails.length ? "quiz-rejoin proof: FAIL" : "quiz-rejoin proof: PASS (quiz, form and spawn once; the rejoin spawned with no second quiz)");
  return fails.length ? 1 : 0;
}

export default {
  name: "quiz-rejoin-proof",
  usage: "quiz-rejoin-proof [--selftest] [--clear-pass]   (session up --pack LF_Everon1989 --mission playtest --clients 1; --clear-pass sends quizclear before the rejoin and must fail at \"no second quiz\")",
  run: (ctx) => runQuizRejoinProof(ctx),
};

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1] && process.argv.includes("--selftest")) {
  runQuizRejoinProof({ args: ["--selftest"] }).then((c) => process.exit(c));
}
