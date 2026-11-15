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

## Worked check: re-theme the town (four overlays at once)

`pack check` and the owner `config.json` route each prove one overlay. This boots all four in one pack
(issue #1005): a recoloured UI theme (`LF_UIThemeConfig`), a price change and a removed item
(`LF_ShopPricesConfig`, `LF_ItemCatalogueConfig`), a prefab slot swap (`LF_PrefabSlotsConfig`) and a renamed
term (`LF_TermsConfig`). The fixture is `tools/test/pack-fixtures/retheme/`; the pack it makes is generated
and not committed.

```
node agent-box/session.mjs retheme-proof --prepare   # pack new Retheme + the fixture + regen-localization + pack check
tools\cli session up --pack LF_Retheme --clients 1 --profile full
node agent-box/session.mjs retheme-proof             # one PASS or FAIL line per overlay
node agent-box/session.mjs down
```

What client 1 reads: the water line at `$31` (the core's is `$6`), the gun counter with no M9 line (shipped
`$500`), the police application titled "Application to the Sheriff", the dropped item's marker as
`LF_EvidenceCasing` (the harness `seen` read's `evidence=` token; the slot swap), and a picture of the price
list whose face is the pack's paper colour. The proof then reads the logs: the economy, slot and terms overlays
log `pack overlay ... changed N` on the server, the theme on the client (a dedicated server has no UI).

Things a pack author trips on, found by this check:

- A term form or any text of your own is a key in `Language/<name>_localization.st`, and the engine reads the
  runtime tables beside it. After adding a row run `tools\cli regen-localization` (it covers a `pack new` pack
  since #1005); without it the key shows as "Missing string ID" and the term has no words.
- `LF_TermsConfig` is on the public surface (a pack may overlay terms).
- A scaffolded pack in `addons/` is loaded by every launch of that worktree, so four launch-list selftests
  (`session`, `host`, `validate-addonsdir`, `validate-rdb-fresh`) fail while it is there. Delete it
  (`node tools/safe-rm.mjs addons/LF_Retheme`) before `validate` or `wt ship`.

## Not covered

- `test --pack`: the in-game suite boots the core mission on purpose; a pack proof is a session.
- A pack with its own world: set `World` in the header to it. `--pack` boots whatever the header names.
