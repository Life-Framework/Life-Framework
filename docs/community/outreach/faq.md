# FAQ and honest replies

The questions a post brings, answered once, true to the repository today. The
maintainer pastes from here when replying in other communities; the bot pins
the short list in our Discord (see [onboarding](../onboarding.md)). Issue: #573.

Tone and limits: [discord.md](../discord.md) and the [bot playbook](../bot-playbook.md).
No hype, no dates, no promises. If an answer here goes stale, fix this file
first and the pins after it.

**Keep it in step with:** `docs/roadmap/README.md` (what is planned), `docs/features.md`
(what exists), `docs/decisions.md` (what we decided and why), and `THIRD_PARTY.md`
(what we borrowed). Last checked against them on 2026-11-10.

## Short answers (the pinned list)

Each one fits a Discord message.

**What is Life Framework?**
An open-source life and roleplay framework for Arma Reforger: economy, jobs,
crime, police, medics and persistence, built to be used in the world instead of
menus. A 1989 Everon server is the proof we are building alongside it.

**How far along is it?**
Early. A small test town is playable with a bank, a store, a car lot, police,
medics, payphones, a fence and a few crimes. Most of it has not been played by
many people. There is no public release.

**Can I play it?**
Yes, the test build. It is a zip you download from https://life-framework.org/playtest
(Windows, with Arma Reforger from Steam): unzip it and run the launcher. It can play on
its own, or join the community server, which is hosted from a person's PC and is not
always on.

**Is this Overthrow?**
No. Overthrow is a resistance and occupation campaign. We are a life and
roleplay framework. We use a small amount of Overthrow's code with its
author's permission and credit it (below).

**How is it different from Everon Life?**
Everon Life is an older open-source life framework for Reforger, and ours
started from it with credit. We are a separate project with our own `LF_`
code prefix, so both can be installed on one server. What we are doing
differently: interactions happen in the world (a clerk, a counter, a
payphone) instead of menus; the systems are separate from the 1989 Everon
setting so another setting can be built on the same core; and every change
is a public pull request that has to pass checks and an in-game test suite.

**Was it made with AI?**
Yes. The maintainer directs the project and merges every change. AI agents
write most of the code, docs and website. The Discord bot is an AI and says
so. Every change goes through the same checks and a pull request, in the open.

**Does it need a dedicated server?**
It is built for dedicated servers. The playtest runs as one, hosted from the
maintainer's or a volunteer's PC. Server owners cannot install it from the
Workshop yet; that is the next planned milestone and has no date.

**When does it release?**
There is no date and we will not guess one. The plan is in the roadmap:
https://life-framework.org/roadmap

**Is it free?**
The source is MIT on GitHub, the Workshop build will be APL because the Workshop
requires it, and the framework is never sold. Donations are not open. See
https://life-framework.org/support for what the project costs.

**How can I help?**
Play the build and report what broke. Or take an issue on GitHub: comment
`/claim` on it. https://life-framework.org/contribute

## Longer replies

For a comment thread, adjust the opening to the question.

### "Another Altis Life clone?"

> Altis Life is where the idea comes from, and we say so. The plan is to keep what made it work (jobs, crime and police on a shared economy) and build it into Reforger's world instead of menus. It is early: a small test town, not a full map. https://life-framework.org/features lists what exists.

### "Is this just Overthrow with a new name?"

> No. Overthrow is a resistance campaign; this is a life and roleplay framework. We use a small number of Overthrow's files with the author's permission and credit them in the repo. They are different mods with different goals. THIRD_PARTY.md lists exactly what we took.

### "Why not just use Everon Life?"

> You can. It is a good project and ours started from it. We are building something different: world-based interactions, a core that is separate from the setting, and tests for each system. If you already run Everon Life, our `LF_` prefix means installing both does not collide, though we do not provide a bridge between them.

### "AI-generated slop?"

> Most of the code is written by AI agents under the maintainer's direction, and we don't hide it. What keeps it honest: every change is a public pull request, the repo has an in-game test suite, and a person plays a feature before it counts as done. If you find something that is broken, that report is the most useful thing you can give us.

### "Can I add it to my server?"

> Not yet. The framework is not packaged for server owners; the plan for that (config, a Workshop release, owner docs) is the next milestone. If you want to follow it, join the Discord or watch the roadmap.

### "Can it work with other mods?"

> The rule is "do not collide", and nothing more. We use an `LF_` prefix so we do not clash with Everon Life or others. We have not tested other mods against it and do not provide bridges or importers.

### "Is it a whitelisted roleplay server I can join?"

> Not yet. What exists is a test town that people try in short sessions. A 24/7 roleplay server is a later milestone and has no date.

### "What happens to my character if the server restarts?"

> In the test build your cash, bank, job, cars and organization are saved by Steam name and should come back. A restart is one of the things we most want people to check.

### "Do you take money?"

> No. Donations aren't open. If that changes, the page at https://life-framework.org/support will say so, and a donation would buy nothing in the game.

### "I found a bug."

> Thank you. Please post it in #playtest-feedback on our Discord (invite: https://discord.gg/MjMexJteqz) or open a Playtest issue on GitHub. Say what you did, what you expected, what happened, where, and which build. A person reads every report. It is not a promise that it will be fixed.

## Lines we do not use

- "Coming soon", "revolutionary", "the best", "the most realistic".
- Any date, month or season for a release or a feature.
- "We will add X" for something that is not on the roadmap as planned.
- Comparisons that rank us above other mods.

## What is built, what is planned, what we do not promise

| | State |
| --- | --- |
| Economy, banking, shops, jobs, vehicles and garage, jail, police duty, organizations and hideouts, payphones and dispatch, downed state and medics, investigation (witnesses, casings, case file), the fence and a few crimes | Built in the test town. Not widely played. `docs/features.md` has each system |
| A travel hub and a few more areas | Planned or in progress. See the playtest page's "Not in this build" list |
| A public Workshop item for the test build | Not yet. Today the build is a zip from the website. A Workshop-ready kit is being prepared (#567) |
| A framework release for server owners (two addons, config, owner docs) | Planned: milestone M3 |
| A 24/7 flagship server | Planned: milestone M4 |
| Console or a particular platform | Not tested. We make no claim |
| Dates for any of it | None. We do not give them |
