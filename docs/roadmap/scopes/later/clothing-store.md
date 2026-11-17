# The clothing store

Milestone: M6 (Making a living), chapter "The clothing store"
Extends: the store step of the building pipeline (`tools/buildings/pipeline/store-slots.mjs`), the
self-serve store (`LF_StoreComponent`, `LF_ShopComponent`, till and shoplifting alarm), the vanilla
shop house (`ShopHouse_E_2I01t_Base.et`), the mannequin (`LF_Mannequin.et`)

## Player loop

A player walking down the main street sees two dressed mannequins in a lit shop window under the
vanilla clothes-store sign and goes in. Inside, folded shirts and trousers lie in stacks on the wall
shelves and a table, coats hang on racks and hats and gloves sit on their own shelf. The player takes
a folded shirt off the top of a stack, pays at the till and puts it on. Walking out unpaid rings the
alarm, as in the supermarket.

## In scope

Base-game clothes only, no `_Dirty` variants, no military uniforms (KLMK and KZS stay in the gun store):

| Line | Prefabs | Draft price |
| --- | --- | --- |
| Cotton shirt | `Shirt_CottonShirt_01_V1`..`V6` | 25 |
| Turtleneck | `Shirt_Turtleneck_01_beige`, `_DarkBlue`, `_grey` | 30 |
| Denim jacket | `Jacket_Denim_01_brownShirt`, `_strippedShirt` | 70 |
| Suit jacket | `Jacket_Suit_01_black_V1`, `_Brown_V1`, `_Grey_V1` | 90 |
| Raincoat | `Jacket_Raincoat_01_green`, `_orange`, `_yellow` | 60 |
| Trousers | `Pants_Trousers_01_DarkBrown`, `_DarkGrey`, `_LightBrown` | 40 |
| Suit trousers | `Pants_Suit_01_black_V1`, `_brown_V1`, `_grey_V1` | 50 |
| Fisherman trousers | `Pants_Fisherman_01_green`, `_orange`, `_yellow` | 45 |
| Hats | flat cap, knitted hat, trucker cap, panama, ushanka | 15-35 |
| Gloves | `Gloves_Leather_01`, `Gloves_Wool_01` | 20 |
| Boots | `CombatBoots_Soviet_01`, `CombatBoots_US_01` (the only footwear) | 60 |

- Our own copy of the vanilla shop house: the vanilla clothes-store variant's windows, doors and
  sign on `_Base`, a duplicate of its furniture set with the shop-room pieces taken out.
- From the street: two mannequins in the 4.2 m window under base-game spot lamps, the vanilla
  `SignCommercial_01_ClothesStore`, a sale board or coat stand on the pavement.
- Inside: window mannequins and one more inside, folded-stack wall shelves, a table of folded shirts,
  coat racks (`CoatRack_01`..`05`), a hats and gloves shelf, the till (`CashierShop_01` on
  `CounterShop_01`) with a clerk.
- Two store-step additions: a **stack slot** (a pile of 3 to 5 folded item models with a slight
  random twist, taken from the top, paid at the till) and a **mannequin fitting** (an outfit list
  dressed on `LF_Mannequin`, display only).
- No licence.

## Out of scope

- New clothing models or retextures.
- A changing room, try-on, sizes or a returns desk.
- Selling clothes back to the store.
- A different facade colour (see the spike below).
- A map icon (the base game has none for shops).

## Spike findings (2026-11-15)

- **Mannequin:** a static entity works. The base-game male body and head meshes with every slot in
  one off-white material and each worn clothing `.xob` as a static `MeshObject` at the same origin render in the
  skeleton's bind pose (arms out, A-pose) and the clothes sit on the body without poking through.
  Cost per mannequin: two to five static mesh entities, no animation, physics, inventory or
  replication; a frozen AI character would carry all of those, so it was not needed.
- **Facade colour:** a `MeshObject` material override on our prefab reaches the building, but the
  wall the street sees at distance is the single `Ext_MLOD` atlas (walls and roof in one texture),
  so a tint there would colour the roof too. Tinted copies of the vanilla multi-layer `Ext_Front`
  materials did not load (the packed `.emat` is not plain text). A real recolour needs a recoloured
  MLOD texture; not worth it for this store.
- **Map icon:** `EMapDescriptorType.MDT_STORE` exists but `Configs/Map/MapDescriptorDefaults.conf`
  defines no icon for it (only pub, fuel station, hospital and similar), and LF has no map marker
  code. A shop icon is a separate feature.

## Depends on

- The gun store's store-step changes (vanilla-base building spec, placed fixtures, display lines)
- The supermarket store (#1143)

## Proof

- `tools\cli building proof clothing-store`: take a folded shirt, pay at the till, wear it; the
  alarm on a walk-out.
- `look` pictures: the street by day and night, the window display, the stack shelves.
