// Proves the Discord welcome relay's event filter (tools/discord-mcp/relay-filter.mjs, #696).
// Run it with `tools\cli discord selftest`. It forwards only:
//   1. a human joining the guild;
//   2. a human's message in #welcome;
//   3. a human's DM to the bot;
// and drops bots and webhooks, the bot itself, other channels, other guilds, system messages.
// red-proof: with `if (d.author.bot)` removed from isHuman() it printed
// "bot join forwarded" and "bot DM forwarded".
import { classifyEvent } from "../discord-mcp/relay-filter.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const ctx = { guildId: "G1", welcomeId: "W1", selfId: "BOT" };
const human = { id: "U1", username: "ana" };
const bot = { id: "U2", username: "somebot", bot: true };
const self = { id: "BOT", username: "Life Framework (AI)", bot: true };
const join = (user, guild = "G1") => ({ t: "GUILD_MEMBER_ADD", d: { guild_id: guild, user } });
const msg = (author, over = {}) => ({
  t: "MESSAGE_CREATE",
  d: { id: "M1", channel_id: "C9", guild_id: "G1", author, content: "hi", type: 0, ...over },
});
const dm = (author, over = {}) => msg(author, { guild_id: undefined, channel_id: "DM1", ...over });

// joins
const j = classifyEvent(join(human), ctx, 5);
check(j?.type === "member_join" && j.user.id === "U1" && j.channelId === "W1" && j.id === "join-U1-5", "human join not forwarded as member_join");
check(!classifyEvent(join(bot), ctx), "bot join forwarded");
check(!classifyEvent(join(self), ctx), "self join forwarded");
check(!classifyEvent(join(human, "OTHER"), ctx), "join in another guild forwarded");

// #welcome
const w = classifyEvent(msg(human, { channel_id: "W1" }), ctx);
check(w?.type === "welcome_message" && w.channelId === "W1" && w.messageId === "M1" && w.content === "hi", "human #welcome message not forwarded as welcome_message");
check(classifyEvent(msg(human, { channel_id: "W1", type: 19 }), ctx)?.type === "welcome_message", "reply in #welcome dropped");
check(!classifyEvent(msg(human, { channel_id: "C9" }), ctx), "message in another channel forwarded");
check(!classifyEvent(msg(human, { channel_id: "W1", guild_id: "OTHER" }), ctx), "#welcome-id message from another guild forwarded");
check(!classifyEvent(msg(bot, { channel_id: "W1" }), ctx), "bot message in #welcome forwarded");
check(!classifyEvent(msg(self, { channel_id: "W1" }), ctx), "own message in #welcome forwarded");
check(!classifyEvent(msg(human, { channel_id: "W1", webhook_id: "H1" }), ctx), "webhook message in #welcome forwarded");
check(!classifyEvent(msg(human, { channel_id: "W1", type: 7 }), ctx), "system join message in #welcome forwarded");

// DMs
const d = classifyEvent(dm(human), ctx);
check(d?.type === "dm_message" && d.channelId === "DM1" && d.user.id === "U1", "human DM not forwarded as dm_message");
check(!classifyEvent(dm(bot), ctx), "bot DM forwarded");
check(!classifyEvent(dm(self), ctx), "own DM forwarded");
check(classifyEvent(dm(human, { content: "x".repeat(5000) }), ctx)?.content.length === 1500, "long DM not truncated to 1500");

// other frames, and the pre-READY window where the bot's own id is unknown
check(!classifyEvent({ t: "READY", d: { user: self } }, ctx), "READY forwarded");
check(!classifyEvent({ t: "TYPING_START", d: { guild_id: "G1", channel_id: "W1" } }, ctx), "TYPING_START forwarded");
check(!classifyEvent({ op: 11 }, ctx), "heartbeat ack forwarded");
check(!classifyEvent(null, ctx), "null frame forwarded");
check(!classifyEvent(msg({ id: "BOT", username: "x" }, { channel_id: "W1" }), ctx), "own id without bot flag forwarded");

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log("discord-relay-selftest: FAIL");
  process.exit(1);
}
console.log("discord-relay-selftest: OK (joins, #welcome and DMs forwarded; bots, webhooks, self, other channels and guilds, system messages dropped)");
