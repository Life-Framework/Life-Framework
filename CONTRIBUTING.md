# Contributing to Life Framework

Thanks for looking. We need more people testing and more people writing code, and
you do not have to be a programmer to help: docs, tooling scripts, the website,
world building, UI, art, sound and clear playtest reports all count.

## What this is

Life Framework is an open-source life and roleplay framework for Arma Reforger,
plus a 1989 Everon server built on it to prove it works. It is early alpha. Most
of the code is EnforceScript (the game's script language) in `addons/`; the dev
tools are Node.js scripts in `tools/`; the website in `website/` is a small
Vite and React project. Two addons:

- **`LifeFramework`** (the core) decides what the game can do: systems, rules,
  config, persistence, the UI kit.
- **`LF_Everon1989`** (the pack) decides what our server is: places, names,
  prices, the period look, the story.

[docs/architecture/core-vs-pack.md](docs/architecture/core-vs-pack.md) says which
side a change belongs on. The core never references the pack.

## Licence

The source in this repository is **MIT**. The build we publish on the Arma
Reforger Workshop is licensed under the **Arma Public License (APL)**, because the
Workshop's terms require it for uploaded content ([decision of
2026-11-08](docs/decisions.md)). Your contribution is licensed as MIT in the repo.

The 1989 pack (`addons/LF_Everon1989/`) is MIT today, but the project may license
future versions of it differently. By contributing to the pack you agree that we
may relicense your contribution for those future versions. Releases already
published stay MIT. The core stays MIT.

## Set up in about 15 minutes

The full guide is [docs/setup.md](docs/setup.md). The short version:

```sh
git clone https://github.com/Life-Framework/Life-Framework.git
cd Life-Framework
git config core.hooksPath .githooks
node tools/cli.mjs status      # shows what it found and what is missing
node tools/cli.mjs validate    # the cheap repo checks
```

- **You need:** Git, Node.js 20+, PowerShell 7+ (`pwsh`, for `validate`). For game
  changes also Arma Reforger and **Arma Reforger Tools** (Workbench) from Steam.
  The in-game tests also want **Arma Reforger Server** (Steam app 1874900).
- **You do not need:** pool or remote test machines, the MCP servers, or a coding
  agent. They are optional and described in
  [docs/dev-tooling.md](docs/dev-tooling.md) and setup.md section 6.
- A docs, tooling or website change needs only Git, Node and `pwsh`, not the game.

Not sure it works on your machine? Ask in Discord `#dev` and say what
`node tools/cli.mjs status` printed.

## How a change flows

1. **Find or file an issue.** The work is tracked as
   [GitHub issues](https://github.com/Life-Framework/Life-Framework/issues). Start
   with a [good first issue](https://github.com/Life-Framework/Life-Framework/issues?q=is%3Aopen%20is%3Aissue%20label%3A%22good%20first%20issue%22)
   or an open branch on the [roadmap](https://life-framework.org/roadmap). Have an
   idea that is not there? Raise it in `#dev` or open an issue first, so nobody
   builds something we would turn down.
2. **Take it.** See "Slice cards and claiming" below.
3. **Branch in a worktree.** `node tools/cli.mjs wt new <short-name>` makes a
   sibling folder on its own branch `ws/<short-name>`. Never commit to `main`, and
   build, test and run only in the worktree (the folder you cloned stays clean).
4. **Build it and prove it.** For script behavior, write a test
   (`Scripts/Game/Tests/LF_Test_*.c`) that fails without your change, then make it
   pass: `node tools/cli.mjs test --no-build --only LF_Test_<Name>`. For tools and
   docs, `node tools/cli.mjs validate` is the check. For anything a player sees,
   take a before and after picture first (`tools\cli look`, see
   [docs/visual-testing.md](docs/visual-testing.md)).
5. **Open a pull request** against `main`. If you have push access,
   `node tools/cli.mjs wt ship <short-name>` runs the gate, pushes and opens it.
   If not, fork, push your branch, and open the PR from your fork. Fill in the PR
   template: what changed, how you proved it, and a `## Manual testing` section
   for anything that needs a person to check it in the game.
6. **CI and review.** GitHub Actions runs the `validate` check on every PR; it must
   be green. A maintainer reviews with you and merges. Merging is not "done": a
   person plays the change on `main` and signs the issue off.

## Slice cards and claiming

Big features are cut into **slice cards**: small issues, labelled `slice`, sized
for one person or agent in 30 to 45 minutes. A card names the files it owns, what
it depends on, and the check that proves it
([docs/process/slice-card.md](docs/process/slice-card.md)). Cards that own
different files can be built at the same time without conflicts, so please stay
inside the files a card owns.

To take one:

- Check it is open, has no assignee, and nobody has an open PR for it. Read what
  its `depends` line waits on: those should already be merged.
- On a roadmap issue (one with an `M<n>` milestone) comment `/claim`; a bot adds
  you as an assignee, and `/unclaim` steps you back. Slice cards sit outside the
  milestones, so the bot does not answer there: comment that you are taking it and
  post the number in `#dev`, and a maintainer assigns you.
- If a card turns out bigger than its budget, say so on the issue. We split it,
  we do not stretch it.

## Code style, in plain terms

The full list is [AGENTS.md](AGENTS.md); the rules people trip on first:

- Class names start with `LF_`, and tests are `LF_Test_*`. Never use `EL_`: the
  older EveronLife mod can load on the same server.
- Never commit generated files: `resourceDatabase.rdb`, `*.rdb.lock`, the addon
  `log`, `*.gproj.user`, `node_modules/`. The pre-commit hook catches them.
- A resource's GUID (in its `.meta`) is unique and permanent. Never copy one, and
  never rename or move a resource without checking what refers to it.
- Text players read goes through the localization table, not into the code or
  layouts. Place names and flavour text belong in the pack.
- Core code never holds a map coordinate, an entity name or a layer name.
- If you change a feature, update its row in `docs/features.md` in the same PR.
- EnforceScript is not C#: there is no `cond ? a : b` operator, managed objects need
  `ref`, and you store `EntityID`, not `IEntity`. Look an API up
  (`node tools/cli.mjs api class <Name>`), never guess it.
- Log a state change or a refusal with `LF_Debug` so it can be found in the log; a
  silent failure is a bug.

## Working with coding agents

Coding agents are welcome. Ask yours to read [AGENTS.md](AGENTS.md), this file and
[docs/setup.md](docs/setup.md) first. Claude Code reads `CLAUDE.md`, which points
at `AGENTS.md`; opencode, Codex and Cursor read `AGENTS.md` directly. The
[contribution page](https://life-framework.org/contribute) has a ready-to-paste
prompt. Agents follow the issue's scope, work in a named worktree, and leave the PR
ready for review. You stay responsible for what you submit.

## Where to ask

- **Discord `#dev`** for setup problems, "is this the right place for my change",
  and picking or claiming an issue. Join:
  [discord.gg/MjMexJteqz](https://discord.gg/MjMexJteqz).
- **The issue itself** for questions about what a card wants.
- **`#feedback` or `#playtest-feedback`** if you only want to play and report.

Playing counts. Join a weekly playtest session (the server is invite-only; ask in
Discord) and send a clear report: where you were, what you pressed, what you
expected, what happened.

## Growing into an approver

After a few solid PRs and some reviews of other people's work, maintainers can
give you approval and merge access. Your own PR still gets an independent review.

All contributions follow the [Code of Conduct](CODE_OF_CONDUCT.md).
