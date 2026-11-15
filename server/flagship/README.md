# The flagship's live config

The two files the Life Framework | Everon 1989 (Alpha) server runs on, kept in git so every change is a PR
(`docs/ops/hotfix.md`). Nobody edits the host's copy by hand. This repo is public: **no secret is ever committed here.**

| File | What it is |
|---|---|
| `config.json` | The owner config: the shipped economy plus the flagship's `dayCycle` and `restarts` |
| `server.json` | The base game's dedicated-server config: listing name, scenario `LF_Everon1989.conf`, mod list |
| `staff.txt` | The staff roster (the maintainer is the only staff); the game reads it from `<profile>/LifeFramework/staff.txt` |

## Decided values

- Listing name: `Life Framework | Everon 1989 (Alpha)`.
- `dayCycle`: `accelerated`, `dayLengthMinutes` 120. An in-game day lasts 2 real hours (12x).
- `restarts`: `06:00` and `18:00` server time, warnings 15, 5 and 1 minutes ahead. There is no siren.
- The alpha gate is a server password (`game.password`). It is host-only.

`scenarioId` and `game.mods` equal `server/example/server.json`; change them there first.

## Host-only values (never in this folder)

| Value | Where it lives instead |
|---|---|
| `game.password` (the alpha gate) | the host's `secrets.json` |
| `game.passwordAdmin` | the host's `secrets.json` |
| `game.admins` (administrator account uids) | the host's `secrets.json`; `tools\cli staff roster admins` prints them from `staff.txt` |
| `staff.webhookUrl` (staff notifications) | the host's `secrets.json` |

The three `game.*` values are empty here and the `staff` section is absent. `tools\cli host config apply` *(planned)*
merges the host's secrets over these files when it writes the profile copy; it refuses a repo file that holds one.

## Checks

`node tools/validation/flagship-config-selftest.mjs` (part of `tools\cli validate`) checks that `config.json` passes the owner-config
check (`docs/config/config.schema.json`), carries the values above, that `server.json` has the exact name and the example's scenario and mods,
and that neither file holds a password, an admin uid or a URL.
