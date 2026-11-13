// tools/discord-mcp/relay-filter.mjs - which Discord gateway events the welcome relay forwards.
//
// Pure functions, no I/O, so `tools\cli discord selftest` can prove the rules.
// The relay forwards exactly five things; the first three go to the webhook that wakes the
// orchestrator, the last two to the feedback webhook (the caller routes on the type prefix):
//   member_join      a human joined the guild
//   welcome_message  a human posted in #welcome
//   dm_message       a human DMed the bot
//   feedback_post    a human's starter message of a new thread in the #feedback forum
//   feedback_reply   a human's later message in such a thread
// Everything else is dropped: bots and webhooks (so the bot never answers itself or
// another bot), the bot's own messages, other channels, other guilds, system messages.

// Message types worth forwarding: 0 = default, 19 = reply. Joins (7), pins (6), thread
// starters and the like are system messages and carry no question to answer.
const FORWARD_MESSAGE_TYPES = new Set([0, 19]);

const MAX_CONTENT = 1500;

// The #feedback forum channel (type 15). Its threads are the posts.
export const FEEDBACK_FORUM_ID = "1568938441637499080";

// Thread bookkeeping lives in ctx.threads, a Map(threadId -> { parentId, name }) the caller
// owns. Tracking frames (GUILD_CREATE, THREAD_*) update it and forward nothing.
function trackThreads(t, d, ctx) {
  if (!ctx.threads) return;
  const put = (th) => {
    if (th?.id && th.parent_id) ctx.threads.set(th.id, { parentId: th.parent_id, name: String(th.name ?? "") });
  };
  if (t === "GUILD_CREATE") {
    if (d.id !== ctx.guildId) return;
    for (const th of d.threads ?? []) put(th);
  } else if (t === "THREAD_LIST_SYNC") {
    if (d.guild_id !== ctx.guildId) return;
    for (const th of d.threads ?? []) put(th);
  } else if (t === "THREAD_CREATE" || t === "THREAD_UPDATE") {
    if (d.guild_id !== ctx.guildId) return;
    put(d);
  } else if (t === "THREAD_DELETE") {
    ctx.threads.delete(d.id);
  }
}

function isHuman(author, selfId) {
  if (!author || !author.id) return false;
  if (author.bot) return false;
  if (selfId && author.id === selfId) return false;
  return true;
}

function who(user) {
  return { id: user.id, username: user.username };
}

// dispatch: one gateway frame ({ t, d }). ctx: { guildId, welcomeId, selfId }.
// Returns the webhook payload, or null when the event is not forwarded.
export function classifyEvent(dispatch, ctx, now = Date.now()) {
  const t = dispatch?.t;
  const d = dispatch?.d;
  if (!t || !d) return null;

  trackThreads(t, d, ctx);

  if (t === "GUILD_MEMBER_ADD") {
    if (d.guild_id !== ctx.guildId) return null;
    if (!isHuman(d.user, ctx.selfId)) return null;
    return { id: `join-${d.user.id}-${now}`, type: "member_join", user: who(d.user), channelId: ctx.welcomeId };
  }

  if (t === "MESSAGE_CREATE") {
    if (d.webhook_id) return null;
    if (!isHuman(d.author, ctx.selfId)) return null;
    if (d.type !== undefined && !FORWARD_MESSAGE_TYPES.has(d.type)) return null;
    const thread = d.guild_id === ctx.guildId ? ctx.threads?.get(d.channel_id) : null;
    if (thread && thread.parentId === FEEDBACK_FORUM_ID) {
      return {
        id: d.id,
        type: d.id === d.channel_id ? "feedback_post" : "feedback_reply",
        threadId: d.channel_id,
        threadName: thread.name,
        messageId: d.id,
        user: who(d.author),
        content: String(d.content ?? "").slice(0, MAX_CONTENT),
        attachments: (d.attachments ?? []).map((a) => ({ url: a.url, filename: a.filename })),
      };
    }
    const isDm = !d.guild_id;
    const inWelcome = d.guild_id === ctx.guildId && !!ctx.welcomeId && d.channel_id === ctx.welcomeId;
    if (!isDm && !inWelcome) return null;
    return {
      id: d.id,
      type: isDm ? "dm_message" : "welcome_message",
      user: who(d.author),
      channelId: isDm ? d.channel_id : ctx.welcomeId,
      messageId: d.id,
      content: String(d.content ?? "").slice(0, MAX_CONTENT),
    };
  }

  return null;
}
