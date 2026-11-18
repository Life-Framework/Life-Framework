import { readFileSync } from "node:fs";
import { join } from "node:path";
import { clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #2674: how long one string RPC argument can be on a dedicated server with a client.
//   rpc-string-proof [--sizes 1024,8192,...]   on `session up --clients 1`
// For each size, client 1 runs `rpcstring <chars> down` (the server sends that many characters in one
// owner RPC) and `rpcstring <chars> up` (the client sends them in one server RPC). Each line says what
// was sent and what arrived (length and hash); a reply that never comes is LOST and ends that direction,
// since the connection may be gone. It ends with a table for docs/architecture/enforce-string-limits.md.
const DEFAULT_SIZES = [1024, 4096, 8191, 8192, 8193, 16384, 32768, 65536, 131072, 262144];

function logText(logsDir) {
  const file = newestLog(logsDir);
  return file ? readFileSync(file, "utf8") : "";
}

async function runRpcStringProof(args) {
  const i = args.indexOf("--sizes");
  const sizes = i >= 0 ? String(args[i + 1]).split(",").map(Number).filter((n) => n > 0) : DEFAULT_SIZES;
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  h.setBoxes([clientBox(dir, 1)]);
  const send = (spec, timeout) => {
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };

  const rows = [];
  const lostAt = { down: 0, up: 0 };
  for (const chars of sizes) {
    const row = { chars };
    for (const way of ["down", "up"]) {
      if (lostAt[way]) {
        row[way] = "skipped";
        continue;
      }
      const r = send({ verb: "rpcstring", words: [String(chars), way] }, 30000);
      const m = /sent=(\d+) got=(\d+) hash=(\w+)/.exec(r.detail ?? "");
      if (r.status === "ok" && m) {
        const whole = Number(m[1]) === chars && Number(m[2]) === chars && m[3] === "ok";
        row[way] = whole ? "whole" : `CUT to ${m[2]} (hash ${m[3]})`;
      } else {
        row[way] = `LOST (${r.status} ${r.detail ?? ""})`.trim();
        lostAt[way] = chars;
      }
    }
    rows.push(row);
  }

  const alive = send({ verb: "ui" }, 10000); // needs no character: the client may still be at the quiz
  console.log(`client 1 after the probe: ${alive.status === "ok" ? "still connected" : `not answering (${alive.status} ${alive.detail})`}`);

  const paths = sessionPaths(dir, 1);
  const pick = (text) => text.split("\n").filter((l) => l.includes("[LFDebug:RpcProbe]") || (/rpl|replication|packet|too (large|big)|overflow|disconnect/i.test(l) && /\((E|W)\)/.test(l)));
  for (const [who, logs] of [["server", join(paths.server, "logs")], ["client 1", paths.clients[0].logs]]) {
    console.log(`${who} lines:`);
    for (const l of pick(logText(logs)).slice(-40)) console.log(`  ${l.trim()}`);
  }

  console.log("");
  console.log("| Chars | Server to client (owner RPC) | Client to server (server RPC) |");
  console.log("|---|---|---|");
  for (const r of rows) console.log(`| ${r.chars} | ${r.down} | ${r.up} |`);
  const cut = rows.some((r) => /CUT/.test(r.down) || /CUT/.test(r.up));
  console.log(`rpc string proof: done (${lostAt.down || lostAt.up || cut ? "a size was cut or lost, see the table" : "every size arrived whole"})`);
  return 0;
}

export default {
  name: "rpc-string-proof",
  usage: "rpc-string-proof [--sizes 1024,8192,65536]",
  run: (ctx) => runRpcStringProof(ctx.args ?? []),
};
