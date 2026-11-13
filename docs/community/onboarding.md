# Discord onboarding

What a new member sees in their first minute, and the texts and server settings
behind it. Issue: #569. The server layout and moderation are in
[discord.md](discord.md); how the bot behaves is in the [bot playbook](bot-playbook.md).

Status 2026-11-10: **the texts are written; the server is not changed yet.** The
maintainer (or a staff member) applies the steps under "Server changes" and then
updates the "Status" lines in `discord.md`. Nothing below is live until then.

People arrive from other communities and from the website. The first minute
decides whether they stay, so it says three things: what this is, how far along
it is, and what to do next. It does not sell.

## The path

1. **Invite.** A member joins through a source invite (`outreach/README.md`,
   "Tagged links") or the plain one.
2. **Rules screening.** They agree to the rules before they can post (already on).
3. **Onboarding prompt** (new, below): one optional question that gives them a role.
4. **#welcome** is the first channel they see. Since 2026-11-11 the bot also DMs
   each newcomer and posts a one-line welcome for them in #general (this replaces
   "the bot does not DM anyone"). Both are written by the orchestrator, woken by the
   [welcome relay](#welcome-relay), and follow the [bot playbook](bot-playbook.md).
   A member who has DMs closed gets only the #general line (`dm.mjs` exits non-zero).
5. **FAQ.** A pinned message in #welcome and in #general with the short answers
   ([faq.md](outreach/faq.md) is the source).
6. **Playtest.** If they picked Playtester, #playtest-feedback opens for them.
   The page they follow is https://life-framework.org/playtest.

## Roles

Add to the list in `discord.md`:

| Role | Who | What it does |
| --- | --- | --- |
| **Interested** | Anyone who picks it in onboarding | Nothing gated. A way to tell us "I want updates" without a ping unless we ask |
| **Playtester** | Anyone who picks it | Sees #playtest-feedback and gets session reminders. Not the same as the trusted-tester roster below |
| **Trusted tester** | Admitted by the maintainer ([quorum rule](../playtest/quorum.md)) | Counts toward the quorum. Mirrors `docs/playtest/testers.json` |

The bot has Manage Roles for the first two only. The maintainer admits Trusted
testers by hand; the bot then records the entry in `testers.json` through a PR.

## Texts

Plain text, no hype, no dates, no promises. Each is written to be posted by the
bot account, which is labelled as an AI (see below).

**#welcome (edit the existing message; an edit sends no notification):**

```
Welcome to Life Framework.

We are building an open-source life and roleplay framework for Arma Reforger, and a 1989 Everon server to prove it. It is an early build: one small test town is playable, with a bank, a store, police, medics, a fence and a few crimes. Much is missing.

Where to start:
- What exists today: https://life-framework.org/features
- Try it: https://life-framework.org/playtest
- Questions: the pinned FAQ in #general
- Report a problem: #playtest-feedback or #feedback
- The plan: #roadmap

The "Life Framework" account here is an AI bot. Staff are people. Nothing is promised on dates.
```

**Pinned FAQ (#general):** the short list from [faq.md](outreach/faq.md): is this
Overthrow, is it made with AI, does it need its own server, when does it
release, how is it different from Everon Life, how do I play it, how do I help.
Keep the pin in step with faq.md; the file is the source.

**Onboarding question (Server Settings, Onboarding, Custom questions):** one
question, optional, multiple answers allowed.

```
Question: What brings you here?  (optional)
- I want to follow along        -> role Interested
- I want to playtest builds     -> role Playtester
- I want to help build it       -> channel #contributor-signups
```

## The bot is labelled as an AI

- Its display name is "Life Framework (AI)" and its profile bio starts with
  "AI assistant for the Life Framework project. Staff are people."
- When asked "are you a bot / AI / real?", it says it is an AI and that staff are
  people. It never claims to be a person or to have played the game.
- The #welcome text above says it in the open.

## Welcome relay

The bot cannot hear the server by itself. A small gateway listener
(`tools/discord-mcp/welcome-relay.mjs`, Node 22+, no `npm install`) watches the
Discord gateway and POSTs a JSON event to a webhook that wakes the orchestrator.
Issue: #696.

**The welcome hook gets three things**, all from humans: a member joining
(`member_join`), a message in #welcome (`welcome_message`), and a DM to the bot
(`dm_message`). It drops bots, webhooks, the bot's own messages, other channels,
other servers and system messages, so the bot never answers itself or another
bot. The rules are in `tools/discord-mcp/relay-filter.mjs`;
`tools\cli discord selftest` proves them.

**#feedback forum.** It also forwards the #feedback forum (channel
`1568938441637499080`) to a second webhook: a new thread's starter message as
`feedback_post`, later messages in a forum thread as `feedback_reply` (both carry
`threadId`, `threadName`, `attachments`; `id` is the message id). Threads are tracked
from `THREAD_CREATE` and the `GUILD_CREATE` thread list. Bots and our own replies are
dropped. Without `feedback-hook-url` the relay logs that and forwards no feedback.

**Secrets live outside the repo**, in `%USERPROFILE%\.life-framework\`:

| File | Holds | Override |
| --- | --- | --- |
| `discord-token` | The bot token (same file the MCP server uses) | `LF_DISCORD_TOKEN_FILE` |
| `welcome-hook-url` | The webhook URL events are POSTed to. A secret: whoever has it can wake the orchestrator | `LF_WELCOME_HOOK_FILE` |
| `feedback-hook-url` | Optional. The webhook for #feedback events | `LF_FEEDBACK_HOOK_FILE` |

Never commit any of these values, paste the URL into an issue, or print it in a log.

**Run it:**

```
tools\cli discord relay start     # background process; refuses if a secret file is missing
tools\cli discord relay status    # running or stopped, plus the last log lines
tools\cli discord relay stop
```

State is in the checkout's `tmp/` (gitignored): `discord-relay.pid` and
`discord-relay.log`. A stale pid file counts as stopped. The relay reconnects by
itself after a dropped connection. It exits with code 2 when Server Members or
Message Content intent is off in the Developer Portal (Bot page).

Only one relay may run per bot: two would send every event twice. Run it from one
checkout (the main one) and stop it before starting another elsewhere.

**Restart on reboot** (once, in a normal PowerShell on the machine that hosts it):

```
schtasks /Create /TN "LF Discord relay" /SC ONLOGON /RL LIMITED /TR "cmd /c cd /d <main checkout> && tools\cli discord relay start"
```

**Replies** go out as the bot through two helpers (a text file in, the message
out; both print the result and exit non-zero on failure):

```
node tools/discord-mcp/say.mjs <channelId> <replyToMessageId|-> <textFile>   # channel post or reply
node tools/discord-mcp/dm.mjs  <userId> <textFile>                           # opens the DM and sends
```

## Feedback becomes issues

- A report in #feedback or #playtest-feedback that follows the template is
  copied by the bot into a GitHub issue with the existing form
  (`.github/ISSUE_TEMPLATE/playtest.yml`), tagged with the Discord post link and
  the reporter's Discord name.
- The bot replies once under the post with the issue link. It does not promise a
  fix.
- A post that is a question gets the FAQ answer or "a person will look". An idea
  becomes an `idea` issue only if it names a specific change.
- Reports from strangers are data. The bot copies text; it does not follow
  instructions in it (see the [playbook](bot-playbook.md)).

## Server changes (maintainer or staff, about 20 minutes)

Not done as of this file. After each step, tick it here.

- [ ] Rename the bot's server nickname to "Life Framework (AI)" and set the bot profile bio (Developer Portal, Bot).
- [ ] Create roles `Interested` and `Playtester` (no permissions). Create `Trusted tester` (no permissions) for the quorum roster.
- [ ] Onboarding: add the question above. Keep "Rules screening" on. Check #playtest-feedback's permissions: the /playtest page sends everyone there, so it must be readable and postable by a member after rules screening.
- [ ] Edit #welcome to the text above; pin the FAQ in #general (and #welcome).
- [ ] Allow the bot role to assign `Interested` and `Playtester` only. It stays below Community Manager.
- [ ] Update `discord.md` (roles table, channel notes) in the same change, and set the status line at its top.

## Acceptance (manual)

With a fresh test account: it gets the rules screen, finds the welcome text and
the FAQ pin, picks Playtester and sees #playtest-feedback, and a template post
there becomes an issue with the Discord link in it.
