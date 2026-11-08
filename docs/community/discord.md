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
| PROJECT | #github | GitHub Actions merged-PR feed (setup pending) | Read only for people |
| PROJECT | #roadmap | New GitHub-synced Forum (setup pending) | One read-only post per roadmap pillar, mirroring the website's Tree of Life; click an issue and comment `/claim` on GitHub to join |
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

To connect it, create an incoming webhook for #github and save its URL as the
repository Actions secret `DISCORD_WEBHOOK_URL`. Disable the old GitHub feed in
the channel to avoid duplicate or unfiltered notifications.

## Roadmap board

GitHub issues in M milestones are the source of truth. Each issue sits in the
Tree of Life by two fields: its `topic:` label is the pillar, and a
`Parent: <System>` line at the top of its body is the system. The website reads
them from GitHub when it builds; `website.yml` also rebuilds immediately when a
milestone or one of its issues changes. `discord-roadmap-sync` creates or updates
one Forum post per pillar, grouped by system, with open issues first and a link
to the full tree on life-framework.org. Every issue links to GitHub. Click an
issue and comment `/claim` on GitHub to join, or comment `/unclaim` to step back.
Each collaborator can comment `/claim` to add themselves as an assignee (up to
GitHub's limit of ten). An assigned issue stays open to more helpers. Assignment
shows who signed up, not whether they are currently active. The website and
forum show everyone assigned; `/claim` and `/unclaim` trigger an immediate sync.
The workflow also refreshes every five days to keep Discord's auto-archived
forum posts visible. The sync manages only the bot's own posts; a post whose
pillar disappears is renamed `[ARCHIVED]`, archived, and locked, never deleted.
Preview the posts without Discord:
`GITHUB_REPOSITORY=Life-Framework/Life-Framework node tools/discord-mcp/sync-roadmap.mjs --dry-run`.
Until the secret and variable below exist, the workflow passes with a notice.

Create a public Forum channel named #roadmap under PROJECT. Let @everyone view
it but deny them `Send Messages`; allow the Life Framework bot to view, send,
read history, and manage threads. Add the forum's channel ID as the repository
Actions variable `DISCORD_ROADMAP_FORUM_ID`, and the bot token as the Actions
secret `DISCORD_BOT_TOKEN`. Run `discord-roadmap-sync` once with **Run workflow**
to seed the board. The bot token needs access to the channel and `Manage Threads`
to update archived posts. The workflow does not create or delete Discord
channels.

## Removed on 2026-11-08

#discord-community-updates, #hidden-join-channel, #roles, #roadmap-items,
#roadmap-design, #documentation, #technical-support, #clips-and-highlights,
the old #feedback text channel, and the old #welcome. Planning now happens in
GitHub milestones and issues; the new #roadmap forum mirrors them.
