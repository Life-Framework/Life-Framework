# Contributing to Life Framework

Welcome. Join the community, choose a piece of work, build it with the team,
then raise a pull request. You can contribute code, playtest reports, world
building, UI, 3D art, sound, or documentation.

## 1. Join the Discord

[Join the Life Framework Discord](https://discord.gg/MjMexJteqz). Say hello in
`#general`, ask questions, and tell us what you would like to work on. The team
can help match your interests to an issue.

## 2. Pick an issue

Start with a [good first issue](https://github.com/Life-Framework/Life-Framework/issues?q=is%3Aopen%20is%3Aissue%20label%3A%22good%20first%20issue%22)
or an open branch on the [Tree of Life](https://life-framework.org/roadmap), the
roadmap map of every issue. Each open issue also has a discussion thread in the
#roadmap forum on Discord. Issues
are the source of truth for the files, boundaries, and proof expected.

Comment `/claim` on the issue to take it; a bot adds you as an assignee. Others
can `/claim` the same issue to join you (up to ten), and `/unclaim` steps you
back. The website and Discord update from the assignee list. Then coordinate
with the team on Discord.

## 3. Build and playtest

Read [AGENTS.md](AGENTS.md), the [setup guide](docs/setup.md), and the
[playtest guide](docs/playtest.md). The guides cover the toolchain, local paths,
and how to open the M1 town.

Keep code changes in a worktree, not `main`:

```powershell
tools\cli wt new <short-name>
cd ..\Life-Framework-ws-<short-name>
tools\cli status
tools\cli validate
```

Follow the issue's proof requirements. For script behavior, write a regression
test and prove it fails before the fix. Run the focused test while iterating,
then verify the player path in DebugWorld or the playtest town when the issue
touches in-game behavior.

To open the local M1 playtest, check whether it is already running, then launch
it if needed:

```powershell
tools\cli playtest status
tools\cli playtest
```

Stop it with `tools\cli playtest stop` when you are done. Do not change or
commit machine-specific paths or generated files.

## 4. Raise a pull request

Use `tools\cli wt ship <short-name> --closes <issue-number>` when the worktree
is ready. It runs the project's checks and opens a pull request. In the PR,
describe the change, list the test and in-game proof, and name anything that
still needs a manual check.

The team reviews the PR with you, helps resolve feedback, and merges it when it
is ready. Stay in the conversation until the checks are green and it lands.

## 5. Grow into an approver

We want contributors to take on review responsibility quickly. After a few
solid PRs have landed and the team has seen your reviews, maintainers can grant
approval and merge access. Trusted approvers review other contributors' work
and help good changes ship without a long wait. Your own PR still gets an
independent review.

## Working with coding agents

Coding agents are welcome. Start at the repository root and ask the agent to
read [AGENTS.md](AGENTS.md), [CONTRIBUTING.md](CONTRIBUTING.md),
[docs/setup.md](docs/setup.md), and [docs/playtest.md](docs/playtest.md). The
[contribution page](https://life-framework.org/contribute) has a ready-to-paste
prompt that checks local setup and opens the playtest before you choose an
issue.

Agents must follow the issue scope, research Enfusion APIs before using them,
work in a named worktree, verify their changes, and leave the PR ready for team
review. The human contributor remains responsible for the result.

## More ways to help

- Play a session and send a clear report through the Discord `#feedback` forum.
- Help with mapping, world building, UI, 3D models, sound, testing, or docs.
- Share an idea or ask for a first issue on Discord.

All contributions follow the [Code of Conduct](CODE_OF_CONDUCT.md). Source code
contributions are licensed under the repository's MIT licence.
