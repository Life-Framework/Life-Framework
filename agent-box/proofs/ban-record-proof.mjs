import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, fixtureDir, newestLog, parseState, readSession, sessionDir, sessionPaths, sessionUid } from "../session.mjs";

// M4 #1571 (#315's acceptance, the ban record #1285), on
//   up --clients 2 --mission playtest --admin-client 1
// Client 2 raises an incident (client 1 is shot at the fuel station), administrator client 1 bans client 2
// with a reason through the real chat path (`chat #ban create <id> <seconds> <reason>`, #1539), then asks
// `#banrecord <uid>`, and client 2 tries to rejoin. The verdict is the server log and the ban record file:
//   1. the incident log has a row for client 2 (an incident number is minted);
//   2. the server logs `[LFDebug:BanRecord] recorded V1|BAN|<uid 2>|...` and the file holds that row with
//      the reason, the staff uid (client 1's account) and the duration;
//   3. "incident in the row": the row names the incident number raised in step 1;
//   4. the server answered the `#banrecord` ask (the admin chat line);
//   5. client 2 is gone from the server after the ban, and a new client 2 gets no join (no spawn, quiz or
//      registration form) the server would log for a returning account.
//
//   ban-record-proof [--skip-incident]
//   --skip-incident  the red run: client 2 does nothing wrong, so the proof fails at "incident in the row"

const REASON = "ban-record proof: shot a player at the fuel station";
const SECONDS = 3600;

const JOIN_MARKER = /(registration form sent to player|spawning player|quiz sent to player) (\d+)/;

// The incident numbers the log minted for a uid after the baseline: `[LFDebug:Incident] #4 damage: <v> / <a> ...`.
// Pure. Returns the numbers, oldest first.
export function incidentNumbersFor(lines, uid) {
  const out = [];
  for (const line of lines) {
    const m = line.match(/\[LFDebug:Incident\] #(\d+) \w+: (\S*) \/ (\S*)/);
    if (m && (m[2] === uid || m[3] === uid)) out.push(Number(m[1]));
  }
  return out;
}

// One ban record row (LF_BanRecord.FormatRow): V1|ACTION|target|unix|staff|seconds|ids|reason. Pure; null when malformed.
export function parseRow(line) {
  const p = String(line).split("|");
  if (p.length !== 8 || p[0] !== "V1") return null;
  return { action: p[1], target: p[2], time: Number(p[3]), staff: p[4], seconds: Number(p[5]), ids: p[6].split(",").filter(Boolean).map(Number), reason: p[7] };
}

// The ordered judgement. `raised` is the incident numbers this run minted for client 2, `rows` the ban record
// lines written after the baseline, `logLines` the server log lines after the baseline. Pure.
// Returns the failures; each starts with the name of the check.
export function judgeBan({ raised, rows, logLines, uid2, uid1, reason, seconds }) {
  const fails = [];
  if (!raised.length) fails.push("incident: the incident log minted no row for client 2");

  const mine = rows.map(parseRow).filter((r) => r && r.target === uid2);
  const ban = mine.find((r) => r.action === "BAN");
  const logged = logLines.some((l) => l.includes("[LFDebug:BanRecord] recorded V1|BAN|" + uid2 + "|"));
  if (!ban) {
    fails.push(`row: no BAN row for ${uid2} in the ban record (${rows.length} new line(s))`);
  } else {
    if (!logged) fails.push("row: the server did not log '[LFDebug:BanRecord] recorded' for the ban");
    if (ban.staff !== uid1) fails.push(`staff uid in the row: ${ban.staff}, expected ${uid1}`);
    if (ban.reason !== reason) fails.push(`reason in the row: '${ban.reason}', expected '${reason}'`);
    if (ban.seconds !== seconds) fails.push(`duration in the row: ${ban.seconds}, expected ${seconds}`);
    if (!ban.ids.some((id) => raised.includes(id))) fails.push(`incident in the row: the row names [${ban.ids.join(",")}], none of the incident(s) [${raised.join(",")}] client 2 raised`);
  }
  return fails;
}

export function selftestBanRecord() {
  const f = [];
  const u1 = "00bbbddd-5e55-0000-0000-000000000001";
  const u2 = "00bbbddd-5e55-0000-0000-000000000002";
  const hit = `[LFDebug:Incident] #4 damage: ${u1} / ${u2} amount 60 how x at 1 murder 0 zone=`;
  if (incidentNumbersFor([hit, "[LFDebug:Incident] #5 cash: " + u1 + " /  amount 5"], u2).join() !== "4") f.push("incidentNumbersFor");
  const row = (ids) => `V1|BAN|${u2}|1700000000|${u1}|${SECONDS}|${ids}|${REASON}`;
  const log = (ids) => [`[LFDebug:BanRecord] recorded ${row(ids)}`];
  const base = { uid1: u1, uid2: u2, reason: REASON, seconds: SECONDS };
  if (judgeBan({ ...base, raised: [4], rows: [row("3,4")], logLines: log("3,4") }).length) f.push("a row naming the raised incident must pass");
  if (!judgeBan({ ...base, raised: [], rows: [row("")], logLines: log("") }).some((x) => x.startsWith("incident:"))) f.push("no incident raised must fail");
  if (!judgeBan({ ...base, raised: [4], rows: [row("2")], logLines: log("2") }).some((x) => x.startsWith("incident in the row"))) f.push("a row without the raised incident must fail at 'incident in the row'");
  if (!judgeBan({ ...base, raised: [4], rows: [], logLines: [] }).some((x) => x.startsWith("row:"))) f.push("no row must fail");
  if (!judgeBan({ ...base, raised: [4], rows: [row("4").replace(u1, "x")], logLines: log("4") }).some((x) => x.startsWith("staff"))) f.push("a wrong staff uid must fail");
  if (parseRow("garbage") !== null || parseRow(`V2|BAN|a|1|b|2|3|c`) !== null) f.push("parseRow accepts a bad line");
  return f;
}

async function runBanRecordProof(args) {
  const skipIncident = args.includes("--skip-incident");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n") : [];
  };
  const recordFile = join(fixtureDir(sessionPaths(dir).server), "ban_record.txt");
  const recordLines = () => (existsSync(recordFile) ? readFileSync(recordFile, "utf8").split("\n").filter((l) => l.trim()) : []);
  // Poll a predicate for up to `ms`; true when it held.
  const until = (ms, pred) => {
    for (let t = 0; t < ms; t += 1500) {
      if (pred()) return true;
      wait(1500);
    }
    return pred();
  };
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const id1 = Number(parseState(on(1, { verb: "state" }, 10000).detail).player);
  const id2 = Number(parseState(on(2, { verb: "state" }, 10000).detail).player);
  if (!id1 || !id2) {
    console.log("FAIL  start: a client has no player id");
    console.log("ban record proof: FAIL");
    return 1;
  }
  const idents = readSession(dir)?.clients ?? [];
  const uid1 = idents.find((c) => c.n === 1)?.identity?.uid ?? sessionUid(id1);
  const uid2 = idents.find((c) => c.n === 2)?.identity?.uid ?? sessionUid(id2);
  const baseLog = serverLines().length;
  const baseRecord = recordLines().length;

  // Leg 1: client 2 raises an incident: it shoots client 1 at the fuel station (outside the safe zone).
  if (!skipIncident) {
    on(1, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(2, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    const shot = on(2, { verb: "shoot", playerId: id1 }, 10000);
    wait(2500);
    if (shot.status !== "ok" || !shot.detail.includes("refused=0")) fails.push(`shot: ${shot.status} ${shot.detail}`);
  }
  const raised = incidentNumbersFor(serverLines().slice(baseLog), uid2);
  console.log(`incidents raised for ${uid2}: ${raised.length ? raised.join(",") : "none"}`);

  // Leg 2: the administrator bans client 2 through the real chat path (an engine `#` command is plain Global text).
  const ban = on(1, { verb: "chat", text: `#ban create ${id2} ${SECONDS} ${REASON}` }, 15000);
  if (ban.status !== "ok") fails.push(`chat #ban: ${ban.status} ${ban.detail}`);
  const wrote = until(25000, () => recordLines().length > baseRecord);
  if (!wrote) console.log("note  no new line in the ban record after 25 s (the backend may not have answered the ban)");

  // Leg 3: the administrator asks for the record.
  const ask = on(1, { verb: "chat", text: `#banrecord ${uid2}` }, 15000);
  if (ask.status !== "ok") fails.push(`chat #banrecord: ${ask.status} ${ask.detail}`);
  wait(2500);

  const rows = recordLines().slice(baseRecord);
  const lines = serverLines().slice(baseLog);
  fails.push(...judgeBan({ raised, rows, logLines: lines, uid2, uid1, reason: REASON, seconds: SECONDS }));
  for (const r of rows) console.log(`ban record: ${r}`);

  // Why a ban may leave no row in this session (first run 2026-11-15, see tmp/handoff-slice-1571.md):
  //  - the engine's chat permission for #ban and #banrecord is ADMINISTRATOR, which a script cannot grant; the harness
  //    gives SESSION_ADMINISTRATOR (LF_HarnessStaff.GrantAdminIfListed), so the line never reaches BanCommand
  //    (no `[LFDebug:AdminAction] admin <uid>: #ban` line);
  //  - `up` boots -noBackend ("Backend Services are disabled"), and vanilla BanCommand needs the backend's ban
  //    service (GetBanServiceApi), so it answers ERR: no row, no kick, no refused rejoin.
  const blocked = [];
  const wasRun = lines.some((l) => l.includes(`[LFDebug:AdminAction] admin ${uid1}: #ban`));
  const noBackend = serverLines().some((l) => l.includes("Backend Services are disabled"));
  if (!rows.length && !wasRun) blocked.push("blocked: the #ban never reached BanCommand (no '[LFDebug:AdminAction] admin <uid>: #ban' line); the engine wants the ADMINISTRATOR chat role and the harness can only grant SESSION_ADMINISTRATOR");
  if (!rows.length && noBackend) blocked.push("blocked: the server runs -noBackend ('Backend Services are disabled'), so vanilla BanCommand has no ban service: it answers ERR and neither records, kicks nor refuses a rejoin");

  // Leg 4: client 2 cannot rejoin. A banned client is kicked; the proof then closes whatever is left of it and
  // starts it again, and the server must not log a join (spawn, quiz or registration form) for it. Skipped when
  // no ban took effect: a client nothing kicked would rejoin and say nothing about the ban.
  if (rows.length) {
    const kicked = until(15000, () => serverLines().slice(baseLog).some((l) => new RegExp(`session slot \\d+ released: player ${id2} left`).test(l)));
    if (!kicked) fails.push(`kick: the server never logged client 2 (player ${id2}) leaving after the ban`);
    const pid = readSession(dir)?.clients?.find((c) => c.n === 2)?.pid;
    if (pid) spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"], { windowsHide: true });
    wait(3000);
    const joinsBefore = serverLines().filter((l) => JOIN_MARKER.test(l)).length;
    const rejoin = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
    process.stdout.write(rejoin.stdout ?? "");
    const newJoins = serverLines().filter((l) => JOIN_MARKER.test(l)).length - joinsBefore;
    console.log(`rejoin: session join 2 exited ${rejoin.status}; the server logged ${newJoins} new join line(s)`);
    if (rejoin.status === 0 || newJoins > 0) fails.push(`rejoin: client 2 rejoined after the ban (join exit ${rejoin.status}, ${newJoins} join line(s) logged)`);
  } else {
    console.log("rejoin: not attempted (no ban took effect)");
  }

  for (const f of [...fails, ...blocked]) console.log(`FAIL  ${f}`);
  if (blocked.length) console.log("ban record proof: BLOCKED (the harness cannot ban yet; the checks above that ran are real, see tmp/handoff-slice-1571.md)");
  else console.log(fails.length ? "ban record proof: FAIL" : "ban record proof: PASS (incident raised, banned with a reason, the row names staff, reason and incident, client 2 could not rejoin)");
  return fails.length || blocked.length ? 1 : 0;
}

export default {
  name: "ban-record-proof",
  usage: "ban-record-proof [--skip-incident]   (session up --clients 2 --mission playtest --admin-client 1)",
  run: (ctx) => runBanRecordProof(ctx.args ?? []),
};
