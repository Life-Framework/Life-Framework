# Shop — Design Requirements

> Context file for the commerce engine and its physical storefronts. Written
> against `docs/design-philosophy.md` (in-world first). Covers the generic
> purchase engine and every store type built on it.

## Intent

The player does their shopping **in the world**. A clothing store has racks and
mannequins, a supermarket has shelves of product, a weapon store has weapons on
the wall, a vehicle dealer has an actual car lot. The player walks up to the
goods and buys them — not a menu they could have opened from anywhere.

The pattern we are explicitly rejecting: "open the shop menu anywhere and buy
from a list." We have played those mods and we are tired of them.

## Interaction pattern (in-world)

Today the Shop feature is a world-anchored menu (rung 3): an `EL_ShopComponent`
on an entity, an `EL_ShopAction` that opens `EL_ShopMenu` within 3 m. The target
is rung 1–2 — **physical goods are the purchase point**.

The ladder per store type:

| Store type | V1 | Target interaction |
| --- | --- | --- |
| Clothing store | shop sign, scroll to buy | garment on a rack / on a mannequin is the purchase point; scroll to cycle colors; preview color on the mannequin |
| Supermarket | shop sign, scroll to buy | product physically on shelves; scroll on the product to choose item + quantity |
| Weapon store | shop sign, scroll to buy | weapons laid out on racks/walls; walk up and take/swap |
| Vehicle dealer | menu at a sign | actual car lot — cars in a few colors, walk to the car, sit in it / open it to buy |
| General shop | shop sign, scroll to buy | mixed goods on shelves and counters |

### The mannequin concept (clothing)

A clothing store can't hold a thousand shirts. The answer is a mannequin (or a
rack) per garment line:

- The mannequin wears the base item.
- Walking up gives an in-place scroll: color variants of that garment.
- Selecting a color *previews* it on the mannequin before purchase.
- Buying delivers that color variant to the player's inventory.

This is rung 2 and is the technical centerpiece of the clothing store. V1 of it
is: a mannequin entity with a scroll menu of variants. Later: live preview,
sizing, layered clothing.

### Vehicle dealer (car lot)

- The lot is populated with the vehicles on offer, one per config, in a couple
  of colors each.
- The player walks to a car and buys it (sit in it / interact).
- Buying delivers the registered vehicle (see `VehicleLock` and
  `LicensePlate`), not a raw entity — a car you can own, lock, and plate.

### Stores are building overrides

Each store type is a drop-in prefab built on a Reforger building (shop, market
hall, supermarket, garage). Replacing the building on the map populates the
location with working purchase points and its price data. One prefab per store
type; no per-map scripting. See `docs/design-philosophy.md` §4.

## V1 (shippable)

1. Fix the purchase engine's money correctness first — the partial-funds and
   refund over-credit exploits in `EL_ShopComponent.BuyItem` (see
   `docs/features.md` §Money & Economy) block anything else.
2. Ship one physical store type end-to-end as the reference (supermarket is the
   cheapest: shelf product + scroll). The other store types reuse the same
   engine.
3. Store prefab(s) inherit Reforger buildings and come with their own price
   config (`Configs/Shop/ShopPrices.conf`).

## Iteration path

- **V2** — physical racks/shelves for supermarket and general goods; item object
  carries its own price so a populated store needs no external list.
- **V3** — mannequin + color variants + preview for clothing.
- **V4** — vehicle car lot with owned-vehicle handoff; weapon racks.
- **V5** — NPC clerks (buying handled through the merchant), store stock with
  restock, buyback of worn/used goods.

## Current state

- `EL_ShopComponent` — catalog-driven buy; **money exploit** (partial
  `RemoveAmount` treated as full success, refund over-credits). Infinite stock.
- `EL_ShopMenu` / `EL_ShopAction` — list menu within 3 m, opens for the local
  player regardless of who interacted.
- `EL_ShopPriceResolver` / `EL_ShopPricesConfig` / `EL_ShopRules` / `EL_ShopItem`
  — price resolution and rules scaffolding.
- `Configs/Shop/ShopPrices.conf` — the price table.

## Dependencies

- `Money` (physical cash as the tender).
- `Trader` (sell-side; same physical-store philosophy).
- `Banking` / `ATM` (store takings, if a shop has a banked float).
- `VehicleLock` / `LicensePlate` for the vehicle dealer handoff.
- `Quantity` (stacking purchased goods).