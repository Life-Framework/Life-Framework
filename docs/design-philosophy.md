# Design Philosophy — In-World First

The design opinions every Life Framework feature must respect. This is the
"why" behind the `Feature.md` context files in
`addons/LifeFramework/Scripts/Game/Feature/*/`. If a feature's `Feature.md`
ever disagrees with this file, this file wins.

## 1. In-World First

We play life mods to live in the world, not to live in menus. The default
answer to "how does the player do X?" is *physically, in the world*:

- To shop, you walk into a store and touch the goods.
- To gather, you walk to the tree or node and take from it.
- To bank, you go to the bank and talk to the teller.
- To process, you go to the machine and operate it.
- To deposit, you walk to the counter — it's a period-accurate 1989.

A UI-only loop is a last resort, never a starting point. When a feature ships
as a menu because that is the cheapest V1, the design must name the physical
replacement and the path to reach it. A menu with no named physical target is
an incomplete design.

## 2. The interaction fidelity ladder

Every player-facing action sits on one of these rungs. Features climb the
ladder over time; they may start low, but the *target* is documented.

1. **Physical** — walk up to a world object and act on it. The object is the
   interface: pick an apple from a tree, sit in a car to buy it, operate a
   processing machine, hand money over a counter.
2. **Physical + selection** — a world object carries options you scroll through
   in place. A mannequin wearing a shirt you cycle through colors; a shelf of
   cans where scrolling picks the item and quantity before you buy.
3. **World-anchored menu** — a real place or person opens a menu, but the menu
   exists only because you are there. A bank teller's counter, a police station
   terminal. The world is the gate; the menu is the transaction.
4. **Pure UI** — no world analog: notifications, HUD, admin, account records.
   Acceptable only where the real 1989 world wouldn't have an object either.

Rule: **a feature must be able to state which rung it lives on today and which
rung it targets.** If the gap is documented, someone can close it. Unlabeled
UI is how mods drift back into menu-land.

## 3. Shippable V1, then climb

We ship the thing that exists, not the thing that is perfect. V1 is the
smallest end-to-end loop that works and can be played. Then we climb the ladder
in verifiable steps. Example — the clothing store:

- **V1**: a shop post or sign with scrollable buy/sell options.
- **V2**: a physical store (reusing existing Reforger prefabs where possible)
  with separate areas for shirts and pants; each garment on the rack is a
  purchase point.
- **V3**: mannequins wearing the garments; scroll to change the color; preview
  the color on the mannequin before buying.
- **V4**: NPC clerks, changing rooms, sizing, layered clothing.

Every rung is playable and shippable on its own. No rung depends on a feature
that does not exist yet. Perfection is the enemy of playable.

## 4. Stores are building overrides

Commerce locations are drop-in prefabs built on Reforger's existing buildings.
A server owner replaces the bank on the map with the Life Framework bank prefab
and the location is *populated*: shelves, racks, mannequins, weapons on the
wall — each one a working purchase point. No hand-wiring per map; the prefab
brings its own population and its own price data.

This is what makes "real life" shopping feasible across many towns without
hand-authoring every store.

## 5. Money is physical

Cash is an inventory item — the MoneyStack stack. You carry money, you hand it
over, you are robbed of it. Bank balances are the *only* abstract money, and
they live at the bank — the place you must physically go to move cash between
pocket and account. Any feature that mints or destroys cash outside these
paths is a bug (see the known exploits in `docs/features.md`).

## 6. Where UI is welcome

- Notifications, HUD status, survival readouts — signals, not transactions.
- Communication (chat) — inherently UI, and it's fine.
- Management surfaces with no world object (admin, whitelist) — anchored to a
  terminal or station where one exists.
- Anything a real 1989 person could not do in person.

## 7. Non-negotiables (from AGENTS.md)

- Every rejection is logged through `EL_Debug` (`rejected`/`failed`/`skipped`),
  never a silent no-op.
- The server validates every transaction; client input is never trusted.
- Fail-safe: a misconfigured store or population degrades that one store, never
  the world.
- In-world purchases must be server-validated and logged, exactly like any
  other money path.