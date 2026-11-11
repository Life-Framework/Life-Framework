# Life Framework Discord connector (MCP)

A zero-dependency MCP server that lets Claude Code run the Life Framework
Discord server through the **Life Framework** bot. Node.js 22+; no `npm install`.

## Setup on a new machine

1. Developer Portal → Life Framework → Bot: copy the token (Reset Token), and
   enable **Server Members Intent** and **Message Content Intent**.
2. Save the token in a private file outside the repo, e.g.
   `%USERPROFILE%\.life-framework\discord-token` (readable only by you).
3. Register the connector (server ID of Life Framework: `1444353039430582324`):

   ```bash
   claude mcp add life-framework-discord --scope user \
     -e DISCORD_TOKEN_FILE=C:/Users/<you>/.life-framework/discord-token \
     -e DISCORD_GUILD_ID=1444353039430582324 \
     -- node C:/path/to/Life-Framework/tools/discord-mcp/index.mjs
   ```

   `DISCORD_TOKEN` (the token itself) also works, but it then sits in plain
   text in Claude's config; prefer the file.
4. Check with `claude mcp list` or `/mcp` in a new Claude Code session.

## Tools (36)

- Live activity: `get_new_events`, `wait_for_events` (messages, DMs to the bot,
  edits, deletions, joins/leaves, reactions, bans, threads).
- Messages: read, send/reply, DM, edit, delete, bulk-delete, pin, react, threads.
- Channels: list, create (public/private), edit, delete, per-channel permissions.
- Members and roles: list, search, nicknames, create/edit/delete/assign roles.
- Moderation: timeout, kick, ban, unban, audit log.
- Server: info, settings, invites, scheduled events, channel webhooks.

Optional: `EVENT_WEBHOOK_URL` POSTs each event as JSON; `EVENT_LOG_FILE`
appends each event as a JSON line.

## Welcome relay and reply helpers

`welcome-relay.mjs` forwards member joins, #welcome messages and DMs to a
webhook; `say.mjs` and `dm.mjs` post the bot's replies. The webhook URL is read
from `%USERPROFILE%\.life-framework\welcome-hook-url` (never the repo). Run it
with `tools\cli discord relay start|stop|status`. Full description:
`docs/community/onboarding.md`, "Welcome relay".

## Rules for agents

Kicking, banning, and deleting channels or roles need the maintainer's OK.
Public posts speak for the project: use the `unslop` skill and keep claims
honest about what is built.
