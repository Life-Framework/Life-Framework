import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir, sessionPaths } from "../session.mjs";

// #1566 (O6 restart toast, #1265, driven by the ticker #1348): a client that joins AFTER the first restart
// warning still gets the rest of them, and nobody gets a siren (the flagship is toast only).
//
//   node agent-box/session.mjs restart-late-proof --prepare [--in 18] [--leads 15,5,1]     (before `up`)
//   node agent-box/session.mjs up --clients 1
//   node agent-box/session.mjs restart-late-proof [--join-after first|last]
//   node agent-box/session.mjs down
//
// --prepare writes the server PROFILE's config.json "restarts" section through the B7 helper
// (tools/playtest/opening/b7-restart-setup.mjs: one test time N minutes ahead, never server/flagship/) and,
// with --leads, swaps the shipped 15,5,1 for shorter leads so the run is minutes, not a quarter hour. The
// server reads it at boot, so it goes in before `up`. The run puts the file back when it ends.
// The run: client 1 arrives; when the server logs the first warning client 2 joins late (`session join 2`)
// and arrives; then for every later lead the server logs the warning and client 2's own log must show a
// `toast received` within a few seconds ("<lead>-minute toast"). `--join-after last` is the red run: client 2
// joins only after the last warning, so the leads it was promised never reach it and it fails at the second
// lead's toast ("5-minute toast" on the shipped leads).
const HELPER = join(ROOT, "tools", "playtest", "opening", "b7-restart-setup.mjs");
const TOAST_LINE = /toast received: #LF-Ops_RestartToast/g;
const TOAST_WAIT_MS = 20000;
const GRACE_MS = 90000;

// "15,5,1" -> [15, 5, 1], largest first; null when it is not a list of positive whole minutes.
export function parseLeads(text) {
  const parts = String(text ?? "").split(",").map((s) => s.trim());
  if (!parts.length || parts.some((p) => !/^\d+$/.test(p) || Number(p) < 1)) return null;
  return [...new Set(parts.map(Number))].sort((a, b) => b - a);
}

// How many restart toasts a console log shows this client received.
export function countToasts(text) {
  return (String(text ?? "").match(TOAST_LINE) || []).length;
}

// Did the server publish the warning for this lead (the ticker's own line).
export function warningFired(serverText, lead) {
  return new RegExp(`restart warning: ${lead} minute\\(s\\)`).test(String(serverText ?? ""));
}

// Ops lines that mention a siren: the flagship sounds none, so any is a fail.
export function sirenLines(text) {
  return String(text ?? "").split("\n").filter((l) => /\[LFDebug:Ops\]/.test(l) && /siren/i.test(l));
}

function profileDir() {
  return join(sessionPaths(sessionDir()).server, "profile", "LifeFramework");
}

function readLogs(logsDir) {
  const out = [];
  if (!existsSync(logsDir)) return "";
  for (const d of readdirSync(logsDir).sort()) {
    const p = join(logsDir, d, "console.log");
    if (existsSync(p)) {
      try { out.push(readFileSync(p, "utf8")); } catch { /* being written */ }
    }
  }
  return out.join("\n");
}

function helper(args) {
  const r = spawnSync(process.execPath, [HELPER, ...args], { encoding: "utf8", windowsHide: true });
  const line = `${r.stdout || ""}${r.stderr || ""}`.trim();
  return { ok: r.status === 0, line };
}

function prepare(args) {
  const dir = profileDir();
  const cfg = join(dir, "config.json");
  const minutes = args.includes("--in") ? args[args.indexOf("--in") + 1] : "18";
  const leadsArg = args.includes("--leads") ? args[args.indexOf("--leads") + 1] : null;
  const leads = leadsArg === null ? null : parseLeads(leadsArg);
  if (leadsArg !== null && !leads) {
    console.log(`FAIL  --leads must be whole minutes like 15,5,1 (got ${leadsArg})`);
    return 1;
  }
  if (!existsSync(cfg)) {
    mkdirSync(dir, { recursive: true });
    writeFileSync(cfg, JSON.stringify({ schemaVersion: 1 }, null, 2) + "\n");
    writeFileSync(join(dir, ".restart-late-created"), "");
  }
  const r = helper(["--profile", dir, "--in", String(minutes)]);
  console.log(r.line);
  if (!r.ok) return 1;
  if (leads) {
    const config = JSON.parse(readFileSync(cfg, "utf8"));
    config.restarts = { ...config.restarts, warningMinutes: leads };
    writeFileSync(cfg, JSON.stringify(config, null, 2) + "\n");
    console.log(`warning leads: ${leads.join(", ")} minute(s)`);
  }
  // The server may boot on another pool machine, whose profile is not this one: `up --seed` copies the
  // same file into the server profile's LifeFramework/ on whichever machine boots it.
  const seed = join(ROOT, "tmp", "restart-late-seed");
  mkdirSync(seed, { recursive: true });
  writeFileSync(join(seed, "config.json"), readFileSync(cfg, "utf8"));
  console.log(`prepared ${cfg}; now \`session up --clients 1 --seed ${join(seed, "config.json")}\` and run the proof within ${minutes} minutes`);
  return 0;
}

// Puts the profile config back the way it was before --prepare.
function restore() {
  const dir = profileDir();
  const marker = join(dir, ".restart-late-created");
  const cfg = join(dir, "config.json");
  const r = helper(["--profile", dir, "--undo"]);
  console.log(r.line);
  if (existsSync(marker)) {
    rmSync(cfg, { force: true });
    rmSync(marker, { force: true });
  } else if (!r.ok && existsSync(cfg)) {
    // A seeded profile has no backup: the schedule still must not outlive the run (it repeats daily).
    try {
      const config = JSON.parse(readFileSync(cfg, "utf8"));
      delete config.restarts;
      writeFileSync(cfg, JSON.stringify(config, null, 2) + "\n");
      console.log(`restart schedule removed from ${cfg}`);
    } catch { /* unreadable: leave it */ }
  }
}

function secondsUntil(time) {
  const [h, m] = String(time).split(":").map(Number);
  const t = new Date();
  t.setHours(h, m, 0, 0);
  if (t.getTime() < Date.now()) t.setDate(t.getDate() + 1);
  return (t.getTime() - Date.now()) / 1000;
}

async function run(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const paths = sessionPaths(dir);
  const joinAfter = args.includes("--join-after") ? args[args.indexOf("--join-after") + 1] : "first";
  if (joinAfter !== "first" && joinAfter !== "last") {
    console.log(`FAIL  --join-after is first or last (got ${joinAfter})`);
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverText = () => readLogs(join(paths.server, "logs"));
  const clientText = (n) => readLogs(paths.clients[n - 1].logs);
  const until = (what, ms, test) => {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      if (test()) return true;
      wait(2000);
    }
    return test();
  };
  const fails = [];
  const check = (name, ok, detail) => {
    console.log(`${ok ? "ok   " : "FAIL "} ${name}${detail ? `: ${detail}` : ""}`);
    if (!ok) fails.push(name);
    return ok;
  };

  let restarts;
  try {
    restarts = JSON.parse(readFileSync(join(profileDir(), "config.json"), "utf8")).restarts;
  } catch { restarts = null; }
  const leads = parseLeads((restarts?.warningMinutes ?? []).join(","));
  const time = restarts?.times?.[0];
  if (!leads || leads.length < 2 || !time) {
    console.log("FAIL  the profile config.json has no restart schedule with two warnings; run --prepare before `up`");
    return 1;
  }
  console.log(`restart at ${time}, warnings ${leads.join(", ")} minute(s) before, ${Math.round(secondsUntil(time))} s from now`);

  try {
    check("warner ticking", /restart warner: ticking once a second/.test(serverText()), "server log line from LF_RestartWarner");
    arrive(on, 1, wait);

    // Client 1 is there for every warning: the baseline that the toast itself works.
    const first = leads[0];
    const firstWait = secondsUntil(time) - first * 60;
    const fired = until("first warning", Math.max(60000, (firstWait + GRACE_MS / 1000) * 1000), () => warningFired(serverText(), first));
    if (!check(`${first}-minute warning fired`, fired, "server log")) return report(fails);
    check(`${first}-minute toast on client 1`, until("toast", TOAST_WAIT_MS, () => countToasts(clientText(1)) >= 1), `${countToasts(clientText(1))} toast line(s)`);

    // Late joiner: after the first warning (green) or after the last one (the red run).
    const joinLead = joinAfter === "last" ? leads[leads.length - 1] : first;
    if (joinAfter === "last") {
      const lastWait = secondsUntil(time) - joinLead * 60;
      if (!until("last warning", Math.max(60000, (lastWait + GRACE_MS / 1000) * 1000), () => warningFired(serverText(), joinLead))) {
        check(`${joinLead}-minute warning fired`, false, "server log");
        return report(fails);
      }
    }
    console.log(`client 2 joins after the ${joinLead}-minute warning (${Math.round(secondsUntil(time))} s before the restart)`);
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
    if (!check("client 2 joined late", joined.status === 0, `session join 2 exited ${joined.status}`)) return report(fails);
    arrive(on, 2, wait);
    const base = countToasts(clientText(2));
    console.log(`client 2 has ${base} restart toast(s) at join`);

    // Every lead it was promised: the server fires it, client 2's own log must show the toast.
    let got = base;
    for (const lead of leads.slice(1)) {
      const wanted = secondsUntil(time) - lead * 60;
      const serverFired = until(`${lead}-minute warning`, Math.max(60000, (wanted + GRACE_MS / 1000) * 1000), () => warningFired(serverText(), lead));
      if (!check(`${lead}-minute warning fired`, serverFired, "server log")) break;
      const arrived = until(`${lead}-minute toast`, TOAST_WAIT_MS, () => countToasts(clientText(2)) > got);
      check(`${lead}-minute toast`, arrived, `client 2 toast lines ${got} -> ${countToasts(clientText(2))}`);
      if (!arrived) break;
      got = countToasts(clientText(2));
    }

    // The flagship is toast only: no Ops siren line on the server or either client.
    const sirens = [...sirenLines(serverText()), ...sirenLines(clientText(1)), ...sirenLines(clientText(2))];
    check("no siren", sirens.length === 0, sirens.length ? sirens[0] : "no Ops siren line anywhere");
  } finally {
    restore();
  }
  return report(fails);
}

function report(fails) {
  console.log(fails.length ? `restart late proof: FAIL (${fails.join("; ")})` : "restart late proof: PASS (a client that joined after the first restart warning got every later toast; no siren)");
  return fails.length ? 1 : 0;
}

// The pure judges, no game: `restart-late-proof --selftest`.
function selftest() {
  const bad = [];
  const eq = (name, a, b) => { if (JSON.stringify(a) !== JSON.stringify(b)) bad.push(`${name}: ${JSON.stringify(a)} != ${JSON.stringify(b)}`); };
  eq("leads sorted", parseLeads("1,15,5"), [15, 5, 1]);
  eq("leads dedupe", parseLeads("5,5,1"), [5, 1]);
  eq("leads bad", parseLeads("5,x"), null);
  eq("leads zero", parseLeads("0,5"), null);
  const log = "[LFDebug:Notify] toast received: #LF-Ops_RestartToast\nnoise\n[LFDebug:Notify] toast received: #LF-Ops_RestartToast\n[LFDebug:Notify] toast received: #LF-Other";
  eq("toasts counted", countToasts(log), 2);
  eq("toasts none", countToasts(""), 0);
  eq("warning fired", warningFired("[LFDebug:Ops] restart warning: 5 minute(s) to the restart at 12:00:00 (300 s)", 5), true);
  eq("warning other lead", warningFired("[LFDebug:Ops] restart warning: 15 minute(s) to the restart", 5), false);
  eq("siren found", sirenLines("[LFDebug:Ops] restart siren on\n[LFDebug:Notify] siren").length, 1);
  eq("siren none", sirenLines("[LFDebug:Ops] restart warning: 5 minute(s)").length, 0);
  for (const b of bad) console.log(`FAIL  ${b}`);
  console.log(bad.length ? "restart late proof selftest: FAIL" : "restart late proof selftest: ok");
  return bad.length ? 1 : 0;
}

export default {
  name: "restart-late-proof",
  usage: "restart-late-proof --prepare [--in N] [--leads 15,5,1]   (before up) | restart-late-proof [--join-after first|last]   (up --clients 1) | --selftest",
  run: (ctx) => {
    const args = ctx.args ?? [];
    if (args.includes("--selftest")) return selftest();
    if (args.includes("--prepare")) return prepare(args);
    return run(args);
  },
};
