// What the server browser shows, for `tools\cli host listing`: one A2S_INFO query, every field the
// listing carries (name, map, max players, password flag, version) compared with
// server/flagship/server.json. tools/host/a2s.mjs stops at the bot count, so the reply is
// parsed here to the end (server type, environment, visibility, VAC, version).
import { readFileSync } from "node:fs";
import { createSocket } from "node:dgram";
import { buildInfoRequest, TIMEOUT_MS } from "./a2s.mjs";

// The alpha listing name is decided (maintainer, M4): a typo in server.json must not make a wrong listing pass.
export const ALPHA_NAME = "Life Framework | Everon 1989 (Alpha)";

const HEADER = Buffer.from([0xff, 0xff, 0xff, 0xff]);

// Reads { name, maxPlayers } from the flagship server.json (game.name, game.maxPlayers).
export function loadExpected(path) {
  const game = JSON.parse(readFileSync(path, "utf8")).game ?? {};
  return { name: game.name, maxPlayers: game.maxPlayers };
}

// { kind: "challenge", challenge } or { kind: "info", name, map, players, maxPlayers, password, version }.
export function parseListingReply(buf) {
  if (buf.length < 5 || !buf.subarray(0, 4).equals(HEADER)) throw new Error("not an A2S reply (bad header)");
  if (buf[4] === 0x41) {
    if (buf.length < 9) throw new Error("A2S challenge is truncated");
    return { kind: "challenge", challenge: Buffer.from(buf.subarray(5, 9)) };
  }
  if (buf[4] !== 0x49) throw new Error(`unexpected A2S reply type 0x${buf[4].toString(16)}`);
  let pos = 5;
  const byte = () => {
    if (pos >= buf.length) throw new Error("A2S reply is truncated");
    return buf[pos++];
  };
  const string = () => {
    const end = buf.indexOf(0, pos);
    if (end < 0) throw new Error("A2S reply is truncated");
    const s = buf.toString("utf8", pos, end);
    pos = end + 1;
    return s;
  };
  byte(); // protocol
  const name = string();
  const map = string();
  string(); // folder
  string(); // game
  byte();
  byte(); // app id
  const players = byte();
  const maxPlayers = byte();
  byte(); // bots
  byte(); // server type
  byte(); // environment
  const password = byte() === 1;
  byte(); // VAC
  const version = string();
  return { kind: "info", name, map, players, maxPlayers, password, version };
}

// What the A2S output says about the password: Reforger does not report it, so it is neither "set" nor "none".
export function passwordLine() {
  return "password: not reported by A2S";
}

// Problem lines, one per wrong field; empty when the listing is right.
//   expected: { name, maxPlayers } from server.json. opts.version / opts.map are checked only when
//   given (server.json holds neither).
export function compareListing(info, expected, opts = {}) {
  const problems = [];
  if (info.name !== ALPHA_NAME) problems.push(`name is '${info.name}', the alpha listing must be exactly '${ALPHA_NAME}'`);
  else if (expected.name !== undefined && info.name !== expected.name) problems.push(`name is '${info.name}', server.json has '${expected.name}'`);
  if (expected.maxPlayers !== undefined && info.maxPlayers !== expected.maxPlayers) problems.push(`max players is ${info.maxPlayers}, server.json has ${expected.maxPlayers}`);
  // The password flag is not checked: Reforger's A2S never reports it (a wrong password is refused live).
  if (opts.map !== undefined && info.map !== opts.map) problems.push(`map is '${info.map}', expected '${opts.map}'`);
  if (opts.version !== undefined && info.version !== opts.version) problems.push(`version is '${info.version}', expected '${opts.version}'`);
  return problems;
}

// One UDP request/reply, bounded by timeoutMs.
export function udpExchange(host, port, timeoutMs = TIMEOUT_MS) {
  return (request) =>
    new Promise((resolve, reject) => {
      const sock = createSocket("udp4");
      const timer = setTimeout(() => {
        sock.close();
        reject(new Error(`A2S query to ${host}:${port} timed out after ${timeoutMs}ms`));
      }, timeoutMs);
      sock.once("error", (err) => {
        clearTimeout(timer);
        sock.close();
        reject(err);
      });
      sock.once("message", (msg) => {
        clearTimeout(timer);
        sock.close();
        resolve(msg);
      });
      sock.send(request, port, host, (err) => {
        if (err) {
          clearTimeout(timer);
          sock.close();
          reject(err);
        }
      });
    });
}

// Query (with the challenge step), parse and compare. Never throws: a dead server or a garbage
// reply is one problem line. exchange(requestBuffer) -> Promise<replyBuffer> is injectable.
export async function runListing({ exchange, expected, ...opts }) {
  let info;
  try {
    let reply = parseListingReply(await exchange(buildInfoRequest()));
    if (reply.kind === "challenge") reply = parseListingReply(await exchange(buildInfoRequest(reply.challenge)));
    if (reply.kind !== "info") throw new Error("A2S server challenged twice");
    info = reply;
  } catch (e) {
    return { ok: false, info: null, problems: [e.message] };
  }
  const problems = compareListing(info, expected, opts);
  return { ok: problems.length === 0, info, problems };
}
