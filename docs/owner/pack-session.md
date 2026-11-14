# Boot your pack in a test session

A pack changes the game through its mission header: the config overlays (prices, terms,
dialogue, prefab slots), signs and strings. `session up --pack` boots your pack the way a server
would, so you can check a change in the game and read it on a client instead of guessing.

## The command

```
tools\cli pack new DemoPack                    # once: writes addons/LF_DemoPack/
tools\cli session up --pack LF_DemoPack        # from a worktree (tools\cli wt new <slug>)
node agent-box/harness.mjs --client 1 ui text  # read what client 1 sees
node agent-box/session.mjs down                # close the session when done
```

`--pack` takes the folder name under `addons/` (the pack `pack new` made). It loads the core and
that pack (the flagship `LF_Everon1989` does not load) and boots the pack's own header,
`Missions/LF_<Name>.conf`, on the core `DebugWorld`. `pack new` writes that header with the
`World` and `SystemsConfig` already filled in, so a fresh pack boots with no edits. The header's
`m_aLF_ConfigOverlays` list is the same mechanism a real server uses, so an overlay that works
here works on a server.

A `--pack` session is a playtest session: the harness build (`LF_HARNESS`) is on, so
`harness.mjs` verbs and the proof verbs in `agent-box/proofs/` work. `--clients 2`, `--profile full`
for pictures, `session restart` and `session join` work as for any session; the pack is recorded
with the session, so a restart reloads it.

## Where it runs

The pack must be a folder of the worktree you run in (`addons/LF_<Name>/`). The pool carries the
worktree snapshot, untracked files included, to a remote machine, so a pack run lands on `lf-testbox`
the same way. A pack kept outside the worktree is not carried; copy it in, or run `pack new` in the
worktree.

## Worked check: change a price

1. In `addons/LF_DemoPack/Configs/ShopPrices.conf`, price drinks at 31 (the core's price is 6):

   ```
   LF_ShopPricesConfig {
    m_aPriceRules {
     LF_ShopPriceRule "{0D3E1F6A5B7C9E21}" {
      m_sFind "Prefabs/Items/Drinks/"
      m_iBuyPrice 31
     }
    }
   }
   ```

   The rule key is the `m_sFind` string; a rule with the core's key replaces its prices, a new key
   is appended. Give a new rule a GUID of your own.
2. `tools\cli session up --pack LF_DemoPack --clients 1`
3. A fresh character opens the civil registry form first. Fill it, then stand at the debug general store
   and open its price list (verbs take positional arguments), then read the client:
   `node agent-box/harness.mjs --client 1 type RegFirstName Ada`, `type RegLastName Vale`, `type RegAge 30`,
   `click SignButton`, `stand Shop_GeneralStore_Debug`, `use LF_ShopAction Shop_GeneralStore_Debug`,
   `type Search Water`, `ui text`. The Waterbottle line shows `$31`. Without `--pack` it shows `$6`.
   (`agent-box/proofs/config-stream-proof.mjs` drives the same screen.)
4. The server log line `[LFDebug:Config] shops: pack overlay ... set N fields` says the overlay applied.

## Not covered

- `test --pack`: the in-game suite boots the core mission on purpose; a pack proof is a session.
- A pack with its own world: set `World` in the header to it. `--pack` boots whatever the header names.
