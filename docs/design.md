# Design

Product and design intent for Life Framework: why it exists, how it should
feel, and how we decide what belongs in the base mod versus an extension.
Every feature's `Feature.md` in `addons/LifeFramework/Scripts/Game/Feature/*/`
is written against this file; if one disagrees with it, this file wins.

Related: [roadmap/README.md](./roadmap/README.md) (the live plan),
[foundation-design.md](./foundation-design.md) (how primitives are shaped),
[features.md](./features.md) (implemented contracts),
[AGENTS.md](../AGENTS.md) (engineering rules).

## 1. Vision

> Sections 1 and 2 come from https://life-framework.org/philosophy.

We are a group of modders contributing our time because we love life mods and
want to bring back the life server community we enjoyed. This is a passion
project: built for fun, built to share, built to play.

We are building a **base framework with all the elements you need to create
your own Life experience**: the core systems, mechanics, and tools, so a server
owner can focus on what makes their server special. We also ship an
**easy-to-set-up, playable version out of the box**, set in **1989** to make
the best use of Arma Reforger's native assets.

We ship fast and welcome new contributors. We share what we learn
(documentation, solved problems, better approaches) so the whole community
benefits.

### Core values

- **Open source first** - built by the community, for the community.
- **Passion driven** - we do this because it is fun; we want to play life mods
  again.
- **Accessible to all** - a large server, with friends, or locally. Your
  server, your rules.
- **Move fast** - ship, iterate, get features into players' hands.

## 2. Free, open, extensible

**MIT licensed.** Free for any purpose, hobby or commercial, no fees or
restrictions. Use, modify, and build on it.

Life Framework is a **modular base with strong defaults** and a *common
language* for extensions: enough to host a credible civilian life server on
vanilla content, with clear extension points for a different era, tone, or
feature set (including the patterns common on Modern Life servers). Clear
standards and interfaces let:

- third-party developers build compatible extensions without modifying the
  core;
- server owners mix and match features;
- community creations work together.

### Framework first, experience second

We ship **primitives and contracts**, not one authored server fantasy.

- Core systems expose config, public APIs, and persistence hooks so mods can
  extend without forking.
- The base mod is one **reference configuration** of those primitives (1989
  Everon, civilian economy, law enforcement hooks). It proves the framework
  works; it is not the only valid configuration.
- Prefer small, testable modules over monolithic managers.
- When two implementations solve the same problem (duplicate bank paths,
  parallel UI stacks), consolidate to **one authoritative seam** before adding
  surface area.

**Decision test:** can a server owner turn this off, replace it, or
reconfigure it in Workbench without editing EnforceScript? If not, redesign it
or demote it to base-mod-only content.

## 3. Period, place, and assets

The default experience is **1989 on Everon**, matching Reforger's native
setting, vehicles, weapons, and environments.

- Use **vanilla vehicles, weapons, structures, and UI patterns** wherever
  possible. New art is a last resort for the base mod.
- Interactions read as plausible for the period: paperwork, workplaces,
  radios, notice boards, bank counters, not contemporary consumer technology.
- The core is **era-agnostic**: economy, jobs, ownership, and crime logic do
  not hard-code 1989. Period flavor lives in configs, prefabs, and
  presentation components. Asset packs or extension mods can move a server to
  modern, WW2, or any era by swapping presentation and content, not core logic.

**Decision test:** does this need custom props or UI chrome vanilla Reforger
does not support? If yes, it belongs in an extension unless there is a strong
vanilla substitute.

## 4. In-world first: the world is the interface

We play life mods to live in the world, not in menus. The default answer to
"how does the player do X?" is *physically, in the world*: walk into a store
and touch the goods, walk to the tree and take from it, go to the bank and
talk to the teller, operate the machine. Information and services are reached
**where people would go in the setting**, never through a universal pocket
computer or bespoke full-screen "life hub".

A UI-only loop is a last resort. When a feature ships as a menu because that is
the cheapest V1, the design names the physical replacement and the path to it.
A menu with no named physical target is an incomplete design.

### The interaction fidelity ladder

1. **Physical** - walk up to a world object and act on it. The object is the
   interface: pick an apple, sit in a car to buy it, operate a machine, hand
   money over a counter.
2. **Physical + selection** - a world object carries options you scroll in
   place: a mannequin whose shirt color you cycle; a shelf where scrolling
   picks item and quantity.
3. **World-anchored menu** - a real place or person opens a menu that exists
   only because you are there (bank counter, police station terminal). The
   world is the gate; the menu is the transaction.
4. **Pure UI** - no world analog: notifications, HUD, admin, account records.
   Acceptable only where a real 1989 world would have no object either.

Every feature states **which rung it is on today and which rung it targets**.
A documented gap can be closed; unlabeled UI is how mods drift back into
menu-land.

### Default information surfaces

| Need | Base-mod approach |
| --- | --- |
| Market / commodity prices | Bulletin boards at markets and processing sites; trader dialogue; optional newspaper or radio bulletins |
| Employment | Apply and work at a physical workplace (quarry, mill, depot, station) |
| Crafting knowledge | Blueprint items, workshop notice boards, station inspect UI |
| Banking | ATM or bank counter |
| Vehicle status | Registration office, garage clerk, paper vehicle document tied to plate |
| Housing | Property ledger at the building, landlord NPC, rent payment at bank |
| Emergency | Payphone, radio call, or flagged interaction; police receive via radio net and station records |
| Licensing | License office or faction authority; persisted license record on the character |

Separate **data services** (prices, recipes, vehicle registry, warrants) from
**presentation components** (bulletin board, clipboard item, registry clerk
action). Several surfaces can read one service; that is how an extension adds
a modern UI without forking economy code.

### Where UI is welcome

- Notifications, HUD status, survival readouts: signals, not transactions.
- Communication (chat).
- Management surfaces with no world object (admin, whitelist), anchored to a
  terminal or station where one exists.
- Anything a real 1989 person could not do in person.

### Shippable V1, then climb

V1 is the smallest end-to-end loop that works and can be played. Then climb
the ladder in verifiable steps. Each rung is playable on its own and depends
on nothing that does not exist yet. Example, the clothing store:

- **V1**: a shop post or sign with scrollable buy/sell options.
- **V2**: a physical store (existing Reforger prefabs) with areas for shirts
  and pants; each garment on the rack is a purchase point.
- **V3**: mannequins wearing the garments; scroll to change and preview color.
- **V4**: NPC clerks, changing rooms, sizing, layered clothing.

### Stores are building overrides

Commerce locations are drop-in prefabs built on Reforger's buildings. A server
owner replaces the bank on the map with the Life Framework bank prefab and the
location is *populated*: shelves, racks, mannequins, weapons on the wall, each
a working purchase point. The prefab brings its own population and price data;
no hand-wiring per map. That is what makes in-world shopping feasible across
many towns.

## 5. Economy: physical money, authoritative math

The civilian loop is **gather -> process -> sell**, with buy paths for tools,
vehicles, and property.

- **Money is physical.** Cash is an inventory item (the MoneyStack). You carry
  it, hand it over, and can be robbed of it or lose it on death. The bank
  balance is the *only* abstract money and lives at the bank, the place you go
  to move cash between pocket and account. Any feature that mints or destroys
  cash outside these paths is a bug.
- **Config-driven catalogues** for goods, prices, recipes, and shop stock.
- **One money mutation seam** for deposit, withdraw, transfer, purchase, fine,
  and paycheck. The server recomputes eligibility and price through the same
  pure functions the client displays.
- **All-or-nothing transactions**: funds, inputs, and stock checked before any
  deduction; a partial failure refunds exactly what moved.
- Dynamic pricing, cross-server markets, and auction houses are optional
  modules; the base mod can ship fixed or locally tuned prices.
- Legal work can raise **standing** (reputation) affecting shop terms and sell
  values; violence and crime lower it. Standing is a hook; formulas live in
  config.

## 6. Real professions and credible progression

Work should read as **employment**, not a character class. Keep three layers
distinct:

1. **Employment** - the job you hold (miner, postal worker, mill hand, police
   officer): config-defined title, workplace, wage, duties.
2. **Competence** - skill from repetition: faster gathering, better equipment,
   higher yields. Levels through activity, not XP menus.
3. **Authority** - licenses and faction roles (police, medic, commercial
   driver, weapons permit), gated by training, whitelist, and record, not a
   cash purchase from a hidden menu.

Base-mod examples: agricultural worker, miner, logger, fisher,
processing-plant worker, refinery worker, mechanic, carpenter, postal courier,
general labor / unemployed. Licensed public roles: police, medic,
press/civil information (where enabled). Underground activity (contraband,
theft, smuggling) flows through the crime and search systems, not an
"illegal profession" slot.

- **Hire**: workplace interaction plus requirements (competence floor, clean
  record, license).
- **Improve**: do the job; bonuses arrive passively.
- **Change jobs**: resignation cost and cooldown are fine, framed as
  employment, not respec friction.
- **Pay**: wages from employer or public payroll on a tick, not quest-reward
  pop-ups unless configured.

**Decision test:** would this job title appear on a believable 1989
employment form or union card?

## 7. Law enforcement and crime

Police play is **faction + tools + records**, integrated with civilian systems.

- Reports through period channels; officers work from dispatch lists, warrant
  records, and vehicle registration at the station or over radio.
- Fines, arrest, search, impound, and detention use the same account, vehicle,
  and property persistence as civilian features.
- Crime severity, wanted level, and penalties are data-driven.
- Equipment and rank progression are config and whitelist concerns, not
  hard-coded loadouts.
- Extensions (modern dispatch apps, ANPR cameras) attach to the same record
  and vehicle services.

## 8. Ownership: vehicles, housing, organizations

Persistence keys are stable ids, never a session `EntityID`.

- **Vehicles**: registration, keys (multiple holders), impound, inspection,
  and garage storage are framework concerns. Tuning, cosmetics, and specialty
  vehicles are content modules.
- **Housing**: lease or purchase with persistent storage; rent on a server
  tick; home spawn feeds spawn logic. Remote rent through banking is fine;
  remote management through a fictional smartphone is not in the base mod.
- **Organizations**: businesses, unions, clubs, factions get registry, ranks,
  shared funds, and optional territory hooks, not a baked-in gang-war meta.
  Server owners configure what controlling a location means.

## 9. Proof standards

Features declare their **proof class** (see the AGENTS.md verification ladder):

| Proof | Meaning |
| --- | --- |
| Logic tests | Pure rules, `--tier fast` |
| World tests | Prefab spawn, resource load, in-session behavior |
| Persistence | Save -> **restart** -> load through public API only |
| Manual | UI feel, JIP, late joiner, dedicated server + client, multiplayer timing |

In-session round trips do not prove restart persistence. A green host-only run
does not prove dedicated-server behavior.

## 10. Localization and content

- Player-facing strings are keys in `Language/everonlife_localization.st`;
  fill `Comment` for translators; layouts reference `#Key`.
- Base-mod copy uses a neutral, period-appropriate tone. Server owners retune
  strings in config or override the language files.

## 11. Base mod vs extension

| In base mod (defaults) | Extension / server content |
| --- | --- |
| Money, shop, sell, recipe, property primitives | Era-specific UI (modern phone, tablet) |
| Job catalog schema + example jobs | Full gang territory metagame |
| Bulletin board + trader price service | Global cross-server auction house |
| License and whitelist framework | Custom vehicle tuning shops |
| Police records, fine, arrest hooks | Author-specific events and minigames |
| 1989 Everon example configs | Kolguyev / modern / alt-history configs |

When in doubt, **ship the hook and one vanilla example**, not the whole
fantasy.

## 12. Feature checklist (PRs and roadmap items)

1. **Modular** - config-driven, extension-friendly API, no undocumented
   singletons.
2. **Period-safe default** - base prefabs and strings fit 1989 Everon without
   extra asset mods.
3. **World-facing** - the primary path uses actions, stations, or existing
   menus; rung on the fidelity ladder stated, today and target.
4. **Authoritative server** - client offers match server validation; a hostile
   RPC cannot grant items, money, or outputs. In-world purchases are
   server-validated and logged like any other money path.
5. **Logged** - every rejection goes through `EL_Debug`
   (`rejected` / `failed` / `skipped`), never a silent no-op.
6. **Fail-safe** - a misconfigured store or population degrades that one
   feature, never the world.
7. **Persistent** - restart path tested if the feature saves state.
8. **Localized** - keys, not literals.
9. **Tested** - LOGIC tests for rules; WORLD or manual steps as scope requires
   ([roadmap/testing/README.md](./roadmap/testing/README.md)).
10. **Documented** - contract updated in [features.md](./features.md) when the
    implementation lands.
