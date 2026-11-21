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

## Alpha pacing (alpha only, remove for a live server)

These keys in `config.json` speed up the first hours so testers can try every path in one evening (0.2.0, 2026-11-21).
Delete them and run `tools\cli host config apply` to go back to the shipped pace.

| Key | Alpha | Shipped | Effect |
|---|---|---|---|
| `jobs.xpAwards` | every award x2, `delivery_run` 250 | `delivery_run` 100, others half | fuel runs open after 2 parcel runs (5 shipped), taxi after 6 (15) |
| `licences.permitWaitDays` | 0 | 1 (2 real hours) | the firearm permit is issued at the counter at once |
| `vehicles.buyPercent` | 80 | 100 | the cheapest car is $560 (shipped $700); sell-back 50% and chop 30% stay below it |
| `police.alphaInstantApproval` | true | false | a police application at the station desk is approved on submit: no playtime gate, cooldown, trial or admin |

Pay is untouched: police duty $900/h and medic $792/h stay 1.5 to 2.5x the civilian runs (`tools\cli economy`).
`validate-flagship-job-gates` still refuses `jobs.testUnlockAll`; the alpha switch opens only the police application.

## Host-only values (never in this folder)

| Value | Where it lives instead |
|---|---|
| `game.password` (the alpha gate) | the host's `secrets.json` |
| `game.passwordAdmin` | the host's `secrets.json` |
| `game.admins` (administrator account uids) | the host's `secrets.json`; `tools\cli staff roster admins` prints them from `staff.txt` |
| `staff.webhookUrl` (staff notifications) | the host's `secrets.json` |

The three `game.*` values are empty here and the `staff` section is absent. `tools\cli host config apply`
merges the host's secrets over these files when it writes the profile copy; it refuses a repo file that holds one.

`game.mods` holds the two Workshop IDs (the `.gproj` GUIDs). After publishing, `tools\cli host config apply --mods <core>,<pack>`
confirms them for a host; `tools\cli host start --public` (the backend-listed server) refuses until it has
(`docs/owner/host-the-flagship.md`).

## Checks

`node tools/validation/flagship-config-selftest.mjs` (part of `tools\cli validate`) checks that `config.json` passes the owner-config
check (`docs/config/config.schema.json`), carries the values above, that `server.json` has the exact name and the example's scenario and mods,
and that neither file holds a password, an admin uid or a URL.
