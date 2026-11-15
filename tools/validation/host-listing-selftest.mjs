// tools/validation/host-listing-selftest.mjs
// inputs: tools/host/listing.mjs, tools/host/commands/listing.mjs, tools/host/a2s.mjs, server/flagship/server.json
//
// Proves `tools\cli host listing`: an injected A2S reply that matches server/flagship/server.json
// passes; a wrong name, a missing password flag and 64 max players each fail with one line;
// the alpha name is pinned; the challenge step and a silent server are handled.
// red-proof: before tools/host/listing.mjs existed this printed
// "FAIL cannot load tools/host/listing.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SERVER_JSON = join(ROOT, "server", "flagship", "server.json");
const ALPHA_NAME = "Life Framework | Everon 1989 (Alpha)";
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function check(cond, msg) {
  if (!cond) fail(msg);
}

let listing;
try {
  listing = await import(pathToFileURL(join(ROOT, "tools", "host", "listing.mjs")).href);
  await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "listing.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/listing.mjs or commands/listing.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-listing-selftest fail ${failed}`);
  process.exit(1);
}

// A full A2S_INFO reply: header, 'I', protocol, name, map, folder, game, appId, players, max, bots,
// server type, environment, visibility (1 = password), VAC, version.
function infoReply({ name = ALPHA_NAME, map = "Everon", players = 3, maxPlayers = 16, password = true, version = "1.4.0.18" } = {}) {
  const z = (s) => Buffer.concat([Buffer.from(s, "utf8"), Buffer.from([0])]);
  return Buffer.concat([
    Buffer.from([0xff, 0xff, 0xff, 0xff, 0x49, 17]),
    z(name), z(map), z("reforger"), z("Arma Reforger"),
    Buffer.from([0x00, 0x00, players, maxPlayers, 0, 0x64, 0x77, password ? 1 : 0, 0]),
    z(version),
  ]);
}
const challenge = Buffer.from("ffffffff410a0b0c0d", "hex");

const expected = listing.loadExpected(SERVER_JSON);
check(expected.name === ALPHA_NAME, `server.json expected name is ${expected.name}`);
check(expected.maxPlayers === 16, `server.json expected maxPlayers is ${expected.maxPlayers}`);

const run = (reply, opts = {}) => listing.runListing({ exchange: async () => reply, expected, ...opts });

// Parse: the fields past `bots` (visibility, version) are read.
const parsed = listing.parseListingReply(infoReply());
check(parsed.name === ALPHA_NAME && parsed.map === "Everon" && parsed.maxPlayers === 16 && parsed.password === true && parsed.version === "1.4.0.18", `parsed ${JSON.stringify(parsed)}`);
check(listing.parseListingReply(infoReply({ password: false })).password === false, "visibility 0 parses as no password");

// Match passes.
const ok = await run(infoReply());
check(ok.ok && ok.problems.length === 0, `matching reply failed: ${ok.problems.join(" / ")}`);

// Each failure is exactly one line, and names the field.
const cases = [
  ["wrong name", infoReply({ name: "Life Framework Playtest" }), /name/],
  ["missing password flag", infoReply({ password: false }), /password/],
  ["64 max players", infoReply({ maxPlayers: 64 }), /max players/],
];
for (const [label, reply, re] of cases) {
  const r = await run(reply);
  check(!r.ok && r.problems.length === 1 && re.test(r.problems[0]), `${label}: expected one ${re} line, got ${JSON.stringify(r.problems)}`);
}

// The alpha name is pinned: a server.json that lost it does not make a wrong listing pass.
const drift = await run(infoReply({ name: "Something else" }), { expected: { ...expected, name: "Something else" } });
check(!drift.ok && drift.problems.some((p) => /name/.test(p)), "a name other than the alpha name must fail even if server.json agrees");

// The password is only waived by an explicit option.
const open = await run(infoReply({ password: false }), { allowOpen: true });
check(open.ok, `allowOpen should accept no password: ${open.problems.join(" / ")}`);

// Version and map are checked only when asked for.
const ver = await run(infoReply({ version: "1.3.0.1" }), { version: "1.4.0.18" });
check(!ver.ok && ver.problems.length === 1 && /version/.test(ver.problems[0]), `version mismatch gave ${JSON.stringify(ver.problems)}`);
const map = await run(infoReply({ map: "Arland" }), { map: "Everon" });
check(!map.ok && map.problems.length === 1 && /map/.test(map.problems[0]), `map mismatch gave ${JSON.stringify(map.problems)}`);

// Challenge step: a challenge reply is answered with the request carrying it.
const seen = [];
const viaChallenge = await listing.runListing({
  expected,
  exchange: async (req) => {
    seen.push(req.toString("hex"));
    return req.length > 25 ? infoReply() : challenge;
  },
});
check(viaChallenge.ok && seen.length === 2 && seen[1].endsWith("0a0b0c0d"), `challenge path: ok=${viaChallenge.ok}, requests=${seen.length}`);

// An unreachable server and a garbage reply are one failing line, not a throw.
const down = await listing.runListing({ expected, exchange: async () => { throw new Error("A2S query to x:1 timed out after 3000ms"); } });
check(!down.ok && down.problems.length === 1 && /timed out/.test(down.problems[0]), `timeout gave ${JSON.stringify(down.problems)}`);
const junk = await run(Buffer.from("00112233490000", "hex"));
check(!junk.ok && junk.problems.length === 1, `garbage reply gave ${JSON.stringify(junk.problems)}`);

if (failed) {
  console.log(`SUMMARY host-listing-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-listing-selftest ok");
