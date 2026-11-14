# Release process: updates and rollback

How a Life Framework release is cut, shipped, and rolled back. It covers the
`main` and `stable` branches, `tools\cli release`, playtest builds, saves across
a game update, and the Workshop steps. Issue states are in
[AGENTS.md](../AGENTS.md) ("What done means") and
[worktrees.md](worktrees.md) ("Release gates").

Status: the commands below exist today. The Workshop upload is **not yet**
run from tooling. It is maintainer-only, by hand in Workbench (#370).

## Who does what

| Step | Agent | Maintainer |
|---|---|---|
| Merge PRs to `main` | Yes (`wt ship`) | Reviews when asked |
| `tools\cli promote` (list the checklist) | Never | Yes |
| `tools\cli promote --confirm` (move `stable`) | Never | Yes |
| `tools\cli release <x.y.z>` dry run | Yes | Yes |
| `tools\cli release <x.y.z> --confirm [--push]` | Never | Yes |
| `tools\cli playtest release` | Never | Yes |
| Workshop upload of core and pack | Never. **Not yet: maintainer-only.** | Yes, by hand (#370) |
| Host `preflight` and `restore` after a game update | Never | Yes (the server owner too) |

## Branches

- `main` is the integration branch. Agents branch from it and merge to it. Playtests
  run from `main`.
- `stable` is the branch a human moves. The Workshop release, the flagship's
  24/7 server, and `tools\cli release` all use it. Nothing edits `stable`
  directly.

## Issue states

An issue moves through these labels. Only a human sets `verified`.

| Label | Meaning |
|---|---|
| `agent:working` | Being built. |
| `in-dev` | Merged to `main`, not verified. |
| `needs-playtest` | Merged, and the PR has a `## Manual testing` section with real steps. |
| `verified` | A human signed it off. Only this closes an issue. |
| `regression` | A failed verification. |

`promote --confirm` sets `verified` and closes the issues the promoted PRs
reference. Reports say "merged, in-dev", never "done".

## Cut a release

1. **List what is waiting.** `tools\cli promote` lists the PRs merged to `main`
   since `stable` and writes `tmp/promote/checklist-<sha>.md`. It changes
   nothing. Each item carries its manual steps.
2. **Run the playtest pass if one is planned.** `tools\cli playtest checklist`
   writes the walk-through for the open `needs-playtest` and `in-dev` issues
   (`docs/worktrees.md`, "One session, many issues"). Quorum sign-off applies
   when `docs/playtest/quorum.json` has `"enabled": true`
   (`docs/playtest/quorum.md`).
3. **Dry run the version.** From a clean worktree on `main`, or a detached HEAD at
   `origin/main` or `origin/stable`:

   ```
   tools\cli release 0.2.0
   ```

   The default is a dry run. It prints the plan, names every refusal (dirty tree,
   wrong branch, version not `x.y.z`, tag taken), and runs the cheap
   `release-ready --strict` check. Nothing changes.
4. **Cut it (maintainer).** `tools\cli release 0.2.0 --confirm` does this in
   order:
   - stamps the version in `addons/LifeFramework/LifeFramework.gproj`,
     `addons/LF_Everon1989/LF_Everon1989.gproj`, and both Workshop kit READMEs;
   - runs the gates: `validate --full`, `release-ready --strict`, the full suite,
     `audit placement --strict`, and a `pack` of each addon;
   - writes `tmp/release/notes-0.2.0.md` from the PR titles merged since the
     previous `v*` tag;
   - tags `v0.2.0` on a release commit that no branch points at, and restores the
     stamped files.

   A failed gate restores the files and tags nothing. `--push` also pushes that
   one tag (`refs/tags/v0.2.0`). Without it nothing leaves the machine.
5. **Promote the commit (maintainer).** `tools\cli promote --to <sha> --confirm`
   fast-forwards `origin/stable` to the commit that passed step 4, tags it
   `stable-<yyyymmdd>-<sha8>`, and marks the issues `verified`. It refuses a
   target that is not a descendant of `stable`, and it never forces.
6. **Upload the Workshop items. Not yet: maintainer-only.** The Workshop
   upload is by hand in Workbench from the tagged commit. The click path is in
   [playtest/workshop-publish.md](playtest/workshop-publish.md). The version
   scheme and names are settled in #367. Every publish takes a higher version
   number, even for a text-only change.

### The first Workshop publish (D3)

Nothing is published before both addons exist. The first Workshop publish of
the core and the pack comes after the whole pack move, batches 1 to 4 (decided
2026-11-14 in [roadmap/m3-plan.md](roadmap/m3-plan.md), D3). No host runs an
old single-item build, so the header move strands nobody. The older
2026-11-12 entry in [decisions.md](decisions.md) is superseded by this one.
Until batch 4 lands, a release is tagged and promoted, but not uploaded.

## Playtest builds

A playtest build is a pre-release zip that players download. It is not a
Workshop item.

```
tools\cli playtest release            # next tag: playtest-m2-1, then -2, ...
tools\cli playtest release --tag playtest-m2-2
```

Run it from a worktree at `origin/main`, with no uncommitted changes. The
command:

1. refreshes `resourceDatabase.rdb` in headless Workbench, then checks that every
   registered resource is fresh. The game refuses a zip whose rdb is stale;
2. creates a GitHub pre-release for the tag, with the rdb attached;
3. starts `.github/workflows/playtest-release.yml`, which packs
   `LifeFramework-Playtest.zip` from a clean checkout and makes the release the
   latest one.

Tags must match `playtest-*`, and an existing tag is refused. The download link
is `releases/latest/download/LifeFramework-Playtest.zip`. When you cut a
playtest, also update `FALLBACK_RELEASE` in `website/src/` (see
[website/README.md](../website/README.md)), so the site shows the right build
when GitHub cannot be reached.

## Saves across a game update

Each save records the game build that wrote it and the save version it uses
(`buildstamp`, save version 32, #980). The current list of save versions and
their payloads is [persistence.md](persistence.md). A save from another build
is loaded only after a preflight.

After a game update, run this before opening the server:

```
tools\cli host preflight
```

It backs the saves up, boots the host on the installed build, waits for the
`save loaded:` line, stops the server to write a shutdown save, and then
judges the log. On a failure it restores the backup, keeps what the failed
boot left, and says "do not open the server". `tools\cli host start` refuses a
save from another build until a preflight passes. `tools\cli host restore
[<backup>]` puts a backup back by hand. The full routine, the log lines, and
the changelog table are in [ops/game-updates.md](ops/game-updates.md).

Save reshapes follow D6 (decisions): one PR per reshape, each with its own
version bump, and one announced reset at a release boundary. Pre-move saves must
load after every move batch (D7). The release notes and the upgrade page for the
release say which save version changed (see [upgrades/README.md](upgrades/README.md)).

## Rollback

There is no one-command rollback. Each path below is a documented way to go
back, and the maintainer chooses which applies.

**Code on `main`.** Use a revert, not a force push.

- `tools\cli promote --revert <PR>` opens a PR on `main` that reverts that
  merge, with auto-merge queued.
- `tools\cli promote --fail N[,N]` marks issues `regression` and reopens them
  without moving `stable`.
- The `issue-state` workflow marks the issues of a merged `Reverts #P` PR as
  `regression`.

**`stable`.** `stable` only moves forward. A promotion never goes to a commit
that is not a descendant of `stable`, so you cannot move it back. To take a bad
change out of `stable`, revert it on `main` and promote the fix forward. When a
regression blocks a promotion (`--fail` inside its range), pass an earlier
`--to` to promote a commit between `stable` and `main` that is still good.

**A released tag.** Tags are not moved. A bad `v<x.y.z>` gets a new number once
its revert or fix is promoted. `tools\cli release` refuses a tag that exists.

**Playtest zip. Not yet automated.** `playtest release` only goes forward, and
no command makes an earlier build the latest one. Doing it means changing the
GitHub release by hand: mark the bad build as a pre-release, and make an
earlier build the latest. Confirm that the earlier zip still loads with the
current game build before you do.

**Workshop items. Not yet: maintainer-only.** Rolling back means uploading the
previous good commit's build again under a new version number. Whether the
Workshop accepts a lower number has not been checked. Settle it before the
first upload, and record the answer in #367.

**Server saves.** `tools\cli host restore` puts back the backup a preflight
or a restart took. Stop the server first. The save a failed boot left is moved
to `backups/<time>-replaced-by-restore`, never deleted.

**Config.** Keep the previous `config.json` before you change it. Run
`tools\cli config-check <config.json>` before a restart. A bad value is refused
and the shipped default stays, so a bad config degrades instead of stopping the
server (see [config/owner-config.md](config/owner-config.md)). There are no
config migrations yet; `schemaVersion` is 1 (#302 to #304 add them).

## Release checklist

- [ ] `tools\cli promote` lists every PR, and each manual step has been run or
      scheduled.
- [ ] `tools\cli release <x.y.z>` dry run is clean on `main` or `stable`.
- [ ] `tools\cli release <x.y.z> --confirm` passed every gate; the notes are in
      `tmp/release/notes-<x.y.z>.md`.
- [ ] `promote --to <sha> --confirm` moved `stable`, and the issues read `verified`.
- [ ] The upgrade page for the version is written and linked
      ([upgrades/README.md](upgrades/README.md)).
- [ ] Save version changes are listed; a preflight is named for owners if the
      game build moved.
- [ ] Workshop upload: not yet, maintainer-only (after D3 batch 4).
- [ ] Playtest zip, if one is cut: `playtest release` ran, the workflow finished,
      and `FALLBACK_RELEASE` is updated.

Related: [worktrees.md](worktrees.md), [playtest/quorum.md](playtest/quorum.md),
[playtest/workshop-publish.md](playtest/workshop-publish.md),
[ops/game-updates.md](ops/game-updates.md), [persistence.md](persistence.md).
