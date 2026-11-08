#!/usr/bin/env node
// Life Framework Discord MCP server
// Lets Claude Code manage one Discord server: channels, messages, members, roles,
// moderation, events — plus a live Gateway listener that captures new messages,
// joins, reactions etc. so Claude can check for and reply to them.
// Zero dependencies. Requires Node.js 22+ (built-in fetch + WebSocket).
//
// Env:
//   DISCORD_TOKEN       (required) bot token
//   DISCORD_GUILD_ID    (required) server ID
//   DISCORD_TOKEN_FILE  (optional) read the token from this file instead of DISCORD_TOKEN
//   EVENT_WEBHOOK_URL   (optional) every captured event is also POSTed here as JSON
//   EVENT_LOG_FILE      (optional) every captured event is appended here as a JSON line

import { appendFile } from "node:fs/promises";
import { readFileSync } from "node:fs";
import { createInterface } from "node:readline";

const TOKEN = process.env.DISCORD_TOKEN_FILE
  ? readFileSync(process.env.DISCORD_TOKEN_FILE, "utf8").trim()
  : process.env.DISCORD_TOKEN;
const GUILD = process.env.DISCORD_GUILD_ID;
const WEBHOOK = process.env.EVENT_WEBHOOK_URL;
const LOGFILE = process.env.EVENT_LOG_FILE;
const log = (...a) => console.error("[life-framework-discord]", ...a);
if (!TOKEN || !GUILD) { log("Set DISCORD_TOKEN (or DISCORD_TOKEN_FILE) and DISCORD_GUILD_ID."); process.exit(1); }
if (typeof WebSocket === "undefined") { log("Node.js 22+ is required."); process.exit(1); }

// ───────────────────────── Discord REST ─────────────────────────
const API = "https://discord.com/api/v10";
async function discord(method, path, body, reason) {
  for (let attempt = 0; attempt < 4; attempt++) {
    const headers = { Authorization: `Bot ${TOKEN}`, "User-Agent": "DiscordBot (life-framework-mcp, 1.0)" };
    if (body !== undefined) headers["Content-Type"] = "application/json";
    if (reason) headers["X-Audit-Log-Reason"] = encodeURIComponent(reason);
    const res = await fetch(API + path, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) });
    if (res.status === 429) {
      const d = await res.json().catch(() => ({}));
      await new Promise(r => setTimeout(r, (d.retry_after ?? 1) * 1000 + 100));
      continue;
    }
    if (res.status === 204) return { ok: true };
    const text = await res.text();
    const data = text ? JSON.parse(text) : {};
    if (!res.ok) throw new Error(`Discord API ${res.status}: ${JSON.stringify(data)}`);
    return data;
  }
  throw new Error("Rate limited by Discord; try again shortly.");
}
const hex = (h) => (h ? parseInt(String(h).replace("#", ""), 16) : undefined);
const CH = { text: 0, voice: 2, category: 4, announcement: 5, stage: 13, forum: 15 };
const chName = (t) => Object.keys(CH).find(k => CH[k] === t) ?? `type_${t}`;
const fmtMsg = (m) => ({
  id: m.id, channel_id: m.channel_id, author: m.author?.global_name ?? m.author?.username, author_id: m.author?.id,
  bot: !!m.author?.bot, time: m.timestamp, content: m.content,
  attachments: m.attachments?.length ? m.attachments.map(a => a.url) : undefined,
  reply_to: m.message_reference?.message_id, mentions_bot: botUser ? m.mentions?.some(u => u.id === botUser.id) : undefined,
});

// ───────────────────────── Gateway (live events) ─────────────────────────
// Intents: GUILDS, GUILD_MEMBERS, GUILD_MODERATION, GUILD_MESSAGES, GUILD_MESSAGE_REACTIONS,
//          DIRECT_MESSAGES, MESSAGE_CONTENT, GUILD_SCHEDULED_EVENTS
const INTENTS = 103943;
const MAX_EVENTS = 1000;
const events = []; // { seq, at, type, data }
let eventSeq = 0, botUser = null, gatewayStatus = "connecting", waiters = [];
const channelNames = new Map();

function pushEvent(type, data) {
  const ev = { seq: ++eventSeq, at: new Date().toISOString(), type, data };
  events.push(ev);
  if (events.length > MAX_EVENTS) events.shift();
  waiters.splice(0).forEach(w => w());
  if (LOGFILE) appendFile(LOGFILE, JSON.stringify(ev) + "\n").catch(e => log("log file:", e.message));
  if (WEBHOOK) fetch(WEBHOOK, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(ev) })
    .catch(e => log("webhook:", e.message));
}

function handleDispatch(t, d) {
  if (t === "READY") { botUser = d.user; gatewayStatus = `connected as ${d.user.username}`; log(gatewayStatus); return; }
  if (t === "GUILD_CREATE" && d.id === GUILD) { d.channels?.forEach(c => channelNames.set(c.id, c.name)); return; }
  if (t === "CHANNEL_CREATE" || t === "CHANNEL_UPDATE") channelNames.set(d.id, d.name);
  const inGuild = d.guild_id === GUILD;
  const isDM = (t === "MESSAGE_CREATE" && !d.guild_id);
  if (!inGuild && !isDM) return;
  const ch = (id) => channelNames.get(id);
  switch (t) {
    case "MESSAGE_CREATE":
      if (d.author?.id === botUser?.id) return; // ignore own messages
      return pushEvent(isDM ? "direct_message" : "message", { ...fmtMsg(d), channel: ch(d.channel_id) });
    case "MESSAGE_UPDATE":
      if (d.content === undefined || d.author?.id === botUser?.id) return;
      return pushEvent("message_edited", { ...fmtMsg(d), channel: ch(d.channel_id) });
    case "MESSAGE_DELETE": return pushEvent("message_deleted", { id: d.id, channel_id: d.channel_id, channel: ch(d.channel_id) });
    case "GUILD_MEMBER_ADD": return pushEvent("member_joined", { user_id: d.user.id, username: d.user.username, display: d.user.global_name });
    case "GUILD_MEMBER_REMOVE": return pushEvent("member_left", { user_id: d.user.id, username: d.user.username });
    case "MESSAGE_REACTION_ADD":
      if (d.user_id === botUser?.id) return;
      return pushEvent("reaction_added", { user_id: d.user_id, message_id: d.message_id, channel_id: d.channel_id, channel: ch(d.channel_id), emoji: d.emoji?.name });
    case "GUILD_BAN_ADD": return pushEvent("member_banned", { user_id: d.user.id, username: d.user.username });
    case "THREAD_CREATE": return pushEvent("thread_created", { id: d.id, name: d.name, parent_id: d.parent_id, owner_id: d.owner_id });
    case "GUILD_SCHEDULED_EVENT_USER_ADD": return pushEvent("event_interest", { event_id: d.guild_scheduled_event_id, user_id: d.user_id });
  }
}

const FATAL = new Set([4004, 4010, 4011, 4012, 4013, 4014]);
function connectGateway(delay = 0) {
  setTimeout(() => {
    let hb = null, seq = null, acked = true;
    const ws = new WebSocket("wss://gateway.discord.gg/?v=10&encoding=json");
    const send = (o) => ws.readyState === 1 && ws.send(JSON.stringify(o));
    ws.onmessage = ({ data }) => {
      const p = JSON.parse(data);
      if (p.s != null) seq = p.s;
      if (p.op === 10) {
        clearInterval(hb);
        hb = setInterval(() => { if (!acked) return ws.close(4000); acked = false; send({ op: 1, d: seq }); }, p.d.heartbeat_interval);
        send({ op: 2, d: { token: TOKEN, intents: INTENTS, properties: { os: process.platform, browser: "life-framework-mcp", device: "life-framework-mcp" } } });
      } else if (p.op === 11) acked = true;
      else if (p.op === 1) send({ op: 1, d: seq });
      else if (p.op === 7 || p.op === 9) ws.close(4000);
      else if (p.op === 0) { try { handleDispatch(p.t, p.d); } catch (e) { log("dispatch:", e.message); } }
    };
    ws.onclose = (e) => {
      clearInterval(hb);
      if (FATAL.has(e.code)) {
        gatewayStatus = e.code === 4014
          ? "disabled: enable Server Members + Message Content intents in the Developer Portal (Bot page)"
          : e.code === 4004 ? "disabled: invalid bot token" : `disabled: gateway close code ${e.code}`;
        return log(gatewayStatus);
      }
      gatewayStatus = "reconnecting"; connectGateway(5000);
    };
    ws.onerror = () => {};
  }, delay);
}

// ───────────────────────── Tools ─────────────────────────
const S = (type, description, extra = {}) => ({ type, description, ...extra });
const str = (d) => S("string", d), int = (d, x) => S("integer", d, x), bool = (d) => S("boolean", d);
const tools = [];
const tool = (name, description, props, required, fn) =>
  tools.push({ name, description, inputSchema: { type: "object", properties: props, required }, fn });

// Live events
tool("get_new_events",
  "Get Discord activity captured live since a cursor: new messages (incl. DMs to the bot), edits, deletions, member joins/leaves, reactions, bans, new threads. Pass the returned `cursor` next time to only get newer events. Use this to find things to reply to.",
  { since_cursor: int("Cursor from the previous call (0 = everything buffered)"), types: S("array", "Optional filter, e.g. [\"message\",\"member_joined\"]", { items: { type: "string" } }), limit: int("Max events (default 100)") },
  [], async ({ since_cursor = 0, types, limit = 100 }) => {
    let out = events.filter(e => e.seq > since_cursor && (!types?.length || types.includes(e.type)));
    const truncated = out.length > limit; out = out.slice(0, limit);
    return { gateway: gatewayStatus, cursor: out.length ? out[out.length - 1].seq : Math.max(since_cursor, 0), truncated, events: out };
  });

tool("wait_for_events",
  "Block until new Discord activity arrives after the cursor (or the timeout passes), then return it. Good for 'watch the server and respond as things come in'.",
  { since_cursor: int("Cursor from get_new_events / previous wait"), timeout_seconds: int("Max wait, 1–300 (default 60)"), types: S("array", "Optional type filter", { items: { type: "string" } }) },
  ["since_cursor"], async ({ since_cursor, timeout_seconds = 60, types }) => {
    const match = () => events.filter(e => e.seq > since_cursor && (!types?.length || types.includes(e.type)));
    const deadline = Date.now() + Math.min(Math.max(timeout_seconds, 1), 300) * 1000;
    while (!match().length && Date.now() < deadline)
      await new Promise(r => { const t = setTimeout(r, deadline - Date.now()); waiters.push(() => { clearTimeout(t); r(); }); });
    const out = match();
    return { gateway: gatewayStatus, cursor: out.length ? out[out.length - 1].seq : since_cursor, timed_out: !out.length, events: out };
  });

// Server
tool("get_server_info", "Overview of the Life Framework server and the bot's connection status.", {}, [], async () => {
  const g = await discord("GET", `/guilds/${GUILD}?with_counts=true`);
  return { id: g.id, name: g.name, description: g.description, members: g.approximate_member_count, online: g.approximate_presence_count,
    boost_tier: g.premium_tier, boosts: g.premium_subscription_count, verification_level: g.verification_level,
    rules_channel_id: g.rules_channel_id, system_channel_id: g.system_channel_id, bot_gateway: gatewayStatus, bot_user: botUser?.username };
});

tool("edit_server", "Change server name, description, welcome/system channel or verification level (0–4).",
  { name: str(""), description: str(""), system_channel_id: str("Channel for join messages"), verification_level: int("0 none – 4 highest") }, [],
  async (a) => { const g = await discord("PATCH", `/guilds/${GUILD}`, a); return { name: g.name }; });

// Channels
tool("list_channels", "List channels and categories with IDs, types, topics.", {}, [], async () => {
  const cs = await discord("GET", `/guilds/${GUILD}/channels`);
  cs.forEach(c => channelNames.set(c.id, c.name));
  return cs.sort((a, b) => a.position - b.position).map(c => ({ id: c.id, name: c.name, type: chName(c.type), category_id: c.parent_id ?? undefined, topic: c.topic || undefined }));
});

tool("create_channel", "Create a channel or category.",
  { name: str(""), type: S("string", "Channel type", { enum: Object.keys(CH) }), category_id: str("Parent category ID"), topic: str(""), private: bool("Hide from @everyone (bot keeps access)") },
  ["name"], async ({ name, type = "text", category_id, topic, private: priv }) => {
    const body = { name, type: CH[type], parent_id: category_id, topic };
    if (priv) {
      const me = botUser ?? await discord("GET", "/users/@me");
      body.permission_overwrites = [{ id: GUILD, type: 0, deny: String(1 << 10) }, { id: me.id, type: 1, allow: String((1 << 10) | (1 << 11) | (1 << 16)) }];
    }
    const c = await discord("POST", `/guilds/${GUILD}/channels`, body);
    return { id: c.id, name: c.name, type: chName(c.type) };
  });

tool("edit_channel", "Rename a channel, change topic, slowmode, or move it to a category.",
  { channel_id: str(""), name: str(""), topic: str(""), slowmode_seconds: int("0–21600"), category_id: str("") }, ["channel_id"],
  async ({ channel_id, name, topic, slowmode_seconds, category_id }) => {
    const c = await discord("PATCH", `/channels/${channel_id}`, { name, topic, rate_limit_per_user: slowmode_seconds, parent_id: category_id });
    return { id: c.id, name: c.name, topic: c.topic };
  });

tool("set_channel_permissions", "Allow/deny permissions for a role or member in a channel. Use permission names like VIEW_CHANNEL, SEND_MESSAGES.",
  { channel_id: str(""), target_id: str("Role ID or user ID (guild ID = @everyone)"), target_is_member: bool("true if target is a user"),
    allow: S("array", "Permission names to allow", { items: { type: "string" } }), deny: S("array", "Permission names to deny", { items: { type: "string" } }) },
  ["channel_id", "target_id"], async ({ channel_id, target_id, target_is_member, allow = [], deny = [] }) => {
    const bits = (names) => String(names.reduce((acc, n) => { const b = PERMS[n.toUpperCase()]; if (b === undefined) throw new Error(`Unknown permission ${n}`); return acc | (1n << BigInt(b)); }, 0n));
    await discord("PUT", `/channels/${channel_id}/permissions/${target_id}`, { type: target_is_member ? 1 : 0, allow: bits(allow), deny: bits(deny) });
    return "Permissions updated.";
  });

tool("delete_channel", "Permanently delete a channel. Confirm with the user first.", { channel_id: str(""), reason: str("") }, ["channel_id"],
  async ({ channel_id, reason }) => { await discord("DELETE", `/channels/${channel_id}`, undefined, reason); return "Channel deleted."; });

// Messages
tool("read_messages", "Read recent messages in a channel or thread (newest first).",
  { channel_id: str(""), limit: int("1–100, default 25"), before: str("Message ID to page backwards from") }, ["channel_id"],
  async ({ channel_id, limit = 25, before }) => {
    const q = new URLSearchParams({ limit: String(limit) }); if (before) q.set("before", before);
    return (await discord("GET", `/channels/${channel_id}/messages?${q}`)).map(fmtMsg);
  });

tool("send_message", "Post a message (optionally as a reply, optionally with an embed).",
  { channel_id: str(""), content: str("Up to 2000 chars"), reply_to_message_id: str(""), embed_title: str(""), embed_description: str(""), embed_color_hex: str("e.g. #4F7CAC") },
  ["channel_id"], async ({ channel_id, content, reply_to_message_id, embed_title, embed_description, embed_color_hex }) => {
    const body = { content, allowed_mentions: { parse: ["users", "roles"] } };
    if (reply_to_message_id) body.message_reference = { message_id: reply_to_message_id, fail_if_not_exists: false };
    if (embed_title || embed_description) body.embeds = [{ title: embed_title, description: embed_description, color: hex(embed_color_hex) }];
    const m = await discord("POST", `/channels/${channel_id}/messages`, body);
    return { id: m.id, channel_id: m.channel_id };
  });

tool("send_direct_message", "DM a member.", { user_id: str(""), content: str("") }, ["user_id", "content"], async ({ user_id, content }) => {
  const dm = await discord("POST", "/users/@me/channels", { recipient_id: user_id });
  const m = await discord("POST", `/channels/${dm.id}/messages`, { content });
  return { id: m.id, channel_id: dm.id };
});

tool("edit_message", "Edit a message the bot posted.", { channel_id: str(""), message_id: str(""), content: str("") }, ["channel_id", "message_id", "content"],
  async ({ channel_id, message_id, content }) => ({ id: (await discord("PATCH", `/channels/${channel_id}/messages/${message_id}`, { content })).id }));

tool("delete_message", "Delete a message.", { channel_id: str(""), message_id: str(""), reason: str("") }, ["channel_id", "message_id"],
  async ({ channel_id, message_id, reason }) => { await discord("DELETE", `/channels/${channel_id}/messages/${message_id}`, undefined, reason); return "Deleted."; });

tool("bulk_delete_messages", "Delete 2–100 messages (younger than 14 days) in one channel.",
  { channel_id: str(""), message_ids: S("array", "", { items: { type: "string" } }), reason: str("") }, ["channel_id", "message_ids"],
  async ({ channel_id, message_ids, reason }) => { await discord("POST", `/channels/${channel_id}/messages/bulk-delete`, { messages: message_ids }, reason); return `Deleted ${message_ids.length}.`; });

tool("pin_message", "Pin (or unpin) a message.", { channel_id: str(""), message_id: str(""), unpin: bool("") }, ["channel_id", "message_id"],
  async ({ channel_id, message_id, unpin }) => { await discord(unpin ? "DELETE" : "PUT", `/channels/${channel_id}/pins/${message_id}`); return unpin ? "Unpinned." : "Pinned."; });

tool("add_reaction", "React to a message with a unicode emoji.", { channel_id: str(""), message_id: str(""), emoji: str("e.g. 👍") }, ["channel_id", "message_id", "emoji"],
  async ({ channel_id, message_id, emoji }) => { await discord("PUT", `/channels/${channel_id}/messages/${message_id}/reactions/${encodeURIComponent(emoji)}/@me`); return "Reacted."; });

tool("create_thread", "Start a thread from a message, or a standalone thread in a text channel.", { channel_id: str(""), name: str(""), message_id: str("") }, ["channel_id", "name"],
  async ({ channel_id, name, message_id }) => {
    const t = await discord("POST", message_id ? `/channels/${channel_id}/messages/${message_id}/threads` : `/channels/${channel_id}/threads`, message_id ? { name } : { name, type: 11 });
    return { id: t.id, name: t.name };
  });

// Members & roles
tool("list_members", "List members with roles and join dates.", { limit: int("1–1000, default 100"), after: str("User ID to page after") }, [],
  async ({ limit = 100, after }) => {
    const q = new URLSearchParams({ limit: String(limit) }); if (after) q.set("after", after);
    return (await discord("GET", `/guilds/${GUILD}/members?${q}`)).map(m => ({ user_id: m.user.id, username: m.user.username, display: m.nick ?? m.user.global_name, bot: !!m.user.bot, roles: m.roles, joined: m.joined_at, timed_out_until: m.communication_disabled_until ?? undefined }));
  });

tool("search_members", "Find members by username/nickname prefix.", { query: str(""), limit: int("default 20") }, ["query"],
  async ({ query, limit = 20 }) => (await discord("GET", `/guilds/${GUILD}/members/search?query=${encodeURIComponent(query)}&limit=${limit}`))
    .map(m => ({ user_id: m.user.id, username: m.user.username, display: m.nick ?? m.user.global_name, roles: m.roles })));

tool("set_nickname", "Change a member's nickname (empty clears it).", { user_id: str(""), nickname: str("") }, ["user_id", "nickname"],
  async ({ user_id, nickname }) => { await discord("PATCH", `/guilds/${GUILD}/members/${user_id}`, { nick: nickname || null }); return "Nickname updated."; });

tool("list_roles", "List roles, highest first.", {}, [], async () => (await discord("GET", `/guilds/${GUILD}/roles`)).sort((a, b) => b.position - a.position)
  .map(r => ({ id: r.id, name: r.name, color: "#" + r.color.toString(16).padStart(6, "0"), hoist: r.hoist, mentionable: r.mentionable, managed: r.managed })));

tool("create_role", "Create a role.", { name: str(""), color_hex: str(""), hoist: bool("Show separately in member list"), mentionable: bool("") }, ["name"],
  async ({ name, color_hex, hoist = false, mentionable = false }) => { const r = await discord("POST", `/guilds/${GUILD}/roles`, { name, color: hex(color_hex) ?? 0, hoist, mentionable }); return { id: r.id, name: r.name }; });

tool("edit_role", "Rename/recolor a role.", { role_id: str(""), name: str(""), color_hex: str(""), hoist: bool(""), mentionable: bool("") }, ["role_id"],
  async ({ role_id, name, color_hex, hoist, mentionable }) => { const r = await discord("PATCH", `/guilds/${GUILD}/roles/${role_id}`, { name, color: hex(color_hex), hoist, mentionable }); return { id: r.id, name: r.name }; });

tool("delete_role", "Delete a role. Confirm with the user first.", { role_id: str("") }, ["role_id"],
  async ({ role_id }) => { await discord("DELETE", `/guilds/${GUILD}/roles/${role_id}`); return "Role deleted."; });

tool("add_role", "Give a member a role.", { user_id: str(""), role_id: str(""), reason: str("") }, ["user_id", "role_id"],
  async ({ user_id, role_id, reason }) => { await discord("PUT", `/guilds/${GUILD}/members/${user_id}/roles/${role_id}`, undefined, reason); return "Role added."; });

tool("remove_role", "Remove a role from a member.", { user_id: str(""), role_id: str(""), reason: str("") }, ["user_id", "role_id"],
  async ({ user_id, role_id, reason }) => { await discord("DELETE", `/guilds/${GUILD}/members/${user_id}/roles/${role_id}`, undefined, reason); return "Role removed."; });

// Moderation
tool("timeout_member", "Time out a member for N minutes (0 removes the timeout; max 40320 = 28 days).", { user_id: str(""), minutes: int(""), reason: str("") }, ["user_id", "minutes"],
  async ({ user_id, minutes, reason }) => {
    const until = minutes > 0 ? new Date(Date.now() + minutes * 60000).toISOString() : null;
    await discord("PATCH", `/guilds/${GUILD}/members/${user_id}`, { communication_disabled_until: until }, reason);
    return minutes > 0 ? `Timed out until ${until}.` : "Timeout removed.";
  });

tool("kick_member", "Kick a member. Confirm with the user first.", { user_id: str(""), reason: str("") }, ["user_id"],
  async ({ user_id, reason }) => { await discord("DELETE", `/guilds/${GUILD}/members/${user_id}`, undefined, reason); return "Kicked."; });

tool("ban_member", "Ban a user. Confirm with the user first.", { user_id: str(""), reason: str(""), delete_message_hours: int("0–168") }, ["user_id"],
  async ({ user_id, reason, delete_message_hours = 0 }) => { await discord("PUT", `/guilds/${GUILD}/bans/${user_id}`, { delete_message_seconds: delete_message_hours * 3600 }, reason); return "Banned."; });

tool("unban_member", "Lift a ban.", { user_id: str(""), reason: str("") }, ["user_id"],
  async ({ user_id, reason }) => { await discord("DELETE", `/guilds/${GUILD}/bans/${user_id}`, undefined, reason); return "Unbanned."; });

tool("get_audit_log", "Recent audit log entries.", { limit: int("default 25") }, [],
  async ({ limit = 25 }) => (await discord("GET", `/guilds/${GUILD}/audit-logs?limit=${limit}`)).audit_log_entries
    .map(e => ({ id: e.id, action_type: e.action_type, by: e.user_id, target: e.target_id, reason: e.reason })));

// Invites, events, webhooks
tool("create_invite", "Create an invite link.", { channel_id: str(""), max_age_hours: S("number", "0 = never expires, default 24"), max_uses: int("0 = unlimited") }, ["channel_id"],
  async ({ channel_id, max_age_hours = 24, max_uses = 0 }) => `https://discord.gg/${(await discord("POST", `/channels/${channel_id}/invites`, { max_age: Math.round(max_age_hours * 3600), max_uses, unique: true })).code}`);

tool("list_events", "List scheduled events.", {}, [], async () => (await discord("GET", `/guilds/${GUILD}/scheduled-events?with_user_count=true`))
  .map(e => ({ id: e.id, name: e.name, start: e.scheduled_start_time, end: e.scheduled_end_time, interested: e.user_count, channel_id: e.channel_id, location: e.entity_metadata?.location })));

tool("create_event", "Schedule an event in a voice channel or at an external location/link.",
  { name: str(""), description: str(""), start_time: str("ISO 8601, e.g. 2026-11-15T18:00:00+01:00"), end_time: str("Required for external events"), voice_channel_id: str(""), location: str("e.g. a Zoom link") },
  ["name", "start_time"], async ({ name, description, start_time, end_time, voice_channel_id, location }) => {
    const body = { name, description, scheduled_start_time: start_time, scheduled_end_time: end_time, privacy_level: 2 };
    Object.assign(body, voice_channel_id ? { channel_id: voice_channel_id, entity_type: 2 } : { entity_type: 3, entity_metadata: { location: location ?? "Online" } });
    const e = await discord("POST", `/guilds/${GUILD}/scheduled-events`, body); return { id: e.id, name: e.name };
  });

tool("create_channel_webhook", "Create an incoming webhook for a channel (lets other tools post into it). Returns the webhook URL — treat it as a secret.",
  { channel_id: str(""), name: str("") }, ["channel_id", "name"],
  async ({ channel_id, name }) => { const w = await discord("POST", `/channels/${channel_id}/webhooks`, { name }); return { id: w.id, url: `https://discord.com/api/webhooks/${w.id}/${w.token}` }; });

const PERMS = { CREATE_INSTANT_INVITE: 0, KICK_MEMBERS: 1, BAN_MEMBERS: 2, ADMINISTRATOR: 3, MANAGE_CHANNELS: 4, MANAGE_GUILD: 5, ADD_REACTIONS: 6,
  VIEW_AUDIT_LOG: 7, PRIORITY_SPEAKER: 8, STREAM: 9, VIEW_CHANNEL: 10, SEND_MESSAGES: 11, SEND_TTS_MESSAGES: 12, MANAGE_MESSAGES: 13, EMBED_LINKS: 14,
  ATTACH_FILES: 15, READ_MESSAGE_HISTORY: 16, MENTION_EVERYONE: 17, USE_EXTERNAL_EMOJIS: 18, CONNECT: 20, SPEAK: 21, MUTE_MEMBERS: 22,
  DEAFEN_MEMBERS: 23, MOVE_MEMBERS: 24, CHANGE_NICKNAME: 26, MANAGE_NICKNAMES: 27, MANAGE_ROLES: 28, MANAGE_WEBHOOKS: 29,
  MANAGE_EVENTS: 33, MANAGE_THREADS: 34, CREATE_PUBLIC_THREADS: 35, CREATE_PRIVATE_THREADS: 36, SEND_MESSAGES_IN_THREADS: 38, MODERATE_MEMBERS: 40 };

// ───────────────────────── MCP over stdio (JSON-RPC 2.0) ─────────────────────────
const out = (msg) => process.stdout.write(JSON.stringify(msg) + "\n");
const rl = createInterface({ input: process.stdin });
rl.on("line", async (line) => {
  if (!line.trim()) return;
  let req; try { req = JSON.parse(line); } catch { return; }
  const { id, method, params } = req;
  if (id === undefined) return; // notification
  try {
    let result;
    if (method === "initialize") result = {
      protocolVersion: params?.protocolVersion ?? "2025-06-18",
      capabilities: { tools: {} },
      serverInfo: { name: "life-framework-discord", version: "1.0.0" },
      instructions: "Manages the Life Framework Discord server. Use get_new_events / wait_for_events to see new messages and activity, then reply with send_message (reply_to_message_id). Ask the user before kicking, banning, or deleting channels/roles.",
    };
    else if (method === "tools/list") result = { tools: tools.map(({ fn, ...t }) => t) };
    else if (method === "tools/call") {
      const t = tools.find(t => t.name === params.name);
      if (!t) throw Object.assign(new Error(`Unknown tool ${params.name}`), { code: -32602 });
      try {
        const r = await t.fn(params.arguments ?? {});
        result = { content: [{ type: "text", text: typeof r === "string" ? r : JSON.stringify(r, null, 2) }] };
      } catch (e) { result = { isError: true, content: [{ type: "text", text: String(e.message ?? e) }] }; }
    }
    else if (method === "ping") result = {};
    else throw Object.assign(new Error(`Method not found: ${method}`), { code: -32601 });
    out({ jsonrpc: "2.0", id, result });
  } catch (e) { out({ jsonrpc: "2.0", id, error: { code: e.code ?? -32603, message: e.message } }); }
});
rl.on("close", () => process.exit(0));

connectGateway();
