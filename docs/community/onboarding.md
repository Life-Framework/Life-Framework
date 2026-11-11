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
4. **#welcome** is the first channel they see. The bot does not DM anyone.
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
