# The supermarket

Milestone: M6 (Making a living), chapter "The supermarket"
Extends: the working supermarket (`Supermarket_Store.et`, `LF_StoreComponent`, `LF_ShopComponent`), survival hunger and thirst, the asset pipeline (`tools\cli asset`)

## Player loop

A player walks into the village supermarket, takes groceries off full shelves, pays at a till
and eats or drinks them later. Every board holds something a 1989 local would buy. Walking out
unpaid still rings the alarm.

## In scope

Fifteen new store lines, with the nine already on the shelves making 24:

| Item | Shape | Use |
| --- | --- | --- |
| Tinned beans | tin | food |
| Tinned fish | flat tin | food |
| Jam | jar | food |
| Pickled gherkins | jar | food |
| Flour | paper sack | stock, cooking later |
| Sugar | paper sack | moonshine input |
| Pasta | box | food |
| Biscuits | wrapped packet | snack |
| Tea | small box | drink |
| Milk | carton | drink |
| Lemonade | glass bottle | drink |
| Soap | bar | household |
| Loaf of bread | generated (Meshy) | food |
| Cheese wheel | generated (Meshy) | food |
| Sausage ring | generated (Meshy) | food |

- Labels in one local style, French in flavour, with invented brands. Printed text comes from the
  label maker, never from a generator.
- Each line gets a price, hunger or thirst values by category, and a place on the shelves through
  the supermarket's `store` block.

## Out of scope

- Alcohol and cigarettes.
- Soviet or American import labels.
- Real brands or trademarks.
- Any new store system: the till, stock, restock and alarm are built.

## Assets

The base game has almost no groceries, so the goods are new models.

- The twelve packages come from shared Blender package shapes (box, tin, bottle, jar, carton,
  sack, packet, bar) and the label maker. No credits.
- The three fresh items go through Meshy image-to-3D, about 210 credits.
- Every model gets an `asset-inventory.md` row and its credit record.

## First slice

The asset pipeline changes that make a batch cheap, proven on this list:

1. Asset imports run on a pool machine with the workbench cap, as the building import does, and a
   whole batch compiles in one Workbench run.
2. Package shapes in Blender and a label maker grown from `tools/assets/cement-bag/make-label.ps1`.
3. One catalog file per batch: one command builds every item, writes each prefab (food, drink or
   household base, hunger and thirst, weight and volume from size, inventory camera, strings,
   price) and one review sheet beside the vanilla apple and crackers.

Then the twelve packages in one PR, and the three fresh items in a second.

## Depends on

- The supermarket store (#1143)
- The asset pipeline (#339)
- `tools\cli fixtures` for the new footprints

## Proof

- `tools\cli fixtures --check` passes with the new items.
- `tools\cli building proof supermarket` with the new lines: take one of each kind, pay, eat or
  drink it.
- `look` pictures of every gondola and cooler, full profile, no empty board.
