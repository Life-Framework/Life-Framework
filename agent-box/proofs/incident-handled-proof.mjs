import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths, sessionUid } from "../session.mjs";

// M4 #1451, beat 3 on a running `up --clients 2` session: client 2 damages client 1 at the fuel
// station, client 1 files /report, and an administrator lists the incident in the Game Master tools
// and takes one admin action (never a ban, #1285). The verdict is the server log, in order: the
// incident row for the hit, the report's flag, then the admin row. The first two legs are driven
// here. The admin leg needs an ADMINISTRATOR client and a way to send `!handled`/`#kick`; the
// harness has neither (see README "gm"), so it is judged when a client is an administrator and
// reported as SPLIT when none is (pass --require-admin to make that a failure).
//
//   incident-handled-proof [--skip-report] [--require-admin]
//   --skip-report  the red run: the report is never filed, so the proof fails at "flag"

const HIT_KINDS = ["damage", "downed", "death"];

// Index of the first log line after `from` that holds every needle, or -1.
export function findLine(lines, from, ...needles) {
  for (let i = from; i < lines.length; i++) {
    if (needles.every((n) => lines[i].includes(n))) return i;
  }
  return -1;
}

// The ordered judgement over the server log lines taken after the proof's baseline. Pure.
// victim and attacker are session uids. Returns { fails, admin } where admin is "ok", "split" or "fail".
export function judgeIncident(lines, victim, attacker, requireAdmin = false) {
  const fails = [];
  let hit = -1;
  for (const kind of HIT_KINDS) {
    const at = findLine(lines, 0, "[LFDebug:Incident]", `${kind}:`, victim, attacker);
    if (at >= 0 && (hit < 0 || at < hit)) hit = at;
  }
  if (hit < 0) fails.push(`hit: no damage, downed or death row for ${victim} by ${attacker} in the incident log`);

  const flag = findLine(lines, Math.max(hit, 0), "[LFDebug:IncidentReport]", `report from ${victim} flagged`);
  if (flag < 0) fails.push(`flag: the report from ${victim} flagged nothing after the hit`);

  const list = findLine(lines, Math.max(flag, 0), "[LFDebug:IncidentGM]", "sent", "to admin");
  const refused = findLine(lines, Math.max(flag, 0), "[LFDebug:IncidentGM]", "refused incident request");
  const action = findLine(lines, Math.max(list, 0), "[LFDebug:AdminAction]", "admin ");
  const row = findLine(lines, Math.max(action, 0), "[LFDebug:Incident]", "admin:");

  let admin = "ok";
  if (list < 0 && action < 0) {
    admin = "split";
    if (requireAdmin) {
      admin = "fail";
      fails.push(refused >= 0 ? "admin: the Game Master list refused the client (not an administrator)" : "admin: no incident list reached an administrator");
    }
  } else {
    if (list < 0) fails.push("admin: no Game Master list before the admin action");
    if (action < 0) fails.push("admin: no admin action after the list");
    else if (row < 0) fails.push("admin: the admin action left no incident row");
  }

  return { fails, admin };
}

export function selftestIncidentHandled() {
  const f = [];
  const v = "00bbbddd-5e55-0000-0000-000000000001";
  const a = "00bbbddd-5e55-0000-0000-000000000002";
  const hit = `[LFDebug:Incident] #4 damage: ${v} / ${a} amount 60 how x at 1 murder 0 zone=`;
  const flag = `[LFDebug:IncidentReport] report from ${v} flagged 1 incident(s)`;
  if (judgeIncident([hit, flag], v, a).fails.length !== 0) f.push("hit then flag should pass");
  if (judgeIncident([hit], v, a).fails.length !== 1) f.push("no report must fail at flag");
  if (!judgeIncident([flag, hit], v, a).fails.some((x) => x.startsWith("flag"))) f.push("a flag before the hit must fail");
  if (judgeIncident([hit, flag], v, a).admin !== "split") f.push("no admin client is split");
  if (judgeIncident([hit, flag], v, a, true).admin !== "fail") f.push("--require-admin must fail with no admin");
  const full = [hit, flag, "[LFDebug:IncidentGM] sent 2 incident row(s) to admin 1", `[LFDebug:AdminAction] admin x: !handled 4 -> `, "[LFDebug:Incident] #5 admin: x /  amount 0 how !handled 4"];
  const r = judgeIncident(full, v, a, true);
  if (r.fails.length || r.admin !== "ok") f.push(`full order should pass: ${r.fails.join("; ")}`);
  if (!judgeIncident([hit, flag, "[LFDebug:IncidentGM] sent 2 incident row(s) to admin 1"], v, a, true).fails.some((x) => x.startsWith("admin"))) f.push("a list with no admin action must fail");
  return f;
}

async function runIncidentHandledProof(args) {
  const skipReport = args.includes("--skip-report");
  const requireAdmin = args.includes("--require-admin");
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
  const serverLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n") : [];
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const id1 = Number(state(1).player);
  const id2 = Number(state(2).player);
  if (!id1 || !id2) {
    console.log("FAIL  start: a client has no player id");
    console.log("incident-handled proof: FAIL");
    return 1;
  }
  const victim = sessionUid(id1);
  const attacker = sessionUid(id2);
  const base = serverLines().length;

  // Leg 1: client 2 hits client 1 at the fuel station (outside the safe zone).
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const shot = on(2, { verb: "shoot", playerId: id1 }, 10000);
  wait(2500);
  const fails = [];
  if (shot.status !== "ok" || !shot.detail.includes("refused=0")) fails.push(`shot: ${shot.status} ${shot.detail}`);

  // Leg 2: client 1 files /report.
  if (!skipReport) {
    const report = on(1, { verb: "report" }, 10000);
    wait(2500);
    if (report.status !== "ok") fails.push(`report: ${report.status} ${report.detail}`);
  }

  // Leg 3: the administrator reads the list in Game Master and acts. A session client is only an
  // administrator when the server says so; ask once and let the log decide.
  const gm = on(2, { verb: "gm" }, 15000);
  wait(3000);
  on(2, { verb: "gm", arg: "close" }, 15000);
  if (gm.status !== "ok") console.log(`note  gm: ${gm.status} ${gm.detail}`);

  const verdict = judgeIncident(serverLines().slice(base), victim, attacker, requireAdmin);
  for (const f of [...fails, ...verdict.fails]) console.log(`FAIL  ${f}`);
  if (verdict.admin === "split") {
    console.log("SPLIT admin leg not driven: no client is an administrator and the harness has no chat or admin-grant verb; see tmp/handoff-slice-1451.md");
  }
  const bad = fails.length + verdict.fails.length;
  console.log(bad ? "incident-handled proof: FAIL" : `incident-handled proof: PASS (hit logged, /report flagged it${verdict.admin === "ok" ? ", admin listed it and acted" : "; admin leg SPLIT"})`);
  return bad ? 1 : 0;
}

export default {
  name: "incident-handled-proof",
  usage: "incident-handled-proof [--skip-report] [--require-admin]",
  run: (ctx) => runIncidentHandledProof(ctx.args ?? []),
};
