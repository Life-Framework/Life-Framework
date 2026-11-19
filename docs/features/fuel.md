# Fuel

Design context: `addons/LifeFramework/Scripts/Game/Feature/Fuel/Feature.md`.

### `LF_FuelStockRules` — `Feature/Fuel/LF_FuelStockRules.c` (#3327, framework #788)
- ✅ Pure station-stock rules, no world (merged, in-dev). A station (`LF_FuelStock`) holds litres up to a capacity. `PlanSale` clamps the litres by stock first, then by what the buyer can pay, and charges only for the litres that move; an empty station (under the minimum stock to sell) refuses with EMPTY, cash that cannot buy one litre with NO_MONEY, price 0 is the kill switch (OFF). `PlanDelivery` lands only the room left and pays only for the litres that landed; a full station refuses with FULL. Prices are hundredths of a cash unit per litre; the fractional part rides in `LF_FuelChargeLedger` (per key, unsaved by design), so ten litres at 0.30 cost exactly 3. Plans do not change the station; `ApplySale` / `ApplyDelivery` do. Int maths through `LF_Utils`. Tests: `fuel/fuel-stock-rules` (LOGIC).
### `LF_FuelConfig` — `Feature/Fuel/LF_FuelConfig.c`, `Configs/Fuel/LF_Fuel.conf` (#3327)
- ✅ One table: `m_bEnabled` (0 in the core), `m_iPricePerLitre`, `m_iDeliveryPayPerLitre`, `m_iCapacity`, `m_iStartLitres`, `m_iLitresPerDelivery`, `m_iMinStockToSell`. `Sanitize` switches an unusable table off with `LF_Debug.Error("Fuel", ...)`. No pump, station component or save yet: later cards. Test `fuel/fuel-stock-rules` (LOGIC).
