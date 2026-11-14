# Checking your owner config before a restart

The server reads your `$profile:LifeFramework/config.json` at boot. A mistake there does not stop
the server: a bad value is refused and the shipped one stays, a bad section is skipped, an unknown
key is ignored. You only find out from the log. `config-check` tells you first, offline, with no
game and no server running.

```
tools\cli config-check path\to\config.json
```

It prints one line per problem, `line:column`, in the words the server logs at boot:

```
config-check: config.json
WARN   3:3  config.json: unknown top-level key mystery; ignored
ERROR  5:24  shops.defaultBuyPrice is not a whole number; keeping the shipped value
ERROR  6:25  shops.defaultSellPrice = -5 is below the minimum 0; keeping the shipped value
WARN   7:5  shops.bogus is not a known key; ignored
config-check: 2 error(s), 2 warning(s)
```

Exit code 1 when there is any error, 0 when there are only warnings or nothing. `--strict` makes
warnings fail too, `--no-loops` skips the profit-loop check, `--json` prints the problems as JSON
for a script, `--schema <file>` checks against another schema file.

## Migrations (what an upgrade would do to your file)

After the problem lines, `config-check` prints a `Migrations:` block: which config migrations the
server would run at boot to bring your file to the schema it reads, in order, with a one-line
description each and every field a step renames, drops or defaults (and whether your file has it).
It says `none needed` when the file is current or has no `schemaVersion`, and a file written for a
newer schema is refused (`refused, no migration can run`, plus the usual error). It is a dry run:
it never writes the file. At boot the server copies the file to `config.json.bak-v<N>-<time>` first.

```
Migrations: config.json: schemaVersion 1, this build reads 3; 2 would run, in order:
  1. v1 -> v2  shop-to-shops: Rename the shop section to shops.
       rename shop -> shops (in your file)
       drop legacyFlag (in your file)
  2. v2 -> v3  add-price-floor: Give shops a default minimum price.
       default shops.priceFloor = 1 (missing from your file; added)
  Dry run: nothing was written. At boot the server first copies the file to config.json.bak-v1-<yyyymmdd-hhmmss UTC>, then writes the migrated file.
```

The steps are described in `docs/config/config.migrations.json` (one entry per step in
`LF_ConfigMigrator.BuildShipped`; the selftest fails when the two differ). `--migrations <file>`
reads another manifest. Paste the block into the release's upgrade page (`docs/upgrades/TEMPLATE.md`).

## What it reports

| Problem | Level | What the server does |
|---|---|---|
| JSON syntax error (missing comma, trailing comma, comment, unclosed string) | error | The whole file is ignored; every section uses the shipped defaults |
| A section that is not an object | error | That section uses the shipped defaults |
| A value of the wrong type (`"cheap"` for a price, `"yes"` for true or false) | error | The value is refused; the shipped one stays |
| A number outside the allowed range (shown in the message) | error | The value is refused; the shipped one stays |
| `schemaVersion` above what the server reads | error | Fields the server knows still apply, the rest are ignored |
| `schemaVersion` below 1 or not a whole number | error | Read as version 1 |
| `schemaVersion` older than the server's | warning | Migrated at boot; the `Migrations:` lines list the steps |
| A key the server does not read (top level, in a section, or in a record) | warning | The key is ignored |
| A record key no shipped record has | warning | It adds a new record; a typo would sit unused |
| A record key the server cannot parse (an empty one, a malformed `type:` key) | error | The record is skipped |
| A profit loop the file's prices create | warning | Nothing; players can make money for nothing |

A profit loop is a buy-then-sell cycle that pays: an item that buys for less than it sells for, a
machine whose bought inputs cost less than its output sells for, a car bought then sold back. The
check uses the same prices as the repo's `validate-profit-loops` (`tools/economy/profit-loops.mjs`)
with your `shops` section applied, leaving out the values the server would refuse. A loop the shipped
prices already make is not reported as yours. It points at the `shops` section, because a loop is
the sum of several prices.

## Where the rules come from

Types, ranges and the set of known keys are read from `docs/config/config.schema.json`, which is
generated from the config classes (`node tools/config/schema.mjs --write`). A new field, or a
changed `params: "min max"` range, reaches this check with no edit to it. The two things the schema
cannot say live in `tools/config/check.mjs`: the grammar of a record key (`shops.priceRules` accepts
a prefab-path substring, `prefab:<GUID>` or `type:<flags>,mode:<mode>`) and where shipped record keys come from.

`docs/config/config.example.json` is a file with no errors (its vehicle record is a warning, because
it adds a record). Proof of the rules:
`node tools/validation/config-check-selftest.mjs`.

## Fixture harness

`tools\cli config-harness --stage validate` runs `config-check` over `tools/test/config-fixtures/`
(`presets/` pass clean, `broken/` has one file per reject class), each with a `.expect.json` holding
the exact log line, line and column. Stages are a list in `tools/config/harness.mjs`; the server-boot
(#299) stage plugs in there. Proof: `node tools/validation/config-harness-selftest.mjs`.

### Changing prices, removing an item (#1005)

`tools/test/config-fixtures/presets/client-economy-override.json` is the worked owner file: it patches the food shop price, removes the M9 from the gun counter and patches the S105's lot price. The same three edits as pack overlays are in `tools/test/config-fixtures/pack-economy/` (`ShopPrices.conf`, `Catalogue.conf`, `VehiclePrices.conf`; copy into a pack's `Configs/` and list each in the mission header's `m_aLF_ConfigOverlays`). Rules an author trips on:

- A shop price rule is addressed by the key the shipped rule has (`Prefabs/Items/Food/` changes every food; a longer substring is a new rule, so give it `"op": "add"`, as `patch` of an id nobody has is refused).
- Removing a catalogue item: also `remove` its offers (`gun_counter/pistol_m9`), or `pack check` reports an offer for an item the catalogue lacks.
- A vehicle is keyed by GUID, braces and case ignored; a patch needs no `m_Prefab` change but the pack `.conf` must repeat `m_Prefab` to name the entry.
- Check offline: `config-check`, then `pack explain <key> --config <file>` shows the layer that set the value; `pack check <addon>` for a pack.

### Client stage (#300): is the value on the client?

A fixture's `.expect.json` may carry `"client": [ read, ... ]`. Each read names a value a player sees and
what it must be: `shopPrice` (the shop's list, searched for an item, shows `expect`, for a config price or a
renamed currency term) or `actionShown` (an action is offered or hidden, for a switched-off system), `shopPrice` with `"gone": true` (an item an owner or pack removed is not listed) and `lotPrice` (the car lot's price sheet shows `expect`) or `uiText` (the sheet an action opens shows `expect` and not `absent`, for a renamed term: `client-term-sheriff-gang`, `client-term-language-match`, `client-term-language-other`). Run it with `node agent-box/session.mjs config-harness-proof` (a proof verb, not `harness.mjs`).
`tools/test/config-fixtures/presets/client-shop-price`, `client-term-currency` and `client-system-off` are the
three, `empty` is the shipped baseline for both kinds of read.

```
tools\cli session up --clients 1
tools\cli session config-harness-proof [--only <name>]
tools\cli session down
```

The proof writes each fixture as the server's owner `config.json`, runs `configreload` (the server streams it),
and client 1 reads the value off its own screen. One running server serves every fixture in turn; a server per
fixture is #299. About 1 minute for four fixtures.

## Where a value came from

```
tools\cli pack explain catalogue.items.pistol_m9 --config path\to\config.json
catalogue.items pistol_m9: present
  category = "weapon"  <- core
  price = 650  <- pack LF_Everon1989 (Configs/Catalogue.conf)  (overridden: core 500)
  sellPrice = 111  <- owner config.json  (overridden: core 250)
  trail: core add > LF_Everon1989 patch > owner patch
```

The key is `<section>.<list>.<id>[.<field>]` (`shops.priceRules.type:64,mode:2.buyPrice`) or a plain
section key (`shops.defaultBuyPrice`). `crime.crimes` alone lists every id a pack or your file
changed. The layers are the core `.conf`, each pack under `addons/` in order (`--pack <addon>` for
one), then your file (`--config`); the merge is the server's (`docs/config/override-model.md`), so a
refused verb and a second pack on one id print under the entry. A field no layer sets is the class
default. Offline; exit 1 when the key names nothing. Lists today: `catalogue.items`,
`catalogue.offers`, `crime.crimes`, `shops.priceRules`, `vehicles.vehicles` (key: the lot prefab's 16-digit GUID, `vehicles.vehicles.54C3CC22DEBD57BE.basePrice`). The website config check (#281) does not
have this view yet.

## Fixtures a reload cannot apply (boot fixtures)

`tools\cli config-harness --stage boot` boots a fresh dedicated server for each fixture whose sidecar says `"boot": true` and checks the server log against the sidecar's `bootLog` (`has`: text a line must contain, `absent`: text none may). The other fixtures run on one shared server with `configreload` (`session config-harness-proof`), which is faster. Use a boot fixture for a value read once at start: the autosave timer (`persistence`), a pack overlay, a system a world switches off at boot. `--only <name part>` runs one. It uses the pool, so it takes whichever machine has room; it is not part of `wt gate`.

## Settings a pack adds

A content pack can add its own sections to your `config.json` for settings the core sections do not
cover (#553). The pack declares them; you set them like any other section. They are named from the
pack: `LF_Everon1989` adds `everon1989` (or `everon1989Chains` and so on).

```json
{ "everon1989": { "festivalDay": 5, "chainsEnabled": false } }
```

Everything above applies the same way: a value of the wrong type or outside its range is refused and
the pack's default stays, an unknown key in the section is warned and ignored, and `config-check`
names the field. `docs/config/config.schema.json`, `config-check`, the website generator and the
reference page list a pack section after the core ones, marked "Pack <addon>: ...". Without that pack
installed, the key is an unknown top-level key and is ignored. A pack section can never replace a core
section; a pack that tries is ignored for that section with an `[LFDebug:Packs]` error.

`tools\cli pack explain everon1989 --config config.json` lists each field with the pack default and
your value; `pack explain everon1989.festivalDay` shows one. A value the server would refuse prints
as `refused: owner config.json sets 9; 9 is above the maximum 7; the pack default stays`.

A wrong-typed value in a pack section is an ERROR in `config-check` (`sectest.limit is not a whole
number; keeping the shipped value`), not a warning; the server keeps the pack default.

A pack kept outside `addons/` (a test pack in `tmp/`) is checked the same way with `--addons <dir>`
(the core plus the packs under `<dir>`) and explained with `pack explain --root <dir>`:

```
tools\cli config-check path\to\config.json --addons path\to\test-root\addons
tools\cli pack explain sectest.limit --root path\to\test-root --config path\to\config.json
```

Both are offline and need no game boot: the section is read from the pack's manifest, not the server.
Proof: `tools/validation/pack-check-selftest.mjs`.
