# Shop — Design Requirements

> Context file for the commerce engine and its physical storefronts. Written
> against `docs/design.md` (in-world first). Covers the generic
> purchase engine and every store type built on it.

## Intent

The player does their shopping **in the world**. A clothing store has racks and
mannequins, a supermarket has shelves of product, a weapon store has weapons on
the wall, a vehicle dealer has an actual car lot. The player walks up to the
goods and buys them — not a menu they could have opened from anywhere.

The pattern we are explicitly rejecting: "open the shop menu anywhere and buy
from a list." We have played those mods and we are tired of them.

## Interaction pattern (in-world)

Today the Shop feature is a world-anchored menu (rung 3): an `LF_ShopComponent`
on an entity, an `LF_ShopAction` that opens `LF_ShopPriceList` within 3 m. The target
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
type; no per-map scripting. See `docs/design.md` §4.

## V1 (shippable)

1. Fix the purchase engine's money correctness first — the partial-funds and
   refund over-credit exploits in `LF_ShopComponent.BuyItem` (see
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

- `LF_ShopComponent` — catalog-driven buy; **money exploit** (partial
  `RemoveAmount` treated as full success, refund over-credits). Infinite stock.
- `LF_ShopPriceList` / `LF_ShopAction` — typed price list with an order slip
  on the UI kit, within 3 m, only for the player who used the counter; the
  till's refusal reason (`LF_EShopRefusal`) prints on the slip.
- `LF_StoreComponent` / `LF_Stores` / `LF_StoreRules` / `LF_StoreTillAction` (#202) — the physical
  store: shelf stock, unpaid tags, the till and receipt, the shoplifting sweep, restock and
  save (`storestock`, save version 21). Unpaid tags survive a restart and a logout (`storeunpaid`,
  save version 25, #479): keyed by the carrier's character key and the prefab, put back on the goods
  at spawn, not swept while the store's grace (`m_iRestoredGraceSeconds`) lets the carrier come back and pay. Flagship: the DebugWorld general store.
  Follow-ups: the market and the car lot on the same pattern (design note on #479), receipt text.
- `LF_ShopPriceResolver` / `LF_ShopPricesConfig` / `LF_ShopRules` / `LF_ShopItem`
  — price resolution and rules scaffolding.
- `Configs/Shop/ShopPrices.conf` — the price table.

## Dependencies

- `Money` (physical cash as the tender).
- `Trader` (sell-side; same physical-store philosophy).
- `Banking` / `ATM` (store takings, if a shop has a banked float).
- `VehicleLock` / `LicensePlate` for the vehicle dealer handoff.
- `Quantity` (stacking purchased goods).

## Back room door (#933)

- **What the room is today:** a market stall (`LF_ShopComponent.m_bBackRoom`) with an M9 for 500
  cash. No hour, licence, contact or key gates it; the only closed state is the shutter a police
  search (#600) puts down for `LF_ShopLimitsConfig.BackRoomShutMs()`. The door says that.
- **Player sees:** the prompt *Knock to buy a handgun*; *Back room (shut)* with the reason while
  shut; the general store's sheet carries the shopkeeper's pointer (`m_sShopHint`, a key); the
  chalkboard card gives the price and the one rule that closes it.
- **Owner and side:** `LF_BackRoomDoor` is the pure wording. `LF_ShopAction` applies it on the
  client from `LF_ShopComponent.IsShutShown()`, an `RplProp` the server sets when a raid shuts
  the room and clears when the shut time passes. The server's `m_iClosedUntilMs` stays the
  authority: a buy or sell is still refused there with `#LF-Shop_RefuseClosed`.
- **Data:** keys `LF-Shop_Open_BackRoom`, `LF-BackRoom_DoorShut`, `LF-Shop_RefuseClosed`,
  `LF-Shop_Hint_General`, `LF-Sign_BlackMarket_*`; nothing persists (a restart reopens the room).
- **Proof:** `shop/back-room-door` (LOGIC), `node agent-box/session.mjs back-room-door-proof`.
