// tools/town/ready.mjs - `tools\cli town ready <state>`: is a map state playable (docs/architecture/everon-world.md 3.5, 3.6; card #2207).
//
//   town ready <state> [--no-session] [--session-up] [--json]
//
// For every place of the state:
//   1. `town check <place>` passes (the static checks: slots mapped, layers, claims, kinds, footprints, budget);
//   2. the reach proof passes: `session town-reach --place <place>` walks a character from the arrival slot's entrance to every
//      other slot's entrance on the island navmesh (agent-box/proofs/town-reach.mjs, LF_TownReachHarness.c);
//   3. the server boot log has no "switches off" line (a system whose places are not on the map, LF_Systems.CheckPlaces).
// It needs a running `session up --mission island` for 2 and 3 (one boot, shared: sessions are RAM-heavy); `--session-up` boots
// one and takes it down after, `--no-session` runs only the static half and says the rest was not run (exit 1: not ready).
// Exit 0 only when every place passes every step.
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const help = "ready <state> [--session-up] [--no-session]   every place of the state: town check, the reach proof on the island, no 'switches off' in the boot log; exit 1 unless all pass";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

// The places of each map state (everon-world.md 3.6 "Phased map states"). A place is listed when its state's chapters need it.
export const STATE_PLACES = {
  0: ["saint-philippe"],
  1: ["saint-philippe", "meaux"],
};

// Pure: the "switches off" lines of a console log, [] when none.
export function switchOffLines(logText) {
  return String(logText).split(/\r?\n/).filter((l) => /switch(es|ed)? off/i.test(l) && /Systems/.test(l));
}

// Pure: the one TOWNREACH json of a proof's stdout, or null.
export function parseReach(stdout) {
  const m = /^TOWNREACH (\{.*\})\s*$/m.exec(String(stdout));
  if (!m) return null;
  try { return JSON.parse(m[1]); } catch { return null; }
}

// Pure: the verdict of a state from its per-place results ({ place, check, reach, switchOff }); each is true, false or null (not run).
export function verdict(places) {
  const problems = [];
  for (const p of places) {
    if (p.check !== true) problems.push(`${p.place}: ${p.check === null ? "town check was not run" : "town check failed"}`);
    if (p.reach !== true) problems.push(`${p.place}: ${p.reach === null ? "the reach proof was not run" : "the reach proof failed"}`);
    if (p.switchOff !== true) problems.push(`${p.place}: ${p.switchOff === null ? "the boot log was not read" : "a system switched off at boot"}`);
  }
  return problems;
}

function cli(args, root, opts = {}) {
  return spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), ...args], { cwd: root, encoding: "utf8", timeout: opts.timeout ?? 600000, maxBuffer: 64 * 1024 * 1024 });
}

export async function main(args, { root = ROOT } = {}) {
  const state = args.find((a) => /^\d+$/.test(a));
  if (state === undefined || !STATE_PLACES[state]) {
    console.log(`usage: tools\\cli town ready <state>   (states: ${Object.keys(STATE_PLACES).join(", ")})`);
    return 1;
  }
  const noSession = args.includes("--no-session");
  const sessionUp = args.includes("--session-up");
  const places = STATE_PLACES[state].map((place) => ({ place, check: null, reach: null, switchOff: null, notes: [] }));

  for (const p of places) {
    const dir = join(root, "docs", "everon", "places", p.place);
    if (!existsSync(join(dir, "mapping.json"))) {
      p.check = false;
      p.notes.push(`no approved mapping (tools\\cli town map ${p.place})`);
      continue;
    }
    const r = cli(["town", "check", p.place], root);
    p.check = r.status === 0;
    if (!p.check) p.notes.push(((r.stdout ?? "") + (r.stderr ?? "")).split(/\r?\n/).filter((l) => /ERROR|FAIL/.test(l)).slice(0, 5).join("; ") || `town check exit ${r.status}`);
  }

  if (!noSession && places.some((p) => p.check)) {
    if (sessionUp) {
      const up = cli(["session", "up", "--mission", "island", "--wait"], root, { timeout: 900000 });
      if (up.status !== 0) for (const p of places) p.notes.push("session up --mission island failed: " + ((up.stdout ?? "") + (up.stderr ?? "")).trim().split(/\r?\n/).slice(-2).join(" | "));
    }
    for (const p of places.filter((x) => x.check)) {
      const r = cli(["session", "town-reach", "--place", p.place, "--json"], root);
      const out = parseReach(r.stdout);
      p.reach = out ? out.pass === true : false;
      if (!out || !out.pass) p.notes.push(out ? out.results.filter((x) => !x.ok).map((x) => `${x.slot}: ${x.detail}`).join("; ") : "the proof printed no TOWNREACH line (is a --mission island session up?)");
    }
    if (places.some((p) => p.reach !== null)) {
      const logs = cli(["logs", "switches off"], root, { timeout: 60000 });
      const lines = switchOffLines(logs.stdout ?? "");
      for (const p of places.filter((x) => x.reach !== null)) {
        p.switchOff = lines.length === 0;
        if (lines.length) p.notes.push(`switched off: ${lines.slice(0, 3).join(" | ")}`);
      }
    }
    if (sessionUp) cli(["session", "down"], root, { timeout: 120000 });
  }

  const problems = verdict(places);
  for (const p of places) console.log(`${p.place}: check ${p.check === null ? "n/a" : p.check ? "ok" : "FAIL"}, reach ${p.reach === null ? "n/a" : p.reach ? "ok" : "FAIL"}, boot log ${p.switchOff === null ? "n/a" : p.switchOff ? "ok" : "FAIL"}${p.notes.length ? "  (" + p.notes.join("; ") + ")" : ""}`);
  console.log(problems.length ? `SUMMARY town ready ${state} fail: ${problems.length} problem(s)` : `SUMMARY town ready ${state} ok: ${places.length} place(s)`);
  for (const q of problems) console.log(`  ${q}`);
  return problems.length ? 1 : 0;
}
