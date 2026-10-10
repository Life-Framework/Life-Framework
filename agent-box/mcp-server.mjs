#!/usr/bin/env node
// Small MCP server for the agent box. Cursor and `tools/cli call` both speak
// to it: Cursor uses Content-Length frames, cli call uses one JSON object per line.

import {
  buildLine,
  cursorMcpConfig,
  formatPads,
  isMainCheckout,
  pads,
  playLayer,
  readFlow,
  resolveCapture,
  runFlow,
  sendCommand,
  statusText,
} from "./harness.mjs";

const tools = [
  {
    name: "box_status",
    description: "Toolchain, MCP dist, and agent-box profile paths on this machine.",
    inputSchema: { type: "object", properties: {} },
  },
  {
    name: "layer_list",
    description:
      "DebugWorld entities that own a player action, with layer file, action class, and coordinates. Scanned from the layer files.",
    inputSchema: { type: "object", properties: {} },
  },
  {
    name: "layer_play",
    description:
      "Stand beside a DebugWorld entity, perform its action (or actionClass) through the same CanBeShown/CanBePerformed/PerformAction checks the interaction key uses, then screenshot the UI.",
    inputSchema: {
      type: "object",
      properties: {
        entityName: { type: "string" },
        actionClass: { type: "string" },
      },
      required: ["entityName"],
    },
  },
  {
    name: "player_press",
    description:
      "Press one player key. F calls SCR_InteractionHandlerComponent.ActionPerform, the same listener as the interact key. WheelUp and WheelDown call ActionScroll. F10 opens the debug menu the way the debug key does.",
    inputSchema: {
      type: "object",
      properties: { key: { type: "string", enum: ["F", "WheelUp", "WheelDown", "F10"] } },
      required: ["key"],
    },
  },
  {
    name: "player_look",
    description:
      "List ScriptedUserActions within 4m whose CanBeShownScript is true. That is the list the interact key would offer.",
    inputSchema: { type: "object", properties: {} },
  },
  {
    name: "player_stand",
    description:
      "Move the local character to just beside a named world entity so a 3m action radius can succeed. The server resolves the name. This is placement, not a player input.",
    inputSchema: {
      type: "object",
      properties: { entityName: { type: "string" } },
      required: ["entityName"],
    },
  },
  {
    name: "player_use",
    description:
      "Perform a ScriptedUserAction the way the interaction key does: CanBeShownScript, CanBePerformedScript, then PerformAction. Requires a spawned character in play mode. Optional entity name searches 80m; otherwise the nearest match within 5m.",
    inputSchema: {
      type: "object",
      properties: {
        className: { type: "string", description: "Enforce class, for example EL_ShopAction." },
        entityName: { type: "string" },
      },
      required: ["className"],
    },
  },
  {
    name: "player_input",
    description:
      "Pulse one menu action for a single press: MenuBack, MenuUp, MenuDown, MenuLeft, MenuRight, MenuSelect, or EL_OpenDebugMenu. Movement actions are refused.",
    inputSchema: {
      type: "object",
      properties: { action: { type: "string" } },
      required: ["action"],
    },
  },
  {
    name: "player_open",
    description: "Toggle the debug menu with the same MenuManager calls as the debug key.",
    inputSchema: {
      type: "object",
      properties: { menu: { type: "string", enum: ["DebugMenu"] } },
      required: ["menu"],
    },
  },
  {
    name: "pillar_shot",
    description:
      "Screenshot a pillar. ui captures the current screen (open and navigate the menu first). world aims the camera at x y z, pins noon and Clear weather, and captures. script is not a screenshot; use pillar_script.",
    inputSchema: {
      type: "object",
      properties: {
        pillar: { type: "string", enum: ["ui", "world"] },
        name: { type: "string", description: "Capture name. For world, a DebugWorld entity name fills x y z when those are omitted. ui name debug_menu opens the debug menu first." },
        x: { type: "number" },
        y: { type: "number" },
        z: { type: "number" },
      },
      required: ["pillar", "name"],
    },
  },
  {
    name: "player_flow",
    description:
      "Run a named flow from agent-box/flows. A flow is the same steps a player takes: press F, scroll the action list, menu keys, then a UI or world screenshot. Known names include ui-debug-menu, ui-shop, and world-atm.",
    inputSchema: {
      type: "object",
      properties: { name: { type: "string" } },
      required: ["name"],
    },
  },
  {
    name: "pillar_script",
    description:
      "Script pillar. Without run, returns the command. With run=true, starts the fast logic suite. Refuses the main checkout.",
    inputSchema: {
      type: "object",
      properties: { run: { type: "boolean" } },
    },
  },
];

function textResult(text) {
  return { content: [{ type: "text", text }] };
}

function commandText(result) {
  const image = resolveCapture(result.detail);
  const lines = [`${result.status} ${result.detail}`, result.line];
  if (image) lines.push(`image: ${image}`);
  return textResult(lines.join("\n"));
}

function callTool(name, args) {
  const a = args ?? {};
  if (name === "box_status") {
    const servers = Object.keys(cursorMcpConfig().mcpServers).join(", ");
    return textResult(`${statusText()}\ncursor servers: ${servers}`);
  }
  if (name === "layer_list") {
    return textResult(formatPads());
  }
  if (name === "layer_play") {
    const played = playLayer(a.entityName, a.actionClass);
    return textResult(played.lines.join("\n"));
  }
  if (name === "player_press") {
    const keys = {
      F: { verb: "interact", step: "confirm" },
      WheelUp: { verb: "interact", step: "next" },
      WheelDown: { verb: "interact", step: "prev" },
      F10: { verb: "open", menu: "DebugMenu" },
    };
    const spec = keys[a.key];
    if (!spec) throw new Error("key must be F, WheelUp, WheelDown, or F10");
    const result = sendCommand(spec);
    return commandText(result);
  }
  if (name === "player_look") {
    return commandText(sendCommand({ verb: "look" }));
  }
  if (name === "player_stand") {
    return commandText(sendCommand({ verb: "stand", entityName: a.entityName }));
  }
  if (name === "player_use") {
    return commandText(sendCommand({ verb: "use", className: a.className, entityName: a.entityName }));
  }
  if (name === "player_input") {
    return commandText(sendCommand({ verb: "input", action: a.action }));
  }
  if (name === "player_open") {
    return commandText(sendCommand({ verb: "open", menu: a.menu }));
  }
  if (name === "player_flow") {
    const played = runFlow(readFlow(a.name));
    return textResult(played.lines.join("\n"));
  }
  if (name === "pillar_shot") {
    let spec = { verb: "capture", mode: a.pillar, name: a.name, x: a.x, y: a.y, z: a.z };
    if (a.pillar === "world" && (a.x === undefined || a.y === undefined || a.z === undefined)) {
      const pad = pads().find((item) => item.name === a.name);
      if (!pad) throw new Error(`unknown world target ${a.name}`);
      spec = { verb: "capture", mode: "world", name: pad.name, x: pad.x, y: pad.y, z: pad.z };
    }
    if (a.pillar === "ui" && a.name === "debug_menu") {
      const opened = sendCommand({ verb: "open", menu: "DebugMenu" });
      if (opened.status !== "ok") return textResult(`${opened.status} ${opened.detail}\n${opened.line}`);
    }
    buildLine(1, spec);
    const result = sendCommand(spec, 25000);
    return commandText(result);
  }
  if (name === "pillar_script") {
    if (!a.run) {
      return textResult(
        "node tools/cli.mjs test --tier fast\nPass run=true from a worktree. The main checkout is the world-editor copy and refuses the suite.",
      );
    }
    if (isMainCheckout()) {
      return textResult("error refused on the main checkout. Create a worktree with tools\\cli wt new <feature>.");
    }
    return textResult("run the suite from the shell: node tools/cli.mjs test --tier fast");
  }
  throw new Error(`unknown tool ${name}`);
}

function handle(msg) {
  if (!msg || msg.jsonrpc !== "2.0") return;
  if (msg.id === undefined) return;
  try {
    if (msg.method === "initialize") {
      reply(msg.id, {
        protocolVersion: "2024-11-05",
        capabilities: { tools: { listChanged: false } },
        serverInfo: { name: "life-agent-box", version: "0.1.0" },
      });
      return;
    }
    if (msg.method === "tools/list") {
      reply(msg.id, { tools });
      return;
    }
    if (msg.method === "tools/call") {
      reply(msg.id, callTool(msg.params?.name, msg.params?.arguments));
      return;
    }
    reply(msg.id, {});
  } catch (err) {
    send({ jsonrpc: "2.0", id: msg.id, error: { code: -32000, message: err.message } });
  }
}

let buffer = Buffer.alloc(0);

let framed = false;

function send(msg) {
  const json = JSON.stringify(msg);
  if (framed) {
    process.stdout.write(`Content-Length: ${Buffer.byteLength(json)}\r\n\r\n${json}`);
    return;
  }
  process.stdout.write(json + "\n");
}

function reply(id, result) {
  send({ jsonrpc: "2.0", id, result });
}

process.stdin.on("data", (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  while (buffer.length) {
    const asText = buffer.toString("utf8");
    const trimmedStart = asText.replace(/^\s+/, "");
    const skipped = asText.length - trimmedStart.length;
    if (skipped) buffer = buffer.subarray(skipped);
    if (!buffer.length) return;

    if (buffer[0] === 0x7b) {
      framed = false;
      const nl = buffer.indexOf(0x0a);
      if (nl === -1) return;
      const line = buffer.subarray(0, nl).toString("utf8").trim();
      buffer = buffer.subarray(nl + 1);
      if (!line) continue;
      try {
        handle(JSON.parse(line));
      } catch {
        // ignore partial noise
      }
      continue;
    }

    const headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd === -1) return;
    const header = buffer.subarray(0, headerEnd).toString("utf8");
    const match = header.match(/Content-Length:\s*(\d+)/i);
    if (!match) {
      buffer = buffer.subarray(headerEnd + 4);
      continue;
    }
    framed = true;
    const length = Number(match[1]);
    const start = headerEnd + 4;
    if (buffer.length < start + length) return;
    const body = buffer.subarray(start, start + length).toString("utf8");
    buffer = buffer.subarray(start + length);
    try {
      handle(JSON.parse(body));
    } catch {
      // ignore
    }
  }
});
