# Market (price curve)

Design context: `addons/LifeFramework/Scripts/Game/Feature/Market/Feature.md`.

### `LF_PriceCurve` — `Feature/Market/LF_PriceCurve.c` (#2844, framework #762)
- ✅ Pure price-curve rules in whole cash units (merged, in-dev). A good has a base, floor and ceiling, a drop per unit sold (`dropPerUnitPct` of the base, at least 1 unit when above 0) and a recovery per real hour (`recoverPctPerHour` of the gap to the base, at least 1 unit, never past the base). `PriceAfterSale` never goes below the floor; `BulkTotal` prices each unit marginally, so a bulk sale of N pays the sum of N single sales; a drop of 0 keeps today's fixed price. `Recover` pays whole real hours only and counts a gap past `maxRecoveryHours` as the cap (`IsClamped` tells the caller to move its stamp to now). All int maths through `LF_Utils.MaxInt/MinInt/ClampInt` (#2787). Static, no world, no state. Test `market/price-curve` (LOGIC).

### `LF_MarketConfig` — `Feature/Market/LF_MarketConfig.c`, `Configs/Market/LF_Market.conf` (#2844)
- ✅ Per good `m_sGood` (item prefab, the trader list's key), `m_iBase`, `m_iFloor`, `m_iCeiling`, `m_iDropPerUnitPct`, `m_iRecoverPctPerHour`; global `m_bEnabled` and `m_iMaxRecoveryHours` (168). The core ships the market **off** (`m_bEnabled` 0) with three neutral goods (copper bar, iron bar, gold bar), so no server changes behaviour until it opts in. `Sanitize` at load removes a good with a floor above the ceiling, a base outside them, a percent outside 0..100, a repeat or no prefab, each with `LF_Debug.Error("Market", ...)` naming the good. Nothing reads the curve yet: the per-good state, the save and the trader sale are later cards of the chapter.
