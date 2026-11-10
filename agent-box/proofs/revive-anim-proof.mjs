import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// A false PASS (#259): the proof used to judge only the harness replies, so a revive animation
// that never played still passed. The logs are the truth about the animation: the server must
// start it and never stop it refused, and every client must report it playing.
export function judgeReviveAnimLogs(serverText, clientTexts) {
  const out = [];
  const bad = [
    ["the character has no animation graph", "no animation graph"],
    ["the CPR clip did not play", "CPR clip did not play"],
    ["revive animation not visible", "the medic is not kneeling in the CPR loiter"],
    ["the medic has no character controller", "no character controller"],
  ];
  if (!serverText.includes("revive animation started")) out.push("server never logged 'revive animation started'");
  if (serverText.includes("STOP_REFUSED")) out.push("server stopped the animation with STOP_REFUSED");
  const all = [["server", serverText], ...clientTexts.map((t, i) => [`client ${i + 1}`, t])];
  for (const [who, text] of all) for (const [needle, label] of bad) if (text.includes(needle)) out.push(`${who} logged: ${label}`);
  clientTexts.forEach((t, i) => {
    if (!t.includes("revive animation playing")) out.push(`client ${i + 1} never logged 'revive animation playing'`);
  });
  return out;
}

function readLog(logsDir) {
  const f = newestLog(logsDir);
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

// Every log of the session, newest file each; a second run in one session judges only its own lines.
function readLogs(dir) {
  const paths = sessionPaths(dir, 2);
  return [readLog(join(paths.server, "logs")) || readLog(paths.server), ...paths.clients.map((c) => readLog(c.logs))];
}

// M2 #512: the medic kneels and does CPR while a revive runs, seen by the patient's client. Client 1
// clocks in as a medic, downs client 2, stands by the body and reports the revive hold with `cpr`
// (what the action's OnActionStart sends; the server re-checks and broadcasts). Client 2 captures
// the medic mid-CPR, then the hold is cancelled and again client 2 captures. Read the server and
// client logs for `revive animation started / playing / stopped`.
// `revive-anim-proof [--name <shot prefix>]`
async function runReviveAnimProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "m2_512_cpr");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const at = (n) => String(parseState(state(n)).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  const before = readLogs(dir).map((text) => text.length);
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  need(parseState(state(1)).faction?.includes("MEDIC"), "client 1 did not clock in as a medic");
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const patient = `Player${parseState(state(2)).player}`;
  need(parseState(state(2)).downed === "1", "client 2 was not downed");
  on(1, { verb: "stand", entityName: patient });
  wait(2500);

  // Client 2 is the patient: a world capture teleports the capturing body to the camera offset,
  // which dragged the patient out of reach and stopped the CPR (STOP_REFUSED). An orbit camera
  // over the pair plus UI captures leaves both bodies where they are. A hospital bill left open by
  // an earlier wake blurs the screen, so it is put away first. Both mid-CPR shots land inside the
  // server's 7 s overrun (LF_ReviveAnim.MAX_SECONDS).
  on(2, { verb: "click", widget: "BillClose" });
  const [mx, my, mz] = at(1);
  const [px, , pz] = at(2);
  const cx = (mx + px) / 2, cz = (mz + pz) / 2;
  on(2, { verb: "camera", args: `orbit ${cx.toFixed(2)} ${my.toFixed(2)} ${cz.toFixed(2)} 5 24` }, 10000);
  wait(1500);

  const started = on(1, { verb: "cpr", playerId: 2, mode: "start" }, 10000);
  need(started.status === "ok", `cpr start returned ${started.status} ${started.detail}`);
  wait(1800);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_loop` }, 25000);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_loop2` }, 25000);

  on(1, { verb: "cpr", playerId: 2, mode: "stop" }, 10000);
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_stopped` }, 25000);
  need(parseState(state(2)).downed === "1", "the patient left the downed state");

  const [serverText, ...clientTexts] = readLogs(dir).map((text, i) => text.slice(before[i] ?? 0));
  fails.push(...judgeReviveAnimLogs(serverText, clientTexts));
  // The shots count only if the CPR still ran: the stop must be ours, not the overrun.
  need(serverText.includes("revive animation stopped: hold cancelled"), "the CPR ended before the proof stopped it (overrun or refusal)");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "revive-anim proof: FAIL" : "revive-anim proof: PASS (start, two views mid-CPR, stop; server and both clients logged the animation playing)");
  return fails.length ? 1 : 0;
}

export default {
  name: "revive-anim-proof",
  usage: "revive-anim-proof [--name <shot prefix>]",
  run: (ctx) => runReviveAnimProof(),
};
