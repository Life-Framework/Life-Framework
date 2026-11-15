// Attendance from a server console.log (`tools\cli host attendance`, #1459): who joined, who left,
// the peak concurrent count and when, and the minutes each player stayed. Pure text in, plain object
// out, so `host history` (#1409) and `host gate` (#1410) can read the same numbers from --json.
//
// Engine lines this reads (real shapes, copied from a server log):
//   NETWORK : ### Connecting player: connectionID=0, Name=
//   NETWORK : ### Creating player: PlayerId=1, Name=Kasper          <- a join
//   NETWORK : ### Updating player: PlayerId=1, Name=Kasper, rplIdentity=0x..., IdentityId=<uuid>
//   NETWORK : Player disconnected: connectionID=0                   <- a leave
// A player is one IdentityId (all-zero or missing: the display name), so a rejoin, even under a new
// name, is the same player. Concurrent players are the sessions open at that point in the log.
// The log has no date: times are HH:MM:SS and a clock that runs backwards is a new day.

// A time stamp starts a log line. Two writers can glue one line onto another mid-line, so a line is
// cut at every stamp and each piece is read on its own (a stamp is never preceded by a digit or colon).
const STAMP = /(?<![\d:])(\d{2}):(\d{2}):(\d{2})\.(\d{3})(?!\d)/g;
const CONNECTING = /### Connecting player: connectionID=(\d+)/;
const CREATING = /### Creating player: PlayerId=(\d+), Name=(.*)$/;
const UPDATING = /### Updating player: PlayerId=(\d+),.*?IdentityId=([0-9a-fA-F-]+)/;
const DISCONNECTED = /Player disconnected: connectionID=(\d+)/;
const NO_IDENTITY = /^[0-]*$/;

function clock(h, m, s) {
  return `${String(h).padStart(2, "0")}:${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
}

export function parseAttendance(text) {
  const sessions = []; // { playerId, name, identity, start, end|null }
  const open = new Map(); // PlayerId -> session
  const connToPlayer = new Map(); // connectionID -> PlayerId
  let pendingConn = null;
  let day = 0;
  let lastSec = -1;
  let now = 0; // seconds since the first day's midnight, as the log has run
  let firstSec = null;
  let peak = null;
  let leaves = 0;
  let unmatchedLeaves = 0;
  let joins = 0;

  const stampClock = (sec) => clock(Math.floor((sec % 86400) / 3600), Math.floor((sec % 3600) / 60), Math.floor(sec % 60));
  const closeSession = (s, at) => {
    s.end = at;
    open.delete(s.playerId);
  };
  const noteCount = () => {
    if (open.size > (peak?.count ?? 0)) peak = { count: open.size, atSeconds: now };
  };

  for (const line of text.split(/\r?\n/)) {
    const cuts = [...line.matchAll(STAMP)];
    // Text before the first stamp (or a line with none) belongs to the last stamp seen.
    const pieces = [];
    if (!cuts.length) pieces.push({ at: null, body: line });
    else {
      if (cuts[0].index > 0) pieces.push({ at: null, body: line.slice(0, cuts[0].index) });
      cuts.forEach((c, i) => pieces.push({ at: c, body: line.slice(c.index, i + 1 < cuts.length ? cuts[i + 1].index : undefined) }));
    }
    for (const { at, body } of pieces) {
      if (at) {
        const sec = Number(at[1]) * 3600 + Number(at[2]) * 60 + Number(at[3]) + Number(at[4]) / 1000;
        if (lastSec >= 0 && sec < lastSec - 1) day++; // allow a second of jitter between writers
        lastSec = sec;
        now = day * 86400 + sec;
        if (firstSec === null) firstSec = now;
      }
      let m;
      if ((m = CONNECTING.exec(body))) {
        pendingConn = Number(m[1]);
      } else if ((m = CREATING.exec(body))) {
        const playerId = Number(m[1]);
        const prior = open.get(playerId);
        if (prior) closeSession(prior, now); // the leave line never reached the log
        const s = { playerId, name: m[2].trim(), identity: null, start: now, end: null };
        sessions.push(s);
        open.set(playerId, s);
        if (pendingConn !== null) connToPlayer.set(pendingConn, playerId);
        pendingConn = null;
        joins++;
        noteCount();
      } else if ((m = UPDATING.exec(body))) {
        const s = open.get(Number(m[1]));
        if (s && !NO_IDENTITY.test(m[2])) s.identity = m[2].toLowerCase();
      } else if ((m = DISCONNECTED.exec(body))) {
        const conn = Number(m[1]);
        const playerId = connToPlayer.get(conn);
        const s = playerId === undefined ? undefined : open.get(playerId);
        connToPlayer.delete(conn);
        if (s) {
          closeSession(s, now);
          leaves++;
        } else {
          unmatchedLeaves++;
        }
      }
    }
  }

  const logEnd = now;
  const stillConnected = open.size;
  const byKey = new Map();
  for (const s of sessions) {
    const key = s.identity ?? `name:${s.name.toLowerCase()}`;
    let p = byKey.get(key);
    if (!p) {
      p = { name: s.name, names: [], identity: s.identity, sessions: 0, seconds: 0 };
      byKey.set(key, p);
    }
    if (!p.names.includes(s.name)) p.names.push(s.name);
    p.name = s.name;
    p.sessions++;
    p.seconds += (s.end ?? logEnd) - s.start;
  }
  const players = [...byKey.values()].map((p) => ({
    name: p.names[0],
    names: p.names,
    identity: p.identity,
    sessions: p.sessions,
    minutes: Math.round((p.seconds / 60) * 10) / 10,
  }));
  return {
    unique: players.length,
    joins,
    rejoins: joins - players.length,
    leaves,
    unmatchedLeaves,
    stillConnected,
    peak: peak ? { count: peak.count, at: stampClock(peak.atSeconds), atSeconds: Math.round(peak.atSeconds) } : null,
    logStart: firstSec === null ? null : stampClock(firstSec),
    logEnd: firstSec === null ? null : stampClock(logEnd),
    players,
  };
}
