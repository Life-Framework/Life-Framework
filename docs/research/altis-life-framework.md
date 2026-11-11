# Altis Life Framework: config reference

What the config layer of the Altis Life framework (AsYetUntitled/Framework,
v5.x, last commit 2021-11-17) tunes, read from a local clone of
`Altis_Life.Altis/config/`. Read it with the [Life/RP benchmark](life-rp-benchmark.md).

**Nothing here was imported.** The framework is CC BY-NC-ND 4.0 (`LICENSE.md`):
no derivatives, no commercial use. This note records design facts (which knobs
exist, what shape the data has) and a few magnitudes. No config text, class
names, or SQF were copied. Numbers are Arma 3 server defaults, not a
recommendation for our economy.

## What the config layer is

Thirteen `.hpp` files, one per concern. The shape is the lesson: every system
is a table a server owner edits, and the scripts read it.

| File | Tunes | Size |
| --- | --- | --- |
| `Config_Master` | One settings class: logging, persistence flags, economy, jail, revive, paychecks, vehicle multipliers, wanted crimes | about 17 KB |
| `Config_Licenses` | Licence list: price, legal or illegal, side | 4 KB |
| `Config_Process` | Processing recipes: inputs, outputs, text, unlicensed fee | 3 KB |
| `Config_Gather` | Gather zones and mining tables with percent splits | 4 KB |
| `Config_vItems` | Virtual item shops, per-shop item lists and gating conditions | 83 classes |
| `Config_Vehicles` | Vehicle shops and prices | 63 classes |
| `Config_Housing`, `Config_Garages` | Buyable buildings, crate counts, garage spawn offsets, per-map | 9 KB, 2 KB |
| `Config_SpawnPoints` | Spawn choices per side, with a condition string | 8 KB |
| `Config_Clothing`, `Config_Weapons`, `Config_Loadouts` | Side shops and starting gear | 17, 9, 3 KB |
| `Config_SpyGlass` | Anti-cheat allow lists | 130 KB |

## Patterns worth knowing

- **Conditions are strings.** Shops, spawns, and licences gate on a code string
  evaluated at runtime (side, rank, licence flags). Flexible, but untyped and
  unlintable. We gate with typed conf fields and `LF_` predicates instead.
- **Licences are flat data.** Each has a price, an illegal flag, and a side.
  Three groups: civilian (driving, boat, pilot, trucking, firearm, diving,
  home), processing (oil, diamond, salt, sand, iron, copper, cement), illegal
  (cocaine, heroin, marijuana, rebel). Prices run 500 to 75,000.
- **Processing has a pay-or-licence fork.** Each recipe is input list, output
  list, and a `NoLicenseCost`, so an unlicensed player can still process by
  paying a fee. That is a cheap way to turn a licence from a wall into a
  price. Fees are small beside licence prices (350 to 1,750).
- **Gathering is zone plus item plus split.** A mineral zone needs a tool and
  rolls a weighted table of outputs (the doc comment's example splits 25/70/5).
  Plain crops and drugs have a fixed amount per zone.
- **Master economy knobs.** Starting bank by side (3,000 civilian, 6,500
  medic, 7,000 cop); paycheck by side every 5 minutes (350, 450, 500);
  5% bank transfer tax; revive fee 1,500 paid to the medic; gang creation
  75,000; house limit 5; Federal Reserve robbery needs 5 cops online; a
  10-minute deposit lock after selling stolen gold.
- **Vehicle prices are one table with per-side multipliers.** Buy, rent, sell,
  chop-shop, impound, and storage fee are each a multiplier on one base price
  (civilian buy 1.0, cop 0.5; chop shop 0.25; impound 0.1). The config warns
  that chop and impound multipliers must stay below buy and rent, or players
  loop them for profit. Worth a test in ours.
- **Crimes are a bounty table.** A code, a localised name, a bounty; roughly
  forty rows from 70 to 15,000.
- **Persistence is opt-in per field.** Separate flags for virtual items,
  player stats, civilian weapons, civilian position, and each vehicle
  property. Defaults are conservative (position, weapons, and vehicle state
  off). Logging has three switches: money, death, advanced.
- **Housing is per-building-class.** Price, crate count, restricted interior
  positions, optional garage with a spawn offset. Garages are a separate table
  bought on top.

## Where we already differ

| Altis Life | Life Framework |
| --- | --- |
| Virtual items in a database | Real inventory items; cash is a physical stack |
| Wanted list by code string | Persisted wanted on the player account |
| Condition strings | Typed conf fields |
| One flat settings class | Core and pack split ([core vs pack](../architecture/core-vs-pack.md)) |

## Take-aways for roadmap scopes

1. A pay-instead-of-licence fee on processing is cheap to add and worth
   considering for the 1989 pack's processing chain.
2. Keep every payout multiplier below its matching purchase price, and add a
   logic test that asserts it.
3. Per-side starting money and paycheck belong in pack config, not core.
4. Make persistence opt-in per field in the same way; we already version the
   payloads.

## Server code: patterns and mistakes

Read from `life_server/Functions/` and `Altis_Life.Altis/core/session/` on
2026-11-10. File names and behaviour only.

**Worth adopting** (filed as M3 epic #684):

- **Crimes are codes.** Wanted entries point at a crime table, so the charge
  list, the bounty and the display all come from one place (#685).
- **Prices derive from one base.** Vehicle buy, sell, chop, impound and storage
  are multipliers, and the config itself warns about buy-sell loops (#686,
  #688).
- **Owners choose what persists.** Separate switches for position, gear, fuel,
  damage and trunk (#689).
- **A cleanup loop.** An hourly server sweep removes empty, unwatched or wrecked
  vehicles; disconnects remove the body and nearby dropped containers (#690,
  #691).
- **Playtime is tracked** per player and side (#692).
- **Partial saves.** A save request names one field (cash, bank, licences, gear,
  position, jail) instead of the whole record. Our engine persistence differs,
  but the idea of saving only what changed holds.

**Mistakes to avoid:**

- **The client reports its own cash.** The partial save sends the client's cash
  and bank values to the server, which writes them. A remote-exec allow list
  (`CfgRemoteExec`) limits which functions can be called, not what values are
  sent. Our rule is the reverse: the server owns money (#693 enforces it).
- **Keys live in one global variable per player** on the server, keyed by a
  formatted string. It works, but nothing persists or validates it. Our keys are
  items (M2 #613).
- **Anti-cheat by scanning the client** (SpyGlass, BattlEye filters). It is an
  arms race; server authority removes most of the need.
- **A headless client for database work** offloads queries from the server. It
  adds a second authority; Reforger's persistence makes it unnecessary for us.
