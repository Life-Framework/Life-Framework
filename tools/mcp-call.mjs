#!/usr/bin/env node
// tools/mcp-call.mjs - call an MCP server tool from the command line.
//
// Lets agents (and humans) use the Enfusion MCP servers without opencode's MCP
// plumbing being loaded in the current session. Speaks JSON-RPC to the server
// over stdio, exactly like opencode does.
//
// Usage:
//   node tools/mcp-call.mjs list [server]
//   node tools/mcp-call.mjs <tool> '<json args>' [server]
//   node tools/mcp-call.mjs <tool> @path/to/args.json [server]
//
// Examples:
//   node tools/mcp-call.mjs list
//   node tools/mcp-call.mjs api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
//   node tools/mcp-call.mjs game_read @tmp/args.json
//
// Notes:
//   - The default server is the first ENABLED entry in opencode.json. Pass a
//     name as the last argument to pick a specific one.
//   - Argument JSON with special characters is safest passed as a file: write
//     the JSON to a temp file, pass `@path`. PowerShell mangles inline JSON.
//   - Server command + environment are read from opencode.json, so this works
//     from any checkout that has the server installed.

import { spawn } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const CONFIG_PATH = join(ROOT, "opencode.json");

function readConfig() {
  try {
    return JSON.parse(readFileSync(CONFIG_PATH, "utf8"));
  } catch {
    return { mcp: {} };
  }
}

function pickServer(wanted) {
  const servers = readConfig().mcp ?? {};
  const names = Object.keys(servers);
  if (wanted) {
    if (!servers[wanted]) {
      console.error(`unknown MCP server: ${wanted} (have: ${names.join(", ") || "none"})`);
      return null;
    }
    return [wanted, servers[wanted]];
  }
  const enabled = names.filter((n) => servers[n].enabled);
  if (enabled.length === 0) {
    console.error(`no enabled MCP servers in ${CONFIG_PATH}`);
    return null;
  }
  const name = enabled[0];
  return [name, servers[name]];
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.log(`usage:
  node tools/mcp-call.mjs list [server]
  node tools/mcp-call.mjs <tool> '<json args>' [server]
  node tools/mcp-call.mjs <tool> @path/to/args.json [server]`);
    return 0;
  }

  const first = args[0];
  let toolName = first;
  let rawArgs = null;
  let wantedServer = null;

  if (first === "list") {
    toolName = null;
    wantedServer = args[1] ?? null;
  } else {
    rawArgs = args[1] ?? null;
    wantedServer = args[2] ?? null;
  }

  const [serverName, serverCfg] = pickServer(wantedServer);
  if (!serverCfg) return 1;

  const cmd = serverCfg.command;
  if (!Array.isArray(cmd) || cmd.length < 2 || cmd[0] !== "node") {
    console.error(`server ${serverName}: command must be [node, <entry>]`);
    return 1;
  }
  const entry = cmd[1];
  if (!existsSync(entry)) {
    console.error(`server ${serverName}: entry not found: ${entry}`);
    console.error(`  run: tools\\cli mcp install ${serverName}`);
    return 1;
  }
  const env = { ...process.env, ...(serverCfg.environment ?? {}) };

  let toolArgs = {};
  if (rawArgs) {
    if (rawArgs.startsWith("@")) {
      const file = rawArgs.slice(1);
      if (!existsSync(file)) {
        console.error(`args file not found: ${file}`);
        return 1;
      }
      try {
        toolArgs = JSON.parse(readFileSync(file, "utf8"));
      } catch (e) {
        console.error(`bad JSON in args file ${file}: ${e.message}`);
        return 1;
      }
    } else {
      try {
        toolArgs = JSON.parse(rawArgs);
      } catch (e) {
        console.error(`bad inline JSON: ${e.message}`);
        console.error(`  write the args to a file and pass @path instead (PowerShell mangles inline JSON)`);
        return 1;
      }
    }
  }

  const child = spawn("node", [entry], {
    env,
    stdio: ["pipe", "pipe", "pipe"],
  });

  let buf = "";
  const pending = new Map();
  let nextId = 1;

  function send(msg) {
    child.stdin.write(JSON.stringify(msg) + "\n");
  }

  function rpc(method, params) {
    const id = nextId++;
    return new Promise((resolve, reject) => {
      pending.set(id, { resolve, reject });
      send({ jsonrpc: "2.0", id, method, params });
    });
  }

  child.stdout.on("data", (d) => {
    buf += d.toString();
    let idx;
    while ((idx = buf.indexOf("\n")) !== -1) {
      const line = buf.slice(0, idx).trim();
      buf = buf.slice(idx + 1);
      if (!line) continue;
      let msg;
      try {
        msg = JSON.parse(line);
      } catch {
        continue;
      }
      if (msg.id && pending.has(msg.id)) {
        const p = pending.get(msg.id);
        pending.delete(msg.id);
        if (msg.error) p.reject(new Error(msg.error.message));
        else p.resolve(msg.result);
      }
    }
  });

  child.stderr.on("data", (d) => {
    const s = d.toString().trim();
    if (s) process.stderr.write(`[${serverName}] ${s}\n`);
  });

  child.on("exit", (code) => {
    for (const p of pending.values()) p.reject(new Error(`server exited: ${code}`));
    process.exit(code ?? 0);
  });

  try {
    await rpc("initialize", {
      protocolVersion: "2024-11-05",
      capabilities: {},
      clientInfo: { name: "mcp-call", version: "1.0.0" },
    });
    send({ jsonrpc: "2.0", method: "notifications/initialized" });

    if (toolName === null) {
      const res = await rpc("tools/list", {});
      for (const t of res.tools) {
        console.log(`${t.name}\t${(t.description || "").split("\n")[0].slice(0, 140)}`);
      }
    } else {
      const res = await rpc("tools/call", { name: toolName, arguments: toolArgs });
      for (const c of res.content || []) {
        if (c.type === "text") console.log(c.text);
        else console.log(JSON.stringify(c));
      }
      if (res.isError) {
        process.stderr.write(`tool ${toolName} reported an error\n`);
        child.kill();
        return 1;
      }
    }
    child.kill();
    return 0;
  } catch (e) {
    console.error(`MCP error: ${e.message}`);
    child.kill();
    return 1;
  }
}

process.exit(await main());