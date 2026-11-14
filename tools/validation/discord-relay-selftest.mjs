// Proves the Discord welcome relay's event filter (tools/discord-mcp/relay-filter.mjs, #696).
// inputs: tools/**, agent-box/**, .githooks/**
// Run it with `tools\cli discord selftest`. It forwards only:
//   1. a human joining the guild;
//   2. a human's message in #welcome;
//   3. a human's DM to the bot;
// and drops bots and webhooks, the bot itself, other channels, other guilds, system messages.
//   4. a human's new post and later replies in the #feedback forum (feedback_post / feedback_reply).
// red-proof: with `if (d.author.bot)` removed from isHuman() it printed
// "bot join forwarded" and "bot DM forwarded"; with the `thread.parentId === FEEDBACK_FORUM_ID` test removed it printed "thread under another channel forwarded".
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

// #feedback forum: threads tracked from THREAD_CREATE and GUILD_CREATE, posts and replies forwarded
const FORUM = "1568938441637499080";
const fctx = { ...ctx, threads: new Map() };
check(!classifyEvent({ t: "THREAD_CREATE", d: { id: "T1", guild_id: "G1", parent_id: FORUM, name: "Bug" } }, fctx), "THREAD_CREATE forwarded");
const post = classifyEvent(msg(human, { id: "T1", channel_id: "T1", attachments: [{ url: "u", filename: "a.png", size: 1 }] }), fctx);
check(post?.type === "feedback_post" && post.id === "T1" && post.threadId === "T1" && post.threadName === "Bug" && post.messageId === "T1" && post.user.id === "U1" && post.attachments[0].url === "u" && post.attachments[0].filename === "a.png" && Object.keys(post.attachments[0]).length === 2, "starter message not forwarded as feedback_post");
const rep = classifyEvent(msg(human, { id: "M2", channel_id: "T1" }), fctx);
check(rep?.type === "feedback_reply" && rep.id === "M2" && rep.threadId === "T1" && rep.attachments.length === 0, "thread message not forwarded as feedback_reply");
check(!classifyEvent(msg(bot, { id: "M3", channel_id: "T1" }), fctx), "bot reply in feedback thread forwarded");
check(!classifyEvent(msg(self, { id: "M4", channel_id: "T1" }), fctx), "own reply in feedback thread forwarded");
check(!classifyEvent(msg(human, { id: "M5", channel_id: "T1", webhook_id: "H1" }), fctx), "webhook message in feedback thread forwarded");
classifyEvent({ t: "THREAD_CREATE", d: { id: "T2", guild_id: "G1", parent_id: "OTHERFORUM", name: "x" } }, fctx);
check(!classifyEvent(msg(human, { id: "M6", channel_id: "T2" }), fctx), "thread under another channel forwarded");
check(!classifyEvent(msg(human, { id: "M7", channel_id: "T9" }), fctx), "message in unknown thread forwarded");
classifyEvent({ t: "GUILD_CREATE", d: { id: "G1", threads: [{ id: "T3", parent_id: FORUM, name: "Old" }] } }, fctx);
check(classifyEvent(msg(human, { id: "M8", channel_id: "T3" }), fctx)?.threadName === "Old", "GUILD_CREATE active thread not tracked");
check(!classifyEvent(msg(human, { id: "M9", channel_id: "T3", guild_id: "OTHER" }), fctx), "feedback thread id from another guild forwarded");
check(!classifyEvent(msg(human, { id: "M10", channel_id: "T1" }), ctx), "thread message forwarded without a thread map");

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log("discord-relay-selftest: FAIL");
  process.exit(1);
}
console.log("discord-relay-selftest: OK (joins, #welcome, DMs and #feedback forum posts and replies forwarded; bots, webhooks, self, other channels and guilds, system messages dropped)");
