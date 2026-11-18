# Market (price curve) - Design Requirements

> Context file for the market price curve (#2844, framework #762, which supersedes #748). Chapter: M6 "The market moves".

## Intent

What a trader pays for a good moves with what players sell: each unit sold pushes the price down, and the
price creeps back to its base while nobody sells. A server turns it on; the core ships it off so no
server changes behaviour until it opts in.

## Interaction pattern

Rung 4 background: pure rules and a config, no screen, no world, no replication in this card.

## Current state

- `LF_PriceCurve` (`LF_PriceCurve.c`): static and pure, whole-cash ints. `DropPerUnit` (percent of the base, at
  least 1 when the percent is above 0), `NextPrice`, `PriceAfterSale`, `BulkTotal` (marginal: the total equals the
  sum of N single sales, closed form, capped at `MAX_BULK_UNITS`), `RecoveryHours`, `IsClamped`, `Recover` (each
  whole real hour closes `recoverPctPerHour` of the gap to the base, at least 1 unit, never past the base).
  All int maths goes through `LF_Utils.MaxInt/MinInt/ClampInt` (#2787).
- `LF_MarketConfig` (`LF_MarketConfig.c`, `Configs/Market/LF_Market.conf`): `m_bEnabled` (0 in the core),
  `m_iMaxRecoveryHours` (168), and per good `m_sGood` (item prefab, the trader list's key), `m_iBase`, `m_iFloor`,
  `m_iCeiling`, `m_iDropPerUnitPct`, `m_iRecoverPctPerHour`. `Sanitize` runs at load: a floor above the ceiling,
  a base outside them, a percent outside 0..100, a repeated or empty good is removed with
  `LF_Debug.Error("Market", ...)` naming the good. Three neutral goods ship (copper bar, iron bar, gold bar).

## Decided

- Prices are ints. Recovery runs on real server seconds (unix time), so it survives restarts and offline gaps;
  a gap past `maxRecoveryHours` counts as the cap (the caller then moves its stamp to now).
- The curve moves only on player sales; shop buy prices stay fixed in M6.

## Not built yet (later cards of the chapter)

The save of the per-good price state (#2846; `LF_MarketBook` and the trader sale landed in #2845), the pack overlay and owner
`config.json` section, and any screen.

## Dependencies

`Core/Config` (the record base), `Core` (`LF_Utils` int helpers, `LF_Debug`, `LF_StaticResetHook`).
