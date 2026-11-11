// tools/discord-mcp/relay-filter.mjs - which Discord gateway events the welcome relay forwards.
//
// Pure functions, no I/O, so `tools\cli discord selftest` can prove the rules.
// The relay forwards exactly three things to the webhook that wakes the orchestrator:
//   member_join      a human joined the guild
//   welcome_message  a human posted in #welcome
//   dm_message       a human DMed the bot
// Everything else is dropped: bots and webhooks (so the bot never answers itself or
// another bot), the bot's own messages, other channels, other guilds, system messages.

// Message types worth forwarding: 0 = default, 19 = reply. Joins (7), pins (6), thread
// starters and the like are system messages and carry no question to answer.
const FORWARD_MESSAGE_TYPES = new Set([0, 19]);

const MAX_CONTENT = 1500;

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

  if (t === "GUILD_MEMBER_ADD") {
    if (d.guild_id !== ctx.guildId) return null;
    if (!isHuman(d.user, ctx.selfId)) return null;
    return { id: `join-${d.user.id}-${now}`, type: "member_join", user: who(d.user), channelId: ctx.welcomeId };
  }

  if (t === "MESSAGE_CREATE") {
    if (d.webhook_id) return null;
    if (!isHuman(d.author, ctx.selfId)) return null;
    if (d.type !== undefined && !FORWARD_MESSAGE_TYPES.has(d.type)) return null;
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
