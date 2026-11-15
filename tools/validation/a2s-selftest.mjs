// tools/validation/a2s-selftest.mjs
// inputs: tools/host/a2s.mjs, tools/host/commands/players.mjs, tools/validation/fixtures/a2s/**
//
// Proves the A2S_INFO client behind `tools\cli host players`: the request packet, the
// challenge reply, the plain info reply (fixtures in fixtures/a2s/*.hex), address parsing,
// and a full query against a fake UDP server (challenge first, then info) plus a timeout
// against a server that never answers.
// red-proof: before tools/host/a2s.mjs existed this printed
// "FAIL cannot load tools/host/a2s.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { readFileSync } from "node:fs";
import { createSocket } from "node:dgram";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "a2s");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function check(cond, msg) {
  if (!cond) fail(msg);
}

const hex = (name) => Buffer.from(readFileSync(join(FIX, name), "utf8").replace(/\s+/g, ""), "hex");

let a2s;
let players;
try {
  a2s = await import(pathToFileURL(join(ROOT, "tools", "host", "a2s.mjs")).href);
  players = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "players.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/a2s.mjs or commands/players.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY a2s-selftest fail ${failed}`);
  process.exit(1);
}

// Request: header, 'T', "Source Engine Query\0", optional 4-byte challenge.
const plain = a2s.buildInfoRequest();
check(plain.toString("hex") === "ffffffff54536f7572636520456e67696e6520517565727900", `plain request is ${plain.toString("hex")}`);
const withChallenge = a2s.buildInfoRequest(Buffer.from([1, 2, 3, 4]));
check(withChallenge.length === plain.length + 4 && withChallenge.subarray(plain.length).toString("hex") === "01020304", "challenge request appends the 4 challenge bytes");

// Challenge reply.
const ch = a2s.parseInfoReply(hex("challenge.hex"));
check(ch.kind === "challenge" && ch.challenge.toString("hex") === "0a0b0c0d", `challenge fixture parsed as ${JSON.stringify(ch)}`);

// Info reply.
const info = a2s.parseInfoReply(hex("info.hex"));
check(info.kind === "info", `info fixture kind is ${info.kind}`);
check(info.name === "Life Framework Playtest", `name is ${info.name}`);
check(info.map === "LF_Everon1989", `map is ${info.map}`);
check(info.players === 3, `players is ${info.players}`);
check(info.maxPlayers === 32, `maxPlayers is ${info.maxPlayers}`);

// Garbage and truncation are errors, never a throw out of the parser's callers' timers.
for (const [label, buf] of [["empty", Buffer.alloc(0)], ["bad header", Buffer.from("00112233490000", "hex")], ["truncated", hex("info.hex").subarray(0, 12)], ["unknown type", Buffer.from("ffffffff5800", "hex")]]) {
  let threw = false;
  try {
    a2s.parseInfoReply(buf);
  } catch {
    threw = true;
  }
  check(threw, `${label} reply must throw`);
}

// Address parsing: host:a2sport, host alone (17777), nothing (127.0.0.1:17777).
const addr = (s) => JSON.stringify(a2s.parseAddress(s));
check(addr("10.0.0.5:17877") === '{"host":"10.0.0.5","port":17877}', `host:port is ${addr("10.0.0.5:17877")}`);
check(addr("example.org") === '{"host":"example.org","port":17777}', `host alone is ${addr("example.org")}`);
check(addr(undefined) === '{"host":"127.0.0.1","port":17777}', `default is ${addr(undefined)}`);
let badPort = false;
try {
  a2s.parseAddress("host:99999");
} catch {
  badPort = true;
}
check(badPort, "a port above 65535 must throw");

// Live path against a fake server: first request gets a challenge, the one carrying it gets the info.
const sock = createSocket("udp4");
const seen = [];
sock.on("message", (msg, rinfo) => {
  seen.push(msg.toString("hex"));
  const has = msg.length > plain.length;
  sock.send(has ? hex("info.hex") : hex("challenge.hex"), rinfo.port, rinfo.address);
});
await new Promise((res) => sock.bind(0, "127.0.0.1", res));
const port = sock.address().port;
try {
  const got = await a2s.queryInfo("127.0.0.1", port, { timeoutMs: 2000 });
  check(got.name === "Life Framework Playtest" && got.players === 3, `queryInfo returned ${JSON.stringify(got)}`);
  check(seen.length === 2 && seen[1].endsWith("0a0b0c0d"), `server saw ${seen.length} requests, second ${seen[1]}`);

  const lines = players.formatInfo(got);
  check(lines.includes("Life Framework Playtest") && lines.includes("LF_Everon1989") && lines.includes("3/32"), `formatInfo gave ${lines}`);
} catch (e) {
  fail(`queryInfo against the fake server threw ${e.message}`);
}
sock.close();

// Timeout: a bound socket that never replies.
const mute = createSocket("udp4");
await new Promise((res) => mute.bind(0, "127.0.0.1", res));
const t0 = Date.now();
let timedOut = false;
try {
  await a2s.queryInfo("127.0.0.1", mute.address().port, { timeoutMs: 300 });
} catch (e) {
  timedOut = /timed out/i.test(e.message);
}
check(timedOut && Date.now() - t0 < 2000, "a silent server must fail with 'timed out' within the budget");
mute.close();

if (failed) {
  console.log(`SUMMARY a2s-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY a2s-selftest ok");
