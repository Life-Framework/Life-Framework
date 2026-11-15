// Pure helpers for `tools\cli host announce` (#1273): validate the session time, fill the
// announcement template, build the sessions.json row. No I/O here; the command does that.
//
// Template: docs/community/announce-template.md. Placeholders are {{time}}, {{relative}}, {{build}},
// {{server}}, {{address}}, {{rules}} and the conditional {{wipe}} line.
// Row shape (the site's next-session record): { at, build, wipe } for a draft, plus
// `announced` (ISO time the post went out) once `--send` posted it (#1537). A draft has no
// `announced` key, so nothing reads a draft as a posted notice: `host wipe` counts its 24 hours
// from `announced` and refuses a row without one.

export const SERVER_NAME = "Life Framework | Everon 1989 (Alpha)";
export const DEFAULT_ADDRESS = "Press **Join the community server** in the launcher: https://life-framework.org/playtest";
export const DEFAULT_RULES = "https://life-framework.org/docs/player-faq";
// The join block of a session post (`host announce`, `host post session`). The opening post keeps
// DEFAULT_ADDRESS: it says the password is gone, which this block does not. The password itself is
// never posted; players get it in Discord.
export const WORKSHOP_CORE = "https://reforger.armaplatform.com/workshop/4550A2B656E1B569";
export const WORKSHOP_PACK = "https://reforger.armaplatform.com/workshop/B1E84D5687E11724";
export const DEFAULT_SESSION_JOIN = [
  "- In-game **Server Browser**: search `Life Framework | Everon 1989 (Alpha)`, or direct join with code `0412780561`.",
  "- The password is in Discord.",
  `- Mods (Workshop): Life Framework <${WORKSHOP_CORE}> and Everon 1989 <${WORKSHOP_PACK}>`,
].join("\n");
export const WIPE_NOTICE_MS = 24 * 60 * 60 * 1000;

export const WIPE_LINE =
  "**WIPE:** this session starts with a wipe. All progress is cleared; you start over. " +
  "A wipe is announced at least 24 hours ahead, and progress is kept from the public launch.";

// { ok, ms } or { ok: false, error }. A wipe needs at least 24 hours of notice (the post goes out now).
export function checkAt({ at, wipe, now }) {
  const ms = typeof at === "string" && /^\d{4}-\d{2}-\d{2}T/.test(at) ? Date.parse(at) : NaN;
  if (Number.isNaN(ms)) return { ok: false, error: `--at "${at}" is not an ISO time (2026-11-22T19:00:00Z or with an offset)` };
  if (ms <= now) return { ok: false, error: `--at ${at} is in the past` };
  if (wipe && ms - now < WIPE_NOTICE_MS) {
    const hours = ((ms - now) / 3600000).toFixed(1);
    return { ok: false, error: `a wipe needs 24 hours of notice on Discord; --at ${at} is only ${hours} h away` };
  }
  return { ok: true, ms };
}

// The next-session record the site reads. `at` is normalized to UTC. `announced` is the ISO time the
// post went out, or absent for a draft; anything that is not a parseable time is treated as a draft.
export function sessionRow({ at, build, wipe, announced = null }) {
  const row = { at: new Date(Date.parse(at)).toISOString(), build, wipe: !!wipe };
  const ms = typeof announced === "string" ? Date.parse(announced) : NaN;
  if (!Number.isNaN(ms)) row.announced = new Date(ms).toISOString();
  return row;
}

// Fills the template. The `{{wipe}}` line is kept only for a wipe session; the rest is replaced everywhere.
export function buildPost({ template, at, build, wipe, address = DEFAULT_SESSION_JOIN, rules = DEFAULT_RULES }) {
  const epoch = Math.floor(Date.parse(at) / 1000);
  const vars = {
    time: `<t:${epoch}:F>`,
    relative: `<t:${epoch}:R>`,
    build,
    server: SERVER_NAME,
    address,
    rules,
    wipe: wipe ? WIPE_LINE : "",
  };
  const text = template
    .replace(/\{\{(\w+)\}\}/g, (m, k) => (k in vars ? vars[k] : m))
    .replace(/\n{3,}/g, "\n\n");
  return text.trim() + "\n";
}

// Returns the sessions.json text with `next` set.
export function sessionsJson(row) {
  return JSON.stringify({ next: row }, null, 2) + "\n";
}
