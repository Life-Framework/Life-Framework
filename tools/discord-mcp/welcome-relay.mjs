// tools/discord-mcp/welcome-relay.mjs - the Discord welcome relay.
//
// A gateway listener (built-in WebSocket, no dependencies, Node 22+). It forwards member
// joins, #welcome messages and DMs to the bot to a webhook that wakes the orchestrator,
// which answers as the AI-labelled bot (docs/community/bot-playbook.md; say.mjs and dm.mjs
// post the replies). Which events count is decided in relay-filter.mjs.
//
// Run it with `tools\cli discord relay start`; run directly (`node welcome-relay.mjs`) for
// a foreground session. Secrets come from ~/.life-framework/ (secrets.mjs), never the repo.
// Intents: GUILDS, GUILD_MEMBERS, GUILD_MESSAGES, DIRECT_MESSAGES, MESSAGE_CONTENT. GUILD_MEMBERS
// and MESSAGE_CONTENT are privileged: switch on Server Members and Message Content in the
// Developer Portal (the gateway closes with code 4014 otherwise).

import { GUILD_ID, readHookUrl, readToken } from "./secrets.mjs";
import { classifyEvent } from "./relay-filter.mjs";

const token = readToken();
const hook = readHookUrl();
const log = (...a) => console.log(new Date().toISOString(), ...a);
const api = (path) =>
  fetch(`https://discord.com/api/v10${path}`, { headers: { Authorization: `Bot ${token}` } }).then((r) => r.json());

const channels = await api(`/guilds/${GUILD_ID}/channels`);
const welcome = Array.isArray(channels) ? channels.find((c) => c.name === "welcome") : null;
if (!welcome) {
  log("no #welcome channel found (bad token, wrong guild, or the bot cannot see it)");
  process.exit(1);
}
log("welcome channel", welcome.id);

const INTENTS = (1 << 0) | (1 << 1) | (1 << 9) | (1 << 12) | (1 << 15);
let selfId = null;
let seq = null;
let heartbeat = null;

function forward(body) {
  return fetch(hook, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) })
    .then((r) => log("hook", body.type, r.status))
    .catch((e) => log("hook error", e.message));
}

function connect() {
  const ws = new WebSocket("wss://gateway.discord.gg/?v=10&encoding=json");
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.s) seq = m.s;
    if (m.op === 10) {
      clearInterval(heartbeat);
      heartbeat = setInterval(() => ws.send(JSON.stringify({ op: 1, d: seq })), m.d.heartbeat_interval);
      ws.send(
        JSON.stringify({
          op: 2,
          d: { token, intents: INTENTS, properties: { os: "windows", browser: "lf-relay", device: "lf-relay" } },
        }),
      );
    }
    if (m.t === "READY") {
      selfId = m.d.user.id;
      log("ready as", m.d.user.username);
    }
    const body = classifyEvent(m, { guildId: GUILD_ID, welcomeId: welcome.id, selfId });
    if (body) forward(body);
  };
  ws.onclose = (ev) => {
    clearInterval(heartbeat);
    log("closed", ev.code, ev.reason);
    if (ev.code === 4014) {
      log("privileged intents (Server Members / Message Content) are not enabled in the Developer Portal");
      process.exit(2);
    }
    setTimeout(connect, 5000);
  };
}

connect();
