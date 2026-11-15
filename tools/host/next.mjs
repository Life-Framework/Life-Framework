// `host next` (#1416): the next scheduled restart and the next announced session, from a clock and two
// parsed files. No I/O here; tools/host/commands/next.mjs reads the files.
//
// The restart rule mirrors the game's LF_RestartSchedule (O5, #1264) and LF_RestartConfig (#1343): `times` are
// 24-hour HH:MM in the server's local time, a time that is not HH:MM is skipped, an empty list is "no scheduled
// restarts", a restart exactly on the clock is that restart (0 s), `warningMinutes` default to 15, 5, 1, each
// lead is 1 to 120 and a repeat counts once. Unlike the game, which drops a warning whose time has passed,
// every warning is listed here with its state (`passed` or `ahead`), so the host sees what was already said.
//
// A session counts only when it was posted: `announced` is an ISO date-time (sessions.schema.json). A draft
// (`host announce` without `--send`), a null `next`, an unreadable time and a session already started are not
// the next session.

export const DEFAULT_LEADS = [15, 5, 1];
export const LEAD_MIN = 1;
export const LEAD_MAX = 120;

const TIME = /^([01]\d|2[0-3]):([0-5]\d)$/;

// The restart times as { h, m } in list order; anything that is not HH:MM is skipped.
function parseTimes(times) {
  const out = [];
  for (const t of Array.isArray(times) ? times : []) {
    const m = typeof t === "string" ? TIME.exec(t) : null;
    if (m) out.push({ h: Number(m[1]), m: Number(m[2]) });
  }
  return out;
}

// Warning leads, longest first, each 1 to 120 whole minutes, a repeat once.
function parseLeads(leads) {
  const list = Array.isArray(leads) ? leads : DEFAULT_LEADS;
  const seen = [];
  for (const n of list) if (Number.isInteger(n) && n >= LEAD_MIN && n <= LEAD_MAX && !seen.includes(n)) seen.push(n);
  return seen.sort((a, b) => b - a);
}

// The next restart at or after `now` (a Date, local time): { at, untilMs, warnings: [{ lead, at, state }] },
// or null when none is scheduled. `restarts` is the config's `restarts` section (may be undefined).
export function nextRestart(restarts, now = new Date()) {
  const times = parseTimes(restarts && restarts.times);
  if (!times.length) return null;
  let best = null;
  for (const { h, m } of times) {
    for (let day = 0; day <= 1; day++) {
      const at = new Date(now.getFullYear(), now.getMonth(), now.getDate() + day, h, m, 0, 0);
      if (at.getTime() >= now.getTime() && (!best || at.getTime() < best.getTime())) best = at;
    }
  }
  if (!best) return null;
  const warnings = parseLeads(restarts && restarts.warningMinutes).map((lead) => {
    const at = new Date(best.getTime() - lead * 60000);
    return { lead, at, state: at.getTime() > now.getTime() ? "ahead" : "passed" };
  });
  return { at: best, untilMs: best.getTime() - now.getTime(), warnings };
}

// The posted session after `now`: { at, build, wipe, announced } (Dates for the two times), or null.
// `sessions` is the parsed sessions.json.
export function nextSession(sessions, now = new Date()) {
  const next = sessions && typeof sessions === "object" ? sessions.next : null;
  if (!next || typeof next !== "object") return null;
  const posted = typeof next.announced === "string" && next.announced !== "" ? Date.parse(next.announced) : NaN;
  if (Number.isNaN(posted)) return null;
  const at = typeof next.at === "string" ? Date.parse(next.at) : NaN;
  if (Number.isNaN(at) || at < now.getTime()) return null;
  return { at: new Date(at), build: String(next.build ?? ""), wipe: next.wipe === true, announced: new Date(posted) };
}

// True when sessions.json holds a `next` that was never posted (a draft): the command says so.
export function hasDraft(sessions) {
  const next = sessions && typeof sessions === "object" ? sessions.next : null;
  return !!next && typeof next === "object" && Number.isNaN(Date.parse(typeof next.announced === "string" ? next.announced : ""));
}

const pad = (n) => String(n).padStart(2, "0");
export const clock = (d) => `${pad(d.getHours())}:${pad(d.getMinutes())}`;
export const dayStamp = (d) => `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
const WEEKDAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

// "Mon 2026-11-16 06:00" in the machine's local time.
export const localStamp = (d) => `${WEEKDAYS[d.getDay()]} ${dayStamp(d)} ${clock(d)}`;

// "10 min", "2 h 5 min", "3 d 4 h"; under a minute is "now".
export function span(ms) {
  const min = Math.floor(Math.max(0, ms) / 60000);
  if (min < 1) return "now";
  const d = Math.floor(min / 1440);
  const h = Math.floor((min % 1440) / 60);
  const m = min % 60;
  if (d) return `${d} d${h ? ` ${h} h` : ""}`;
  if (h) return `${h} h${m ? ` ${m} min` : ""}`;
  return `${m} min`;
}
