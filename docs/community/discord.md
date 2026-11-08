# Discord server

How the Life Framework Discord is laid out and moderated, as restructured on
2026-11-08. Change the server and this file together.

Invite: https://discord.gg/MjMexJteqz

## Stance

We set the direction. Contributions land only through GitHub pull requests
that the maintainers approve. The server posts no hype, no dates, and no
promises. The bot account "Life Framework" posted the welcome, rules, and
how-to-contribute texts; nothing else is announced from it.

## Channels

| Category | Channel | Who can post | Notes |
| --- | --- | --- | --- |
| START HERE | #welcome | Staff | What the project is, where it stands, where to look |
| START HERE | #rules | Staff | Eight rules. The guild's rules channel |
| START HERE | #announcements | Staff | Announcement channel |
| COMMUNITY | #general | Everyone | 5 s slowmode. Links and uploads only for Trusted, Contributor, staff |
| COMMUNITY | #feedback | Everyone | Forum. Tags Bug, Idea, Question, Playtest (one required). 10 min between new posts |
| COMMUNITY | #showcases | Trusted, Contributor, staff open posts | Forum. Everyone can reply |
| PROJECT | #github | GitHub Actions merged-PR feed (webhook) | Read only for people |
| PROJECT | #roadmap | Forum synced from GitHub by the bot | One thread per roadmap issue; everyone can reply, only the bot opens threads. Tags: the seven pillars, Needs a hand, In progress, Done |
| CONTRIBUTORS | #contributor-signups | Staff | Visible to everyone. How to contribute through GitHub |
| CONTRIBUTORS | #contributors | Contributor, Advisor, staff | Hidden from everyone else |
| CONTRIBUTORS | #code-review | Contributor, Advisor, staff | Hidden from everyone else |
| VOICE | Lounge | Everyone | |
| VOICE | Contributors | Contributor, Advisor, staff | |
| STAFF | #mod-log | Staff, Dyno, the bot | AutoMod alerts. System, update, and safety-alert channel |

Read-only channels deny sending, reactions, threads, polls, and voice messages
to @everyone.

## Roles

Top to bottom: Discord Admin, Organizer, Community Manager, Life Framework
(bot), Dyno, Contributor, Advisor, Trusted, Server Booster.

- **Community Manager** is the moderator role: manage messages, threads and
  nicknames, timeout, kick, ban, view the audit log.
- **Contributor** is for people whose work the maintainers accepted. It
  replaced the nine skill roles (Scripting, Mapping, UX, Community, Web, 3D,
  Graphic, Documentation, New Contributor); everyone who held one got it.
- **Trusted** unlocks links and uploads in #general and new posts in
  #showcases. Staff give it to members who take part in good faith.
- **Advisor** can read the contributor channels.

## Moderation

- Verification level High. Explicit-content filter scans all members.
  Notifications default to mentions only.
- Rules screening: new members agree to the rules before they can post.
- Onboarding is on, with no questions; it shows the public channels.
- AutoMod blocks mass mentions, invite links (our own invite is allowed),
  scams, suspected spam, slurs and sexual content. Every hit alerts #mod-log.
- Moderators remove spam and scams and ban without warning. Dyno stays for
  its existing moderation features.

## GitHub announcements

The `discord-merged-pr` workflow posts only when a pull request is merged. It
includes the PR description and displays up to nine GitHub-hosted images from
that description inline. Mentions in PR descriptions are suppressed.

It posts through the #github webhook "Merged pull requests", stored as the
repository Actions secret `DISCORD_WEBHOOK_URL`. The repository webhook that
sent every GitHub event to #github is turned off. Without the secret the
workflow passes with a notice.

## Roadmap board

GitHub issues in M milestones are the source of truth. Each issue sits in the
Tree of Life (life-framework.org/roadmap) by two fields: its `topic:` label is
the pillar, and a `Parent: <System>` line at the top of its body is the system.
The website reads them when it builds; `website.yml` rebuilds when a milestone
or one of its issues changes.

`discord-roadmap-sync` mirrors the same issues into the #roadmap forum, one
thread per issue, so each piece of work has its own place to discuss:

- An open issue gets a thread named `#<number> <title>`. The bot's first
  message is the issue card: status, milestone, pillar and system, why, what
  to build, done when, what it waits on, who is on it, and how to join. Every
  sync edits it in place.
- Tags follow the issue: its pillar plus Needs a hand, In progress, or Done.
  Tags are moderated, so only the bot and staff change them.
- Everyone can reply in a thread; only the bot opens threads.
- A closed issue's thread is tagged Done and archived. A reply reopens it.
- An issue that leaves the roadmap has its thread archived and locked, never
  deleted. The sync touches only threads the bot opened.

To take an issue, comment `/claim` on it on GitHub (`claim-issue` workflow). A
bot assigns you, and up to ten people can claim one issue; `/unclaim` steps
back. Assignment shows who signed up, not whether they are active. `/claim` and
`/unclaim` trigger an immediate website and forum sync. The sync also runs on
every issue or milestone change and every five days.

Setup, done 2026-11-08: the forum is #roadmap under PROJECT (channel
1569083962628767784). @everyone can view, read history, react, and send in
threads, but cannot open posts. The bot can view, send, manage threads, and
manage the channel (for the tags). The repository has the Actions variable
`DISCORD_ROADMAP_FORUM_ID` and the secret `DISCORD_BOT_TOKEN`. Without them the
workflow passes with a notice.

Preview the cards without Discord:
`GITHUB_REPOSITORY=Life-Framework/Life-Framework node tools/discord-mcp/sync-roadmap.mjs --dry-run`.
For a real run, set `DISCORD_BOT_TOKEN`, `GITHUB_TOKEN`, and
`DISCORD_ROADMAP_FORUM_ID` in a terminal, or use **Run workflow** on
`discord-roadmap-sync`.

## Removed on 2026-11-08

#discord-community-updates, #hidden-join-channel, #roles, #roadmap-items,
#roadmap-design, #documentation, #technical-support, #clips-and-highlights,
the old #feedback text channel, and the old #welcome. Planning now happens in
GitHub milestones and issues; the new #roadmap forum mirrors them.
