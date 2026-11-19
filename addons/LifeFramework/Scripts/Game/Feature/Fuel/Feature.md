# Fuel - Design Requirements

> Context file for fuel station stock (#3327, framework #788).

## Intent

A fuel station holds real litres that a delivery job refills. A sale is refused when the station is empty, and a
delivery pays only for the litres that fit. A server turns it on; the core ships it off.

## Interaction pattern

Rung 4 background: pure rules and a config, no screen, no world, no replication in this card. The pump action, the
delivery drop-off and the stock sign are later cards.

## Current state

- `LF_FuelStockRules` (`LF_FuelStockRules.c`): static and pure, ints. `PlanSale` clamps by stock, then by money;
  `PlanDelivery` clamps by the room left; `PlanSaleFor` reads the config. A plan names what would move
  (`m_iLitres`, `m_iCash`, `m_iCarryAfter`) and changes nothing; `ApplySale` and `ApplyDelivery` change the
  `LF_FuelStock`. Price 0 is the kill switch. Prices are hundredths of a cash unit per litre, and
  `LF_FuelChargeLedger` keeps the sub-unit remainder per key (unsaved on purpose).
- `LF_FuelConfig` (`LF_FuelConfig.c`, `Configs/Fuel/LF_Fuel.conf`): one table, off in the core. Per-station rows and
  pack overlays arrive with the station component.

## Not yet

Station component, pump and delivery actions, the `fuelstock` save section (a `LF_<Area>SavePayload`), the
out-of-fuel sign.
