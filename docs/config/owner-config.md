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

## What it reports

| Problem | Level | What the server does |
|---|---|---|
| JSON syntax error (missing comma, trailing comma, comment, unclosed string) | error | The whole file is ignored; every section uses the shipped defaults |
| A section that is not an object | error | That section uses the shipped defaults |
| A value of the wrong type (`"cheap"` for a price, `"yes"` for true or false) | error | The value is refused; the shipped one stays |
| A number outside the allowed range (shown in the message) | error | The value is refused; the shipped one stays |
| `schemaVersion` above what the server reads | error | Fields the server knows still apply, the rest are ignored |
| `schemaVersion` below 1 or not a whole number | error | Read as version 1 |
| `schemaVersion` older than the server's | warning | Read as it is |
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

`docs/config/config.example.json` is a file that passes clean. Proof of the rules:
`node tools/validation/config-check-selftest.mjs`.

## Fixture harness

`tools\cli config-harness --stage validate` runs `config-check` over `tools/test/config-fixtures/`
(`presets/` pass clean, `broken/` has one file per reject class), each with a `.expect.json` holding
the exact log line, line and column. Stages are a list in `tools/config/harness.mjs`; the server-boot
(#299) and client (#300) stages plug in there. Proof: `node tools/validation/config-harness-selftest.mjs`.
