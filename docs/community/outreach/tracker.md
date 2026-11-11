# Outreach tracker and weekly results

Issue: #574. Kit: [README.md](README.md). One row per community, kept by
whoever last touched it (the maintainer after posting, an agent for the rest).
Update it the same day; a stale row misleads the weekly numbers.

Statuses, in order: `drafted` (the file exists), `asked` (mods messaged),
`approved`, `posted`, `declined`, `no reply`. A community can go `asked` to
`no reply` after two weeks; do not nudge more than once.

## Tracker

| Community | File | Source tag | Status | Asked | Posted | Link to the post | What came back |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Official Arma Reforger Discord | [file](arma-reforger-discord.md) | arma-reforger-discord | drafted | | | | |
| Everon Life | [file](everon-life.md) | everon-life | drafted | | | | |
| Overthrow | [file](overthrow.md) | overthrow | drafted | | | | |
| r/ArmaReforger | [file](reddit-armareforger.md) | reddit-armareforger | drafted | | | | |
| GitHub README | | github | not wired | | | | The README links the Discord directly; point it at `/join/github` if we want to count it |

Dates are `YYYY-MM-DD`. "What came back" is one line: replies, mods' answer,
anything surprising. Put detail in the weekly summary.

## Weekly summary

One file a week in `weekly/<yyyy-mm-dd>.md` (the Monday it covers from), copied
from [weekly/TEMPLATE.md](weekly/TEMPLATE.md). The first one is posted to the
epic (#566) as a comment, with the maintainer's name for posts they made.

Where the numbers come from:

| Number | Source | How |
| --- | --- | --- |
| Visitors per community | Cloudflare Web Analytics, filter Path = `/join/<source>` | Dashboard, Web Analytics, the site, Path; one row per source tag. Visits and page views, no cookies, no personal data |
| Other site visitors | The same dashboard | Referrer column |
| Discord joins per community | The Discord invites named after each source (Server Settings, Invites: uses) | Read each invite's use count at the start and the end of the week; the difference is the week's joins. Only works for a source whose invite is pasted into `website/content/outreach-sources.json` |
| Playtester sign-ups | The Playtester role count (Server Settings, Roles) and new roster entries in `docs/playtest/testers.json` | Count at the start and the end of the week |
| Reports filed | GitHub: issues with the `playtest` template opened that week, and posts in #playtest-feedback | `gh issue list --search "created:>=<date>"` |

The analytics dashboard is the maintainer's login; agents cannot read it. The
maintainer pastes the visitor numbers into the file, or exports the table, and
the agent writes the rest. If the numbers are missing, the summary says "not
provided" and does not estimate.

## Adjusting the kit

Each week, in the summary's last section, answer three questions from the
numbers and the replies:

1. Which source brought people, and how many of them joined?
2. What did people ask that the [FAQ](faq.md) did not answer? Add it.
3. What did a mod or a commenter object to? Change the post text for that
   community, or stop posting there.

A change goes in the same pull request as the summary. Do not add a community
because a number is low elsewhere; ask the maintainer.
