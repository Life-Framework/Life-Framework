# Add a shop to your map

This page places a shop counter on your own world and sets its prices. It is prefabs and config
only, but it needs Workbench, because a new map is a world you place.

The counter sells what you put in it at the prices in your `config.json`. A delivery job is a
separate page: [add a delivery job](add-job.md).

## 1. Have a pack and a world

If you have not made one yet, follow steps 1 and 2 of [add a delivery job](add-job.md#1-make-a-pack):
`tools\cli pack new Town` writes `addons/LF_Town/`, and the world with the roleplay game mode and a
spawn point is `addons/LF_Town/Worlds/Town.ent`. A shop needs that game mode; the counter without
it has no prices to read.

## 2. Copy the counter prefab

The core ships a working counter, `{F7EEC900E44C3CCA}Prefabs/Props/Commercial/LF_GunCounter.et`.
Duplicate it into your pack so you can change it (in the Resource Browser, right-click the prefab
and duplicate it; save it as `addons/LF_Town/Prefabs/Store.et`). Placing the core prefab directly
also works, until the day you want a different name or stock.

On the `Jobs` layer, place your copy where a player can walk up to it, and give the entity a name,
for example `Town_Store`.

The prefab carries:

| Component | Values |
|---|---|
| `LF_ShopComponent` | the shop: name, description, stock, price source |
| `ActionsManagerComponent` | one action context `default`, offset `0 0.9 0`, radius `1.6` |
| `LF_ShopAction` | the prompt in that context; its UI info name is the row the player uses |
| `RplComponent` | `Enabled 1`; a back room replicates its shutter with it |

Change these on `LF_ShopComponent`:

| Field | Set it to |
|---|---|
| `m_sShopName` | `Town Store`, or a `#` key from your pack's string table |
| `m_sShopDescription` | a line for the price list, or empty |
| `m_sOutletId` | **empty** for a shop that uses your price rules (the gun counter ships with `gun_counter`) |
| `m_aShopItems` | the stock list; empty means every priced item in the game |
| `m_bFence` | on to sell only to proven criminals; each stock row's `m_iMinCrimes` is its threshold |
| `m_bBackRoom` | on for a room the police may search and seize |

The counter's prompt is the `UIInfo` name on the `LF_ShopAction` row (`Buy a handgun` in the gun
counter). Set it to what the shop is, for example `Browse the counter`.

With a curated stock list, add one row per item: `m_ItemPrefab` is the item prefab. A row can carry
`m_sDisplayName` (a plain name or a `#` key) and `m_iMaxQuantity`. For example, the backpack:

```
m_ItemPrefab "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et"
```

`m_iPrice` on a row is **not** the price a player pays. The price always comes from your `shops`
config in the next step.

## 3. Set the prices

Prices are server config, not prefabs. Put this in `$profile:LifeFramework/config.json` (the path is
in the [install guide](install.md#5-know-the-two-folders)):

```json
{
  "schemaVersion": 1,
  "shops": {
    "defaultBuyPrice": 40,
    "defaultSellPrice": 20,
    "priceRules": {
      "Prefabs/Items/Equipment/Backpacks/": { "buyPrice": 90, "sellPrice": 45 }
    }
  }
}
```

The keys are matched in order, later rules win, and the rule's key is a **prefab-path substring**
(`Prefabs/Items/Equipment/Backpacks/` matches every backpack there), `prefab:<GUID>`, or
`type:<flags>,mode:<mode>` for arsenal types. An item no rule matches uses the defaults. `sellPrice`
is what the counter pays the player; a sell price of zero means the counter refuses to buy it.

Check the file offline before the restart:

```
tools\cli config-check path\to\config.json
```

A new rule key prints a warning that no shipped rule has it. That is expected for your own rule:
it adds one. What matters is `0 error(s)`. A prefab path that matches nothing gives no warning, so
check the path against the item's real resource path in Workbench.

## 4. Or use a catalogue outlet

The core also ships the item catalogue (#988): an outlet is a named list of items with one price
and one gate each. Set `m_sOutletId` (`gun_counter`, `clothing_rack`, `police_armory`) and the shop
sells **exactly** that outlet's offers at the catalogue prices, behind their gates. The `shops`
section is not read for an outlet.

```json
{
  "schemaVersion": 1,
  "catalogue": {
    "items": {
      "pistol_m9": { "price": 650, "sellPrice": 300 }
    },
    "offers": {
      "gun_counter/pistol_m9": { "licence": "FIREARM_PERMIT" }
    }
  }
}
```

`catalogue.items` changes the item's price and how it is sold; `catalogue.offers` is the
`outlet/item` pair and carries the gate: `licence`, `job`, `minRank`, `orgRight`, or `removed: true`.
The shipped outlets and their items are in `Configs/Catalogue/LF_ItemCatalogue.conf`.

An outlet is the right choice when you want one list of items shared by several counters (a chain),
or a job or licence gate per item. A price-rule shop is the right choice for a one-off counter.

## 5. Run it

Start the server on the pack's mission, or press **Play** in Workbench on `Worlds/Town.ent`, the
same as for a job site ([add a delivery job](add-job.md#7-run-it)). Then:

1. Walk to the counter. The prompt is the action's UI info name.
2. Open the price list. The name and description come from the prefab; the rows and prices from
   your config.
3. Buy an item, check the cash, sell it back, check the sell price.
4. Restart the server and buy again to prove the config, not a cache, is what charges.

The log names the shop and the trades:

```
[LFDebug:Shop] bought item=... price=90
[LFDebug:Shop] sold item=... price=45
```

## Problems

| Log line or symptom | What it means |
|---|---|
| The counter has no prompt | no action context, or `LF_ShopAction` is not in the `default` context |
| The price list is empty | the curated list is empty **and** no item is priced, or every item is `hidden` |
| Every row is 40 / 20 | no rule matched; check the rule key against the item's resource path |
| `failed (unpriced) item=...` | the prefab has no usable price; a default price is missing |
| `failed (not sold here)` | the item is not in this shop's curated list, or is not this outlet's offer |
| `failed (insufficient funds)` | the player's cash is below the total; a stack pays per unit |
| `failed (inventory full)` | the player cannot carry the whole order; the rest is refunded |
| The item does not appear in an outlet shop | no `offers` row for that `outlet/item` pair, or `removed: true` |
| A login-only gate | `licence` is an `LF_ELicenseType` name; `job` is a job id (`POLICE`, `DELIVERY`, ...) |
| Changing the config changed nothing | the server reads it at boot; restart, or use `/configreload` on the server |
