// A2S_INFO client for `tools\cli host players` (docs/runbook use: is anyone on before a restart?).
//
// The Source query protocol over UDP: send "T" + "Source Engine Query\0"; a server may answer
// with a challenge ("A" + 4 bytes) that the request must repeat, then with the info ("I").
// Reforger's A2S port is a2s.port in the server config (17777 by default).
import { createSocket } from "node:dgram";

export const DEFAULT_HOST = "127.0.0.1";
export const DEFAULT_A2S_PORT = 17777;
export const TIMEOUT_MS = 3000;

const HEADER = Buffer.from([0xff, 0xff, 0xff, 0xff]);
const INFO_PAYLOAD = Buffer.concat([Buffer.from("TSource Engine Query", "latin1"), Buffer.from([0])]);

// "host:a2sport", "host" (default port) or nothing (127.0.0.1:17777).
export function parseAddress(text) {
  if (!text) return { host: DEFAULT_HOST, port: DEFAULT_A2S_PORT };
  const i = text.lastIndexOf(":");
  if (i < 0) return { host: text, port: DEFAULT_A2S_PORT };
  const host = text.slice(0, i) || DEFAULT_HOST;
  const portText = text.slice(i + 1);
  const port = Number(portText);
  if (!/^\d+$/.test(portText) || port < 1 || port > 65535) throw new Error(`bad A2S port '${portText}' in '${text}'`);
  return { host, port };
}

export function buildInfoRequest(challenge) {
  return Buffer.concat([HEADER, INFO_PAYLOAD, challenge ?? Buffer.alloc(0)]);
}

// Reads NUL-terminated strings and bytes from a reply, throwing on truncation.
function reader(buf, start) {
  let pos = start;
  const need = (n) => {
    if (pos + n > buf.length) throw new Error("A2S reply is truncated");
  };
  return {
    byte() {
      need(1);
      return buf[pos++];
    },
    short() {
      need(2);
      const v = buf.readUInt16LE(pos);
      pos += 2;
      return v;
    },
    string() {
      const end = buf.indexOf(0, pos);
      if (end < 0) throw new Error("A2S reply is truncated");
      const s = buf.toString("utf8", pos, end);
      pos = end + 1;
      return s;
    },
  };
}

// { kind: "challenge", challenge } or { kind: "info", name, map, folder, game, players, maxPlayers, bots, ... }.
// Throws on anything else (bad header, split packet, unknown type, truncation).
export function parseInfoReply(buf) {
  if (buf.length < 5 || !buf.subarray(0, 4).equals(HEADER)) throw new Error("not an A2S reply (bad header)");
  const type = buf[4];
  if (type === 0x41) {
    if (buf.length < 9) throw new Error("A2S challenge is truncated");
    return { kind: "challenge", challenge: Buffer.from(buf.subarray(5, 9)) };
  }
  if (type !== 0x49) throw new Error(`unexpected A2S reply type 0x${type.toString(16)}`);
  const r = reader(buf, 5);
  const protocol = r.byte();
  const name = r.string();
  const map = r.string();
  const folder = r.string();
  const game = r.string();
  const appId = r.short();
  const players = r.byte();
  const maxPlayers = r.byte();
  const bots = r.byte();
  // After bots: server type, environment, visibility (1 = password set), VAC. A short reply leaves
  // passworded undefined rather than failing the query.
  let passworded;
  try {
    r.byte();
    r.byte();
    passworded = r.byte() === 1;
  } catch {
    passworded = undefined;
  }
  return { kind: "info", protocol, name, map, folder, game, appId, players, maxPlayers, bots, passworded };
}

// One UDP query with the challenge step: the whole exchange shares one timeoutMs budget.
// Rejects with "A2S query to host:port timed out after Nms" when nothing complete arrives.
export function queryInfo(host, port, { timeoutMs = TIMEOUT_MS } = {}) {
  return new Promise((resolve, reject) => {
    const sock = createSocket("udp4");
    let done = false;
    let challenged = false;
    const finish = (err, value) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      try {
        sock.close();
      } catch {
        // already closed
      }
      if (err) reject(err);
      else resolve(value);
    };
    const timer = setTimeout(() => finish(new Error(`A2S query to ${host}:${port} timed out after ${timeoutMs}ms`)), timeoutMs);
    const send = (challenge) => sock.send(buildInfoRequest(challenge), port, host, (err) => err && finish(err));
    sock.on("error", (err) => finish(err));
    sock.on("message", (msg) => {
      let reply;
      try {
        reply = parseInfoReply(msg);
      } catch (e) {
        finish(e);
        return;
      }
      if (reply.kind === "info") finish(null, reply);
      else if (challenged) finish(new Error("A2S server challenged twice"));
      else {
        challenged = true;
        send(reply.challenge);
      }
    });
    send();
  });
}
