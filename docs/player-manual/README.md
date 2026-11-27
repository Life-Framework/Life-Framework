# Life Framework: Everon 1989. Player manual

For the **0.3.0 alpha** on the real Everon island. Written for players, not developers. Keep it on your phone or print it:
[`manual.html`](manual.html) is the same manual in one file, with every map inside.

## The first ten minutes

1. You wake in the **hostel** in Saint-Philippe. A waypoint and a mark on your map send you south to the town hall.
2. Talk to the **town clerk**. You get your ID card, a welcome pack (bread, a sausage, a bottle of water and $100 in cash) and a starter car.
3. Take the car out at the **garage counter** by the car lot, drive to the **orchard**, pick **six apples** and press them into **juice** at the stall.
4. Sell the juice at the **market stall** in town. Each step shows its own waypoint, and the next errands wait under **F2**.
5. Eat and drink from your inventory (**Use**). Press **F** on anything with a prompt: counters, signs, doors, ATMs.

## What is in the manual

| Page | What it covers |
|---|---|
| [1. Getting started](01-getting-started.md) | The hostel, the arrival quest, the map, waypoints, food and drink |
| [2. Money](02-money.md) | Cash against bank, ATMs, what things cost |
| [3. Jobs](03-jobs.md) | One card per job: where, how to start, what you do, pay per hour |
| [4. Homes](04-homes.md) | Buy at the sign, keys, waking up at home. Map of every home |
| [5. Shops](05-shops.md) | What each shop sells, and where it is |
| [6. Police](06-police.md) | Join, duty, car, cuff, search, arrest, jail, fines |
| [7. Crime](07-crime.md) | Robbery, chop shop, moonshine, heists, wanted level |
| [8. Medic and clinic](08-medic.md) | Being downed, the clinic, working as a medic |
| [9. Controls](09-controls.md) | Keyboard and gamepad |
| [Maps](maps.md) | Every map in one place |

## What is live, and what is not (read this first)

This manual describes what is on the island **today**. Some systems exist in the code but have no placed site yet.
Each is marked in the page that covers it.

| Live | Not on the island yet |
|---|---|
| Parcel runs, the postal van run, taxi runs, wheat harvest with the reaper, lumber (by hand or by truck), fishing and the smokehouse, **iron mining and smelting** (Coalman's Brow, Highstone, the east smelter), **coal** (East Rise seam, the power plant buys it), scrap and the recycler, **export orders** at the dock, moonshine, chop shop, till robberies, the bank heist, the north rebel outpost, crews and the registry, police duty, medic duty, homes and plots for sale, cars and the **truck yard**, every shop in Saint-Philippe | **Garbage round** (only in the test grid). **Fuel haul** (no tanker depot on the island). **Copper and stone** (no copper seam, refinery or quarry is placed yet). **Grain mill and bakery flour.** |

The other towns (Meaux, Montignac, Lamentin, Morton, Saint Pierre) are scenery for now. Everything you can do is in
Saint-Philippe or at one of the work sites on the [island map](maps.md); the mines, the smelter, the truck yard and the
north outpost are described in [Jobs](03-jobs.md) and [Crime](07-crime.md) with their distances from town.

## The alpha server in one paragraph

An in-game day is **2 real hours**; nights pass twice as fast. The server restarts at **06:00 and 18:00** and warns you 15,
5 and 1 minutes before. You rejoin where you logged out (up to 12 hours). New characters get **5 minutes of grace**:
robberies and vault breaks refuse them. Homes and plots cost **half** their listed price, cars **80 percent**, the firearm
permit is **instant**, and the police desk approves applications **at once**. The alpha can be wiped; see the
[player FAQ](../player-faq.md).

## Rebuilding this manual

```
node docs/player-manual/build.mjs
```

It re-renders every map from the real island (offline, no game needed), rewrites the legends and shop tables from the
game's data, and rebuilds `manual.html`. The maps live in `website/public/docs-images/player-manual/`.
