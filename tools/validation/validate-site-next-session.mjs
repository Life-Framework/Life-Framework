// The site shows the next session time and nothing live (#1680, the 2026-11-14 decision).
// inputs: website/src/pages/Servers.tsx, website/src/pages/Alpha.tsx, website/src/pages/Join.tsx, website/src/components/NextSession.tsx, website/content/sessions.json, tools/validation/fixtures/site-next-session/**
//
// /servers, /alpha and /join say when the next announced session is, read from website/content/sessions.json at
// build time, and no more: no request to a status endpoint, no A2S query, no player count from a live source
// (a count of players online goes stale and promises a server that may be down). Comments and string literals are
// stripped first, so prose such as "no player count" or "the A2S port" does not trip it. A page is flagged for:
//   a network call      fetch(, XMLHttpRequest, WebSocket, EventSource, sendBeacon, axios
//   a live-data hook    useServerStatus, useLive..., useA2S..., useServerInfo
//   an A2S query        an a2s... or ...A2SQuery identifier (prose "A2S" in JSX text is fine), or an a2s/status/players URL in a string
//   a player count      .players, a `players:` key, `{players}`, playerCount, playersOnline, onlinePlayers, ...
// website/src/lib/serverbundle.ts (the generator's bundle, whose `a2s` is a port in config) is not a page and is not read.
// Fixtures: fixtures/site-next-session/fetch-status.tsx and live-players.tsx must fail, clean.tsx must pass, every run.
// red-proof: `node tools/validation/validate-site-next-session.mjs --surface tools/validation/fixtures/site-next-session/fetch-status.tsx`
// exits 1 with "FAIL ... fetch-status.tsx:6" (2026-11-15, #1680).

import { existsSync, readFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURES = join("tools", "validation", "fixtures", "site-next-session");
const PAGES = ["website/src/pages/Servers.tsx", "website/src/pages/Alpha.tsx", "website/src/pages/Join.tsx"];

// Blank out comments and the inside of string/template literals, keeping every newline so line numbers hold.
// Strings that look like a URL keep their text: an a2s or status URL is what the URL rule reads.
export function strip(src) {
  let out = "";
  let i = 0;
  const blank = (s) => s.replace(/[^\n]/g, " ");
  while (i < src.length) {
    const c = src[i];
    const two = src.slice(i, i + 2);
    if (two === "//" && src[i - 1] !== ":") {
      const end = src.indexOf("\n", i);
      const stop = end < 0 ? src.length : end;
      out += blank(src.slice(i, stop));
      i = stop;
    } else if (two === "/*") {
      const end = src.indexOf("*/", i + 2);
      const stop = end < 0 ? src.length : end + 2;
      out += blank(src.slice(i, stop));
      i = stop;
    } else if (c === '"' || c === "'" || c === "`") {
      let j = i + 1;
      while (j < src.length && src[j] !== c && !(c !== "`" && src[j] === "\n")) j += src[j] === "\\" ? 2 : 1;
      const body = src.slice(i + 1, j);
      out += c + (/\/\/|\/[\w-]*(?:a2s|status|players)\b/i.test(body) ? body : blank(body)) + (src[j] === c ? c : "");
      i = j + 1;
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

const RULES = [
  { re: /\b(?:fetch\s*\(|XMLHttpRequest\b|WebSocket\b|EventSource\b|sendBeacon\b|axios\b)/, what: "a network call" },
  { re: /\buse(?:Server(?:Status|Info|Players)|Live\w*|A2S\w*)\b/, what: "a live-data hook" },
  { re: /\b(?:a2s\w*|\w*A2S(?:Query|Client|Info)\w*)\b/, what: "an A2S query" },
  { re: /["'`][^"'`\n]*\/(?:api\/)?(?:status|players|server-?info)\b[^"'`\n]*["'`]/i, what: "a status or players URL" },
  { re: /\b(?:playerCount|playersOnline|onlinePlayers|currentPlayers|numPlayers|players_online)\b|\.players\b|\bplayers\s*:|\{\s*players\s*\}/, what: "a player count" },
];

// [{ line, what, text }] for one source text.
export function findLive(src) {
  const lines = strip(src).split("\n");
  const orig = src.split("\n");
  const out = [];
  lines.forEach((l, k) => {
    for (const r of RULES) if (r.re.test(l)) out.push({ line: k + 1, what: r.what, text: orig[k].trim().slice(0, 100) });
  });
  return out;
}

const rel = (f) => relative(ROOT, f).replaceAll("\\", "/");
const show = (file, b) => `${file}:${b.line}: ${b.what}: ${b.text}`;
const surfaceArg = process.argv.indexOf("--surface");

if (surfaceArg > -1) {
  const file = process.argv[surfaceArg + 1];
  if (!file || !existsSync(file)) { console.error(`FAIL no such file: ${file}`); process.exit(1); }
  const bad = findLive(readFileSync(file, "utf8"));
  for (const b of bad) console.error("FAIL " + show(file, b));
  if (bad.length) process.exit(1);
  console.log(`validate-site-next-session: OK ${file}`);
  process.exit(0);
}

const errors = [];
for (const [name, expectOk] of [["fetch-status.tsx", false], ["live-players.tsx", false], ["clean.tsx", true]]) {
  const f = join(ROOT, FIXTURES, name);
  if (!existsSync(f)) { errors.push(`fixture ${rel(f)} is missing`); continue; }
  const ok = findLive(readFileSync(f, "utf8")).length === 0;
  if (ok !== expectOk) errors.push(`fixture ${rel(f)} should ${expectOk ? "pass" : "fail"} the next-session-only rule`);
}
for (const p of PAGES) {
  const f = join(ROOT, p);
  if (!existsSync(f)) { errors.push(`${p} is missing: update PAGES in validate-site-next-session.mjs`); continue; }
  for (const b of findLive(readFileSync(f, "utf8"))) errors.push(show(p, b) + " (the site shows the next session time and nothing live)");
}

if (errors.length) {
  for (const e of errors) console.error("FAIL " + e);
  console.error(`validate-site-next-session: ${errors.length} problem(s)`);
  process.exit(1);
}
console.log(`validate-site-next-session: OK (${PAGES.length} pages show the next session and nothing live)`);
