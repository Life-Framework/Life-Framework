# Dedicated server session

One session, run later on a worktree's own ports. This pass does not boot the
server. The release integrator is in Workbench play. The gate is fixture logs:

```sh
node tools/dedicated/session.mjs --selftest
```

That command does not start ArmaReforgerServer. `node tools/dedicated/session.mjs`
prints the session and the launch flags. It does not start the process either.

## The session

1. Dedicated server up.
2. One player is in game (`[LFDebug:CharacterCreation] spawning player`).
3. A second player joins after the first is in game (`flow start for player` or
   `spawning player` for a different id, before the save).
4. A save (`save point created` or `save completed`).
5. The process stops (`session ending, engine shutdown save expected`) and
   comes back (`resolved SCR_PersistenceSystem`).
6. A load (`loading the latest save point`) on that second process.

The load must show four lines, in this order, after the load starts:

- `[LFDebug:Persistence] applied N account record(s)`
- `[LFDebug:Persistence] applied N bank account record(s)`
- `[LFDebug:Persistence] applied N organization record(s)`
- `[LFDebug:VehicleLock] lock state -> locked`

`N` is at least 1. The same four lines from before the restart do not count.

## Ports

The server listens on 2001 whatever `-bindPort` says: `-server <world>` mode
ignores it (see `docs/worktrees.md`). Clients join `127.0.0.1:2001`. Only one
server runs on the machine at a time; the CLI's `server-2001` lock enforces
that. The restart passes `-loadSessionSave`.

## Fixtures

| Log | Result |
|---|---|
| `tools/dedicated/fixtures/missing-joiner.log` | Fails. The late joiner is absent. The save, the restart, and the four load lines are present. |
| `tools/dedicated/fixtures/restart-load.log` | Passes. The second player joins after the first is in game, and the load shows the account, the bank balance, the organization, and the vehicle lock. |

`node tools/dedicated/session.mjs --check <console.log>` judges a log the same
way. It still does not start the server.
