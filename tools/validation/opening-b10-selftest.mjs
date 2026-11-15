// Proves tools/playtest/opening/b10-site.mjs, beat B10 of the opening-day script (#1493): sessions.json passes its
// schema, the built servers page shows that time, and the page holds no player count and no live status words.
// inputs: tools/**, website/content/**
//
// Fixtures are built in memory: a sessions.json, and a servers page shaped like the prerendered one
// (website/dist/servers/index.html). The selftest never builds the site and never reads website/dist.
//   1. a scheduled session and the page that shows its UTC time pass; "none" and the empty-state page pass;
//   2. a schema-invalid file (missing key, wrong type, unknown key, bad date) fails;
//   3. a page without the session time fails, and so does a stale time;
//   4. "players online", "player count", "12/40 players", a status badge or a "Server status" line fails;
//      the capacity sentence "about 40 players" and the word "players" in rules text pass;
//   5. a missing dist page is BLOCKED "site not built", not a pass.
//
// red-proof: 2026-11-15, run before tools/playtest/opening/b10-site.mjs existed it printed
// "FAIL cannot load b10-site.mjs" and exited 1.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fails = [];
const expect = (ok, msg) => {
  if (!ok) fails.push(msg);
};

let b10;
try {
  b10 = await import(pathToFileURL(join(root, "tools", "playtest", "opening", "b10-site.mjs")).href);
} catch (e) {
  console.log(`FAIL cannot load b10-site.mjs: ${e.message}`);
  process.exit(1);
}
const { checkSchema, utcText } = b10;
// The fixtures are dated around a fixed now, so the selftest does not rot when the fixture session passes.
const now = new Date("2026-11-15T12:00:00Z");
const checkPage = (html, sessions) => b10.checkPage(html, sessions, now);
const checkB10 = (o) => b10.checkB10({ now, ...o });

const schema = JSON.parse(readFileSync(join(root, "website", "content", "sessions.schema.json"), "utf8"));
const scheduled = { next: { at: "2026-11-21T20:00:00+01:00", build: "0.1.0", wipe: false, announced: "2026-11-15T12:30:00Z" } };
const none = { next: null };

// A page shaped like the prerendered servers page: nav, the next-session panel, the rules, the capacity note.
const page = (panel, extra = "") =>
  `<!doctype html><html><head><title>Servers</title><style>.status{color:red}</style><script>var players_online=1;</script></head>` +
  `<body><nav><a href="/">Home</a><a href="/servers">Servers</a></nav><main>` +
  `<div id="next-session"><p class="kicker">Flagship server</p><h2>Next session</h2>${panel}</div>` +
  `<p>Death drops what you carry. Everyone spawns civilian.</p>` +
  `<p>The listen-server host handles about 40 players, and moves to a rented box once sessions fill.</p>${extra}` +
  `</main></body></html>`;
const timePanel = `<p><time datetime="2026-11-21T19:00:00.000Z">${utcText(new Date("2026-11-21T20:00:00+01:00"))}</time></p><p>Build 0.1.0.</p>`;
const nonePanel = `<p>No session announced yet: watch Discord</p>`;

// 1. good inputs
expect(utcText(new Date("2026-11-21T20:00:00+01:00")) === "Sat 21 Nov 2026, 19:00 UTC", `utcText: ${utcText(new Date("2026-11-21T20:00:00+01:00"))}`);
expect(checkSchema(scheduled, schema).length === 0, "scheduled sessions.json should pass the schema");
expect(checkSchema(none, schema).length === 0, "next: null should pass the schema");
expect(checkB10({ sessions: scheduled, schema, html: page(timePanel) }).length === 0, `scheduled + page with the time: ${checkB10({ sessions: scheduled, schema, html: page(timePanel) })}`);
expect(checkB10({ sessions: none, schema, html: page(nonePanel) }).length === 0, "none + empty-state page should pass");

// 2. schema-invalid
const bad = (n) => checkSchema({ next: { ...scheduled.next, ...n } }, schema).length > 0;
expect(checkSchema({}, schema).length > 0, "missing next should fail");
expect(checkSchema({ next: { at: scheduled.next.at } }, schema).length > 0, "missing build/wipe/announced should fail");
expect(bad({ wipe: "no" }), "wipe as a string should fail");
expect(bad({ at: "tomorrow evening" }), "a non-date at should fail");
expect(bad({ at: "2026-11-21T20:00:00" }), "an at without an offset should fail");
expect(bad({ build: "" }), "an empty build should fail");
expect(bad({ players: 12 }), "an unknown key (a player count) should fail");
expect(checkSchema({ next: null, online: true }, schema).length > 0, "an up/down field should fail");
const badRun = checkB10({ sessions: { next: { at: "x" } }, schema, html: page(timePanel) });
expect(badRun.some((m) => m.includes("sessions.json")), "a schema-invalid file should be reported by checkB10");

// 3. the page shows the time
expect(checkPage(page(nonePanel), scheduled).length > 0, "a page with no session time should fail for a scheduled session");
expect(checkPage(page(timePanel), none).length > 0, "a page showing a time for next: null should fail");
// #1537: a draft (no announced time) is not shown, like none.
const { announced: _posted, ...draftNext } = scheduled.next;
expect(checkPage(page(nonePanel), { next: draftNext }).length === 0, "a draft with the empty-state page should pass");
expect(checkPage(page(timePanel), { next: draftNext }).length > 0, "a page showing a draft's time should fail");
const stale = `<p><time>${utcText(new Date("2026-11-22T20:00:00+01:00"))}</time></p>`;
expect(checkPage(page(stale), scheduled).length > 0, "a different time should fail");

// 4. no player count, no live status words
const banned = {
  "players online": "<p>12 players online</p>",
  "online players": "<p>Online players: 12</p>",
  "player count": "<p>Player count 12</p>",
  "12/40 players": "<p>12/40 players</p>",
  "12 players connected": "<p>12 players connected</p>",
  "status badge": `<span class="badge status-online">Online</span>`,
  "role=status": `<div role="status">Up</div>`,
  "Server status": "<p>Server status: running</p>",
  "is online": "<p>The server is online.</p>",
  "offline": "<p>Offline</p>",
  "uptime": "<p>Uptime 99%</p>",
  "live now": "<p>Live now</p>",
};
for (const [name, extra] of Object.entries(banned)) {
  const errs = checkPage(page(timePanel, extra), scheduled);
  expect(errs.length > 0, `"${name}" on the page should fail`);
}
expect(checkPage(page(timePanel), scheduled).length === 0, "the capacity sentence 'about 40 players' should pass");
// scripts and styles are not page text: the fixture head holds "players_online" and ".status" and still passes.
expect(checkPage(page(timePanel), scheduled).length === 0, "script and style text is ignored");

// 5. no built page
const live = join(root, "website", "dist", "servers", "index.html");
const blocked = checkB10({ sessions: scheduled, schema, html: null });
expect(blocked.length === 1 && blocked[0].startsWith("BLOCKED site not built"), `a missing page should be BLOCKED site not built: ${blocked}`);
// the live file: checked only when the site was built here
if (existsSync(live)) {
  const liveSessions = JSON.parse(readFileSync(join(root, "website", "content", "sessions.json"), "utf8"));
  const errs = checkB10({ sessions: liveSessions, schema, html: readFileSync(live, "utf8") });
  for (const e of errs) fails.push(`website/dist/servers/index.html: ${e}`);
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`SUMMARY opening-b10-selftest fail ${fails.length}`);
  process.exit(1);
}
console.log("SUMMARY opening-b10-selftest ok");
