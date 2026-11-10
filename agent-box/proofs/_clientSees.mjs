// What a CLIENT sees, not what the server holds (#522). A proof that judges harness replies from
// the acting client or the server log passes while the other player sees nothing (the #512 revive
// animation). These helpers read the OTHER client: its replicated peer bodies (`peers`), the
// actions its own CanBeShownScript offers (`look`, driven by replicated state), the text on its
// screen (`ui text`), and its own console log. The pure judges are covered by `session.mjs selftest`.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, newestLog, parsePeer, sessionPaths } from "../session.mjs";

// Newest console log of the server and of clients 1..n, as full text.
function readLog(logsDir) {
  const f = newestLog(logsDir);
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}
export function readLogs(dir, clients = 2) {
  const paths = sessionPaths(dir, clients);
  return [readLog(join(paths.server, "logs")) || readLog(paths.server), ...paths.clients.map((c) => readLog(c.logs))];
}
// [server, client1, ...] lengths now; pass to logsSince later so a repeat run judges only its lines.
export function logMark(dir, clients = 2) {
  return readLogs(dir, clients).map((t) => t.length);
}
export function logsSince(dir, mark, clients = 2) {
  return readLogs(dir, clients).map((t, i) => t.slice(mark[i] ?? 0));
}

// "LF_LockpickAction@Roadmap_Lockpick_Door,SCR_PickUpItemAction" -> ["LF_LockpickAction@...", ...]
export function parseActions(look) {
  const s = String(look ?? "").trim();
  return !s || s === "none" ? [] : s.split(",");
}
// Does the actions line offer `className` (optionally on `entityName`)?
export function offers(look, className, entityName = "") {
  return parseActions(look).some((a) => a.startsWith(className) && (!entityName || a.endsWith(`@${entityName}`)));
}
// Metres between two peers as seen on `peersLine` (null when either is missing).
export function peerGap(peersLine, idA, idB) {
  const a = parsePeer(peersLine, idA);
  const b = parsePeer(peersLine, idB);
  return a && b ? Math.hypot(a.at[0] - b.at[0], a.at[2] - b.at[2]) : null;
}
// Distance moved between two peer reads, null when either is missing.
export function peerMoved(peersBefore, peersAfter, id) {
  const a = parsePeer(peersBefore, id);
  const b = parsePeer(peersAfter, id);
  return a && b ? Math.hypot(a.at[0] - b.at[0], a.at[1] - b.at[1], a.at[2] - b.at[2]) : null;
}

export function selftestClientSees() {
  const f = [];
  const peers = "player=1 downed=0 life=ALIVE carrier=0 seat=1 at=10,0,10 | player=2 downed=1 life=INCAPACITATED carrier=1 seat=0 at=13,0,14";
  if (peerGap(peers, 1, 2) !== 5) f.push("peerGap");
  if (peerGap(peers, 1, 9) !== null) f.push("peerGap missing peer");
  if (peerMoved(peers, peers.replace("at=13,0,14", "at=13,0,16"), 2) !== 2) f.push("peerMoved");
  if (!offers("LF_LockpickAction@Door,SCR_X", "LF_LockpickAction", "Door")) f.push("offers hit");
  if (offers("LF_LockpickAction@Door", "LF_LockpickAction", "Other")) f.push("offers wrong entity");
  if (offers("none", "LF_LockpickAction")) f.push("offers on none");
  return f;
}

// Builds the helper for one proof run. `on(n, spec, timeoutMs)` and `wait(ms)` are the proof's own.
export function makeClientSees({ on, wait, dir }) {
  const peers = (n) => on(n, { verb: "peers" }, 10000).detail;
  // `filter` keeps only action classes containing it (the harness `look` caps at 12 actions, and a vehicle fills them).
  const look = (n, filter = "") => on(n, { verb: "look", filter }).detail;
  // `look` lists at most 12 actions: around a big target, stand on each side and join the lists.
  const lookAround = (n, entityName, filter = "") => {
    const all = [];
    for (const side of ["e", "w", "n", "s"]) {
      on(n, { verb: "stand", entityName, side });
      wait(1200);
      all.push(...parseActions(look(n, filter)).map((a) => `${side}:${a}`));
    }
    return all;
  };
  const uiText = (n) => on(n, { verb: "ui", mode: "text" }, 10000).detail;
  return {
    peers,
    look,
    lookAround,
    uiText,
    // Client n must see `className` offered (or not offered) within 4 m of its body.
    expectOffer(n, className, entityName, want, fails, label) {
      const got = offers(look(n, className), className, entityName);
      if (got !== want) fails.push(`${label}: client ${n} ${want ? "does not see" : "still sees"} ${className}${entityName ? `@${entityName}` : ""}`);
      return got;
    },
    // Client n must show `needle` somewhere on screen.
    expectText(n, needle, fails, label) {
      const t = uiText(n);
      if (!t.includes(needle)) fails.push(`${label}: client ${n} screen has no '${needle}': ${t.slice(0, 160)}`);
      return t;
    },
    // Client n's own console log must contain `needle` after `mark`.
    expectLog(n, mark, needle, fails, label) {
      const text = logsSince(dir, mark, Math.max(2, n))[n] ?? "";
      if (!text.includes(needle)) fails.push(`${label}: client ${n} never logged '${needle}'`);
      return text.includes(needle);
    },
  };
}

// #504: client n is already running (an earlier run in this session): reuse it. Otherwise join it
// late. Returns "reused", "joined" or "failed".
export function ensureClient(on, wait, n) {
  const live = on(n, { verb: "state" }, 8000);
  if (live.status === "ok") return "reused";
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", String(n)], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) return "failed";
  arrive(on, n, wait);
  wait(8000);
  return "joined";
}
