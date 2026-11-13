// #147 telephone calls on a running `up --clients 2` session (lowspec is enough: it reads lines and
// logs, no pictures), with client 2 on the police roster (`session.mjs roster 2` before up).
// Client 2 clocks in and stands at the station telephone; client 1 rings the station from the fuel
// payphone. Judged on what each client's lines show (`harness phone`) and on the server's
// [LFDebug:Phone] lines: the station rings, the officer picks up, both talk on busy lines, the
// caller hangs up; a call where the caller walks off ends; an unanswered call rings out as MISSED;
// the caller leaves a call-back request with a message, the officer reads the pad and rings the
// payphone back, the caller picks up.
// The payphone cooldowns (60 s per player) set the pace: about four minutes.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { rosterHas } from "./scanner-proof.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

export const PAYPHONE = "Roadmap_Fuel_Payphone";
export const STATION = "Roadmap_Dispatch_Phone";
export const DESK = "Roadmap_Police_Desk";
const PLAYER_COOLDOWN_MS = 62000;
const RING_OUT_MS = 33000;

/** "lines=2 253,230:RINGING:0:3:0 126,254:BUSY:1:0:0" -> [{ key, state, talker, rings, offer }] */
export function parseLines(detail) {
  return String(detail ?? "").trim().split(/\s+/).slice(1).map((part) => {
    const [key, state, talker, rings, offer] = part.split(":");
    return { key, state, talker: Number(talker), rings: Number(rings), offer: Number(offer ?? 0) };
  }).filter((l) => l.key && l.state);
}

/** "... voice=up call=1 entry=1 txSet=0 ..." -> { voice: "up", call: "1", entry: "1", ... } */
export function parseVoice(detail) {
  const out = {};
  for (const m of String(detail ?? "").matchAll(/(\w+)=([^\s]+)/g)) out[m[1]] = m[2];
  return out;
}

export const has = (lines, state) => lines.some((l) => l.state === state && (state !== "BUSY" || l.talker > 0));

async function runPhoneProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = (n, name) => {
    on(n, { verb: "stand", entityName: name });
    wait(2500);
    on(n, { verb: "look" });
  };
  const use = (n, className, entityName) => on(n, { verb: "use", className, entityName }, 10000);
  const lines = (n) => parseLines(on(n, { verb: "phone" }, 10000).detail);
  const voice = (n) => parseVoice(on(n, { verb: "phone" }, 10000).detail);
  const fails = [];
  const expect = (ok, what) => { if (!ok) fails.push(what); };

  if (!rosterHas(dir, 2)) {
    console.log("phone proof: FAIL  client 2 is not on the police roster; run `session.mjs down`, `session.mjs roster 2`, then `up --clients 2` again");
    return 1;
  }

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  console.log("\n== client 2 clocks in and waits at the station telephone; client 1 goes to the payphone");
  at(2, DESK);
  expect(use(2, "LF_DutyAction", DESK).status === "ok", "client 2 clock in failed");
  wait(3000);
  at(2, STATION);
  at(1, PAYPHONE);

  console.log("\n== call 1: ring, pick up, talk, the caller hangs up");
  expect(use(1, "LF_PhoneDialAction", PAYPHONE).status === "ok", "client 1 could not use the dial action");
  wait(5000);
  const ringing2 = lines(2);
  const dialling1 = lines(1);
  expect(has(ringing2, "RINGING"), "the station telephone does not show RINGING on the officer's client");
  expect(ringing2.some((l) => l.state === "RINGING" && l.rings > 0), "the officer's client heard no ring");
  expect(has(dialling1, "BUSY"), "the payphone does not show the caller on it");
  expect(use(2, "LF_PhoneAnswerAction", STATION).status === "ok", "client 2 could not pick up");
  wait(3000);
  expect(has(lines(2), "BUSY"), "the station telephone does not show the officer on it");
  expect(has(lines(1), "BUSY"), "the payphone line is not busy while talking");
  for (const n of [1, 2]) {
    // The radio's state reaches the client a moment after the entry attaches: poll up to 20 s.
    let v = voice(n);
    for (let i = 0; i < 8 && !/^1\d{5}:1$/.test(v.radio ?? ""); i++) {
      wait(2500);
      v = voice(n);
    }
    expect(v.voice === "up" && v.entry === "1", `client ${n}: the telephone is not the active voice entry while talking (voice=${v.voice} entry=${v.entry})`);
    expect(/^1\d{5}:1$/.test(v.radio ?? ""), `client ${n}: the telephone radio is not powered on the call's private-band frequency here (radio=${v.radio})`);
  }
  expect(use(1, "LF_PhoneHangUpAction", PAYPHONE).status === "ok", "client 1 could not hang up");
  wait(3000);
  expect(!has(lines(1), "BUSY") && !has(lines(2), "BUSY") && !has(lines(2), "RINGING"), "a line is still busy or ringing after the hang-up");
  for (const n of [1, 2]) {
    const v = voice(n);
    expect(v.voice === "down" && v.entry === "0", `client ${n}: the telephone entry is still there after the hang-up (voice=${v.voice} entry=${v.entry})`);
  }

  console.log(`\n== call 2: the caller walks off a connected call (waiting ${PLAYER_COOLDOWN_MS / 1000} s for the payphone cooldown)`);
  wait(PLAYER_COOLDOWN_MS);
  use(1, "LF_PhoneDialAction", PAYPHONE);
  wait(4000);
  use(2, "LF_PhoneAnswerAction", STATION);
  wait(3000);
  at(1, DESK);
  wait(3000);
  expect(!has(lines(2), "BUSY"), "the station line is still busy after the caller walked away");
  at(1, PAYPHONE);

  console.log(`\n== call 3: nobody picks up (waiting ${PLAYER_COOLDOWN_MS / 1000} s, then ${RING_OUT_MS / 1000} s of ringing)`);
  wait(PLAYER_COOLDOWN_MS);
  use(1, "LF_PhoneDialAction", PAYPHONE);
  wait(RING_OUT_MS);
  expect(!has(lines(2), "RINGING"), "the station still rings after the ring time");

  console.log("\n== call-back: client 1 leaves a request at the payphone; client 2 rings it back from the pad");
  expect(lines(1).some((l) => l.offer > 0), "the payphone line shows no call-back offer after the ring-out");
  expect(use(1, "LF_PhoneLeaveRequestAction", PAYPHONE).status === "ok", "client 1 could not open the message slip");
  wait(2500);
  on(1, { verb: "type", widget: "PhoneSlipMessage", text: "Robbery_at_the_bank" });
  expect(on(1, { verb: "click", widget: "PhoneSlipLeaveButton" }).status === "ok", "client 1 could not leave the message");
  wait(3000);
  expect(use(2, "LF_PhoneReadPadAction", STATION).status === "ok", "client 2 could not read the pad");
  wait(2500);
  const pad = on(2, { verb: "ui" }, 10000).detail;
  const row = /PhoneRequest(\d+)/.exec(pad);
  expect(row, "the pad lists no request");
  if (row) {
    expect(on(2, { verb: "click", widget: `PhoneRequest${row[1]}_Action` }).status === "ok", "client 2 could not press Ring back");
    wait(5000);
    expect(has(lines(1), "RINGING"), "the payphone does not ring back");
    expect(use(1, "LF_PhoneAnswerAction", PAYPHONE).status === "ok", "client 1 could not pick up the ring-back");
    wait(3000);
    expect(has(lines(1), "BUSY") && has(lines(2), "BUSY"), "the ring-back did not connect both lines");
    expect(use(2, "LF_PhoneHangUpAction", STATION).status === "ok", "client 2 could not hang up the ring-back");
    wait(3000);
  }

  const log = newestLog(join(sessionPaths(dir).server, "logs"));
  const server = log ? readFileSync(log, "utf8").split(/\r?\n/).filter((l) => l.includes("[LFDebug:Phone]")) : [];
  for (const l of server) console.log(`server: ${l.replace(/^.*\[LFDebug:Phone\] /, "")}`);
  const saw = (re, what) => expect(server.some((l) => re.test(l)), `server log: ${what}`);
  saw(/call \d+ RINGING: player \d+ dialled police/, "no RINGING line");
  saw(/call \d+ CONNECTED/, "no CONNECTED line");
  saw(/call \d+ ENDED \(HUNG_UP\)/, "no ENDED (HUNG_UP) line");
  saw(/call \d+ ENDED \(WALKED_AWAY\)/, "no ENDED (WALKED_AWAY) line");
  saw(/call \d+ MISSED \(NO_ANSWER\)/, "no MISSED (NO_ANSWER) line");
  saw(/call-back request \d+ left by .*'Robbery_at_the_bank'/, "no call-back request left line");
  saw(/call-back request \d+ rung back by/, "no ring-back line");
  saw(/call-back request \d+ answered at/, "no answered ring-back line");
  saw(/call \d+ voice up over the line transport: \d+ kHz, caller radio rplId \S+ powered=1 f=\d+ key=LF_call_/, "no voice up line with powered lines");
  saw(/call \d+ voice down/, "no voice down line");
  expect(!server.some((l) => /Error|refused after the coin/.test(l)), "server log: a phone error line");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "phone proof: FAIL" : "phone proof: PASS (rings at the station, on-duty pick-up, hang-up, walk-away and ring-out end the call; call-back request left, read and rung back; lines follow on both clients)");
  return fails.length ? 1 : 0;
}

export default {
  name: "phone-proof",
  usage: "phone-proof",
  run: (ctx) => runPhoneProof(),
};
