// Beat B10 of the opening-day script (#1493): the site's next session.
//   node tools/playtest/opening/b10-site.mjs [--dist <dir>] [--sessions <file>]
// Checks that website/content/sessions.json passes sessions.schema.json, that the prerendered
// website/dist/servers/index.html shows that session's time (the page's own UTC text), and that the page holds no
// player count and no live status words (decision 2026-11-14: the site shows the next session time only).
// It reads the built page and never builds the site: no dist page prints "BLOCKED site not built" and exits 2.
// Exit: 0 pass, 1 fail, 2 blocked.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..", "..");

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const two = (n) => String(n).padStart(2, "0");

/** The time text website/src/pages/Servers.tsx prerenders: fixed UTC, so the page and this check agree. */
export const utcText = (d) =>
  `${DAYS[d.getUTCDay()]} ${d.getUTCDate()} ${MONTHS[d.getUTCMonth()]} ${d.getUTCFullYear()}, ${two(d.getUTCHours())}:${two(d.getUTCMinutes())} UTC`;

export const EMPTY_TEXT = "No session announced yet";

/** The JSON Schema subset sessions.schema.json uses: type (or a list), required, properties, additionalProperties false, enum, minLength, format date-time (with offset). */
export function checkSchema(value, schema, path = "$") {
  const kind = value === null ? "null" : Array.isArray(value) ? "array" : typeof value;
  const types = schema.type === undefined ? null : [].concat(schema.type);
  if (types && !types.includes(kind)) return [`${path}: expected ${types.join("|")}, got ${kind}`];
  const errs = [];
  if (schema.enum && !schema.enum.includes(value)) errs.push(`${path}: not one of ${JSON.stringify(schema.enum)}`);
  if (kind === "string") {
    if (schema.minLength !== undefined && value.length < schema.minLength) errs.push(`${path}: shorter than ${schema.minLength}`);
    if (schema.format === "date-time") {
      const ok = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$/.test(value) && !Number.isNaN(Date.parse(value));
      if (!ok) errs.push(`${path}: "${value}" is not an ISO 8601 date-time with an offset`);
    }
  }
  if (kind === "object") {
    for (const k of schema.required || []) if (!(k in value)) errs.push(`${path}.${k}: required`);
    for (const [k, v] of Object.entries(value)) {
      if (schema.properties?.[k]) errs.push(...checkSchema(v, schema.properties[k], `${path}.${k}`));
      else if (schema.additionalProperties === false) errs.push(`${path}.${k}: unknown key`);
    }
  }
  return errs;
}

/** The visible text of a page: script and style bodies and tags dropped, entities for the few that matter decoded. */
export function pageText(html) {
  return html
    .replace(/<script\b[\s\S]*?<\/script>/gi, " ")
    .replace(/<style\b[\s\S]*?<\/style>/gi, " ")
    .replace(/<!--[\s\S]*?-->/g, " ")
    .replace(/<[^>]+>/g, " ")
    .replace(/&nbsp;/g, " ")
    .replace(/&amp;/g, "&")
    .replace(/\s+/g, " ")
    .trim();
}

// Phrases that read as a live player count or a live up/down status. "about 40 players" (capacity) is not one.
const BANNED_TEXT = [
  [/\bplayers?\s+(online|connected|playing|in game|now)\b/i, "a player count"],
  [/\bonline\s+players?\b/i, "a player count"],
  [/\bplayer\s+count\b/i, "a player count"],
  [/\b\d+\s*\/\s*\d+\s+players?\b/i, "a player count"],
  [/\b\d+\s+(online|connected)\b/i, "a player count"],
  [/\bcurrently\s+(online|live|running|playing)\b/i, "a live status"],
  [/\bserver\s+status\b/i, "a live status"],
  [/\bstatus\b/i, "a status word"],
  [/\boffline\b/i, "a live status"],
  [/\buptime\b/i, "a live status"],
  [/\b(is|are)\s+(online|up|down|live)\b/i, "a live status"],
  [/\b(online|live)\s+now\b/i, "a live status"],
];
// A status badge by markup: a class, role or aria-label that names a status or badge.
const BANNED_MARKUP = /<[^>]+\b(?:class|id|role|aria-label)\s*=\s*"[^"]*\b(?:status|badge|online|offline|player-?count)\b[^"]*"/i;

/** Problems with a servers page for a sessions.json. `now` (a Date) decides whether the session is still ahead. */
export function checkPage(html, sessions, now = new Date()) {
  const errs = [];
  const text = pageText(html);
  // A draft (no announced time, #1537) is not shown: the page says nothing is scheduled until it is posted.
  const at = sessions?.next?.at && typeof sessions.next.announced === "string" ? new Date(sessions.next.at) : null;
  const ahead = at && !Number.isNaN(at.getTime()) && at.getTime() >= now.getTime();
  if (ahead) {
    const want = utcText(at);
    if (!text.includes(want)) errs.push(`page: does not show the next session time "${want}"`);
  } else if (!text.includes(EMPTY_TEXT)) {
    errs.push(`page: sessions.json has no upcoming session, so the page should say "${EMPTY_TEXT}"`);
  } else if (/\b\d{1,2}:\d{2} UTC\b/.test(text)) {
    errs.push("page: shows a session time but sessions.json has no upcoming session");
  }
  for (const [re, what] of BANNED_TEXT) {
    const m = re.exec(text);
    if (m) errs.push(`page: holds ${what} ("${m[0]}")`);
  }
  const mk = BANNED_MARKUP.exec(html.replace(/<script\b[\s\S]*?<\/script>/gi, " ").replace(/<style\b[\s\S]*?<\/style>/gi, " "));
  if (mk) errs.push(`page: holds a status badge (${mk[0].slice(0, 80)})`);
  return errs;
}

/** All of B10. `html` null means the site is not built. Returns a list of messages; empty is a pass. */
export function checkB10({ sessions, schema, html, now = new Date() }) {
  if (html === null || html === undefined) return ["BLOCKED site not built: website/dist/servers/index.html is missing"];
  const errs = checkSchema(sessions, schema).map((e) => `sessions.json: ${e}`);
  if (errs.length) return errs;
  return checkPage(html, sessions, now);
}

function main(argv) {
  const arg = (name, dflt) => (argv.includes(name) ? argv[argv.indexOf(name) + 1] : dflt);
  const dist = resolve(arg("--dist", join(root, "website", "dist")));
  const sessionsPath = resolve(arg("--sessions", join(root, "website", "content", "sessions.json")));
  const schema = JSON.parse(readFileSync(join(root, "website", "content", "sessions.schema.json"), "utf8"));
  const pagePath = join(dist, "servers", "index.html");
  // A site built before the first announcement has no sessions.json; the page then says nothing is scheduled.
  const sessions = existsSync(sessionsPath) ? JSON.parse(readFileSync(sessionsPath, "utf8")) : { next: null };
  const html = existsSync(pagePath) ? readFileSync(pagePath, "utf8") : null;
  const errs = checkB10({ sessions, schema, html });
  if (errs.length === 1 && errs[0].startsWith("BLOCKED")) {
    console.log(errs[0]);
    console.log("SUMMARY b10-site blocked");
    return 2;
  }
  for (const e of errs) console.log(`FAIL ${e}`);
  console.log(errs.length ? `SUMMARY b10-site fail ${errs.length}` : "SUMMARY b10-site ok");
  return errs.length ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main(process.argv.slice(2)));
