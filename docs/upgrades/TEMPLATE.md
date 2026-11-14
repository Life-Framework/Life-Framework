# Upgrade to <x.y.z>

Upgrading from `<previous x.y.z>`. Release notes: `tmp/release/notes-<x.y.z>.md`
(the release page on GitHub). Game build tested: `<build>` (see
[ops/game-updates.md](../ops/game-updates.md)).

## Before you start

1. Stop the server (`tools\cli host stop`), then copy your server profile folder,
   the one that holds `LifeFramework/config.json` and the saves. Keep the copy
   until the server has run a full restart.
2. If the game updated, run `tools\cli host preflight` before anything else
   (manual). A preflight that fails restores your saves and says not to open
   the server.

## Summary

<One or two sentences: what a server owner notices first. No marketing.>

## Config changes

Edit your `config.json` (`$profile:LifeFramework/config.json`) by hand, then run
`tools\cli config-check <path>` (manual). Nothing here is required unless your
server uses the feature.

| Key | Was | Now | Why it changed |
|---|---|---|---|
| `<section.key>` | `<old>` | `<new>` | `<one line>` |

- Added: `<section.key>` default `<value>`. Leave it out to keep the default.
- Removed: `<section.key>`. The server logs it as ignored; delete the line.
- A pack changed a default: `tools\cli pack explain <section.key> --config <path>`
  shows which layer sets it.

If nothing changed in config this release, write "No config changes."

## Save version changes

| Save version | Payload | What changed | Old saves |
|---|---|---|---|
| `<N>` | `<payload>` | `<what it stores now>` | Load whole / load with a default / reset |

- Saves from before this release load: `<yes, every payload / no: list>`.
- A reset is needed: `<no / yes: the one announced reset, and what it wipes>`.
- After the upgrade, one restart must write and reload a save (manual). Watch for
  `save loaded: version <N>` in the console log.

## Migrations

`<None.>` / `<The config migration <id> runs at boot. The original file is backed
up to <path> first.>` Config migrations run at boot once they exist (#303).
`config-check` lists the ones that would run (#304).

## Manual steps

Run these in order. Each one names what to observe.

1. `<step>`. Observe: `<line or screen>`.
2. `<step>`. Observe: `<line or screen>`.

These come from the `## Manual testing` sections of the PRs in this release. A
step that automation cannot reach is listed here, not left in the PR.

## Rollback

To go back to `<previous x.y.z>`: stop the server, put the copy from "Before you
start" back, and restart on the old build. If a save was written by this
release, the old build may not load it; restore the saves from the copy. See
[release-process.md](../release-process.md) ("Rollback").
