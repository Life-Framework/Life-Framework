# Design Principles

> How Life Framework is meant to feel and how we decide what belongs in the
> base mod versus an extension. Companion to [vision.md](./vision.md) (why we
> exist) and [foundation-design.md](./foundation-design.md) (how primitives are
> shaped). Engineering rules live in [AGENTS.md](../AGENTS.md).

Life Framework is a **modular base with strong defaults** — enough to host a
credible civilian life server on vanilla Arma Reforger content, with clear
extension points for server owners who want a different era, tone, or feature
set (including the patterns common on **Modern Life Servers**).

These principles apply to every feature proposal, prefab, script class, and
roadmap item.

---

## 1. Framework first, experience second

We ship **primitives and contracts**, not a single authored server fantasy.

- Core systems expose config, public APIs, and persistence hooks so third-party
  mods can extend without forking the repo.
- The base mod is one **reference configuration** of those primitives (1989
  Everon, civilian economy, law enforcement hooks). It proves the framework
  works; it is not the only valid configuration.
- Prefer small, testable modules (see [foundation-design.md](./foundation-design.md))
  over monolithic managers.
- When two implementations solve the same problem (duplicate bank paths, parallel
  UI stacks), consolidate to **one authoritative seam** before adding surface
  area.

**Decision test:** Can a server owner turn this off, replace it, or reconfigure
it in Workbench without editing EnforceScript? If not, redesign or demote it to
base-mod-only content.

---

## 2. Period, place, and assets

The default experience is **1989 on Everon**, aligned with Arma Reforger’s
native setting and asset library.

- Use **vanilla vehicles, weapons, structures, and UI patterns** wherever
  possible. New art is a last resort for the base mod.
- Interactions should read as plausible for the period: paperwork, workplaces,
  radios, notice boards, bank counters — not contemporary consumer technology.
- The framework core stays **era-agnostic**: economy, jobs, ownership, and
  crime logic do not hard-code “1989”. Period flavor lives in configs, prefabs,
  and presentation components.
- Server owners may add asset packs or extension mods to run a **Modern Life
  Server** or another era. Those extensions swap presentation and content; they
  should not require rewriting core logic.

**Decision test:** Does this feature require custom props or UI chrome that
vanilla Reforger does not already support? If yes, it belongs in an extension
mod unless there is a strong vanilla substitute.

---

## 3. The world is the interface

Information and services are accessed **where people would go in the setting**,
not through a universal pocket computer.

### Default information surfaces

| Need | Base-mod approach |
| --- | --- |
| Market / commodity prices | Bulletin boards at markets and processing sites; trader dialogue; optional newspaper or radio bulletins |
| Employment | Apply and work at a **physical workplace** (quarry, mill, depot, station) |
| Crafting knowledge | Blueprint items, workshop notice boards, station inspect UI |
| Banking | ATM or bank counter (existing menu patterns) |
| Vehicle status | Registration office, garage clerk, paper vehicle document tied to plate |
| Housing | Property ledger at the building, landlord NPC, rent payment at bank |
| Emergency | Payphone, radio call, or flagged interaction; police receive via radio net and station records |
| Licensing | License office or faction authority; persisted license record on character |

### Presentation vs data

Separate **data services** (prices, recipes, vehicle registry, warrants) from
**presentation components** (bulletin board widget, clipboard item, registry
clerk action). Multiple surfaces can read the same service; that is how an
extension mod adds a modern UI without forking economy code.

**Decision test:** Can the player learn or do this without opening a bespoke
full-screen “life hub”? Prefer world actions and existing Reforger menus.

---

## 4. Real professions and credible progression

Work should sound like **employment**, not a character class.

### Three layers (keep them distinct)

1. **Employment** — the job you hold (miner, postal worker, mill hand, police
   officer). Config-defined title, workplace, wage, and duties.
2. **Competence** — skill from repetition: faster gathering, access to better
   equipment, higher yields. Levels through activity, not XP menus.
3. **Authority** — licenses and faction roles (police, medic, commercial driver,
   weapons permit). Gated by training, whitelist, and record — not a cash
   purchase from a hidden menu.

### Base-mod employment examples

Agricultural worker, miner, logger, fisher, processing-plant worker, refinery
worker, mechanic, carpenter, postal courier, general labor / unemployed.

Licensed public roles: police, medic, press/civil information (where enabled).

Underground activity (contraband, theft, smuggling) flows through the **crime
and search systems**, not a selectable “illegal profession” slot.

### Progression feel

- **Hire:** workplace interaction + requirements (competence floor, clean
  record, license).
- **Improve:** do the job; bonuses arrive passively.
- **Change jobs:** resignation cost and cooldown are fine; frame as employment
  fiction, not respec friction.
- **Pay:** wages from employer or public payroll on a tick — not abstract
  “quest reward” pop-ups unless configured that way.

**Decision test:** Would this job title appear on a believable 1989 employment
form or union card? Avoid joke names and gamey class labels in the base config.

---

## 5. Economy: physical loops, authoritative math

The civilian loop is **gather → process → sell**, with optional buy paths for
tools, vehicles, and property.

- **Config-driven catalogues** for goods, prices, recipes, and shop stock
  ([foundation-design.md](./foundation-design.md)).
- **One money mutation seam** for deposit, withdraw, transfer, purchase, fine,
  and paycheck. Every caller uses the same rules; the server recomputes
  eligibility and price through the same pure functions the client displays.
- **All-or-nothing transactions** — full funds, full inputs, and full stock
  checked before any deduction; partial failures refund exactly what moved.
- **Cash as inventory** (robbery and loss-on-death surface) plus **bank balance**
  (persistence and large purchases) — one logical account, two representations.
- Dynamic pricing, cross-server markets, and player auction houses are
  **optional modules**. The base mod can ship with fixed or locally tuned prices
  until those modules exist.

Legal work can improve **standing** (reputation / community record) affecting
shop terms and sell values. Violence and crime degrade it. Standing is a
framework hook; exact formulas live in config.

---

## 6. Law enforcement and crime

Police play is **faction + tools + records**, integrated with civilian systems.

- Reports through period-appropriate channels; officers work from **dispatch
  lists, warrant records, and vehicle registration** at the station or over radio.
- Fines, arrest, search, impound, and detention connect to the same account,
  vehicle, and property persistence as civilian features.
- Crime severity, wanted level, and penalties are data-driven.
- Equipment and rank progression are config and whitelist problems, not
  hard-coded loadouts in core scripts.

Extensions (modern dispatch apps, ANPR cameras, etc.) attach to the same record
and vehicle services.

---

## 7. Ownership: vehicles, housing, organizations

Persistence keys are **stable ids**, never session `EntityID` (see AGENTS.md).

### Vehicles

Registration, keys (multiple holders), impound, inspection, and garage storage
are framework concerns. Tuning, cosmetic packs, and specialty vehicles are
content modules.

### Housing

Lease or purchase with persistent storage; rent collection on a server tick;
home spawn feeds spawn logic. Remote rent payment through **banking** is
appropriate; remote management through a fictional smartphone is not in the
base mod.

### Organizations

Groups (businesses, unions, clubs, factions) get **registry, ranks, shared funds,
and optional territory hooks** — not a baked-in gang-war meta. Server owners
configure what control of a location means.

---

## 8. Multiplayer and proof standards

Features must declare their **proof class** (AGENTS.md verification ladder):

| Proof | Meaning |
| --- | --- |
| Logic tests | Pure rules, fast `--tier fast` |
| World tests | Prefab spawn, resource load, in-session behavior |
| Persistence | Save → **restart** → load through public API only |
| Manual | UI feel, JIP, late joiner, dedicated-server + client, multiplayer timing |

In-session round trips do not prove restart persistence. A green host-only test
does not prove dedicated-server behavior.

---

## 9. Localization and content

- Player-facing strings are **localization keys** in
  `Language/everonlife_localization.st`, not literals in scripts or layouts.
- Content is authored for translation: fill `Comment` for translators; layouts
  reference `#Key`.
- Base-mod copy uses neutral, period-appropriate tone. Server owners retune
  strings in config or override language files.

---

## 10. What belongs in the base mod vs an extension

| In base mod (defaults) | Extension / server content |
| --- | --- |
| Money, shop, sell, recipe, property primitives | Era-specific UI (modern phone, tablet) |
| Job catalog schema + example jobs | Full gang territory metagame |
| Bulletin board + trader price service | Global cross-server auction house |
| License and whitelist framework | Custom vehicle tuning shops |
| Police records, fine, arrest hooks | Author-specific events and minigames |
| 1989 Everon example configs | Kolguyev / modern / alt-history configs |

When in doubt, **ship the hook and one vanilla example**, not the entire fantasy.

---

## 11. Feature checklist (for PRs and roadmap items)

Before marking work complete, confirm:

1. **Modular** — config-driven; extension-friendly API; no hidden singletons
   unless documented.
2. **Period-safe default** — base prefabs and strings fit 1989 Everon without
   extra asset mods.
3. **World-facing** — primary player path uses actions, stations, or existing
   menus; no mandatory custom hub UI.
4. **Authoritative server** — client offers match server validation; hostile RPC
   cannot grant items, money, or outputs.
5. **Persistent** — restart path tested if the feature saves state.
6. **Localized** — keys, not literals.
7. **Tested** — LOGIC tests for rules; WORLD or manual steps as scope requires
   ([test-plan.md](./test-plan.md)).
8. **Documented** — behavior contract updated in [features.md](./features.md)
   when implementation lands.

---

## Related docs

- [Vision](./vision.md) — project philosophy and MIT commitment
- [Roadmap](./roadmap.md) — phased delivery plan
- [Foundation design](./foundation-design.md) — economy, trade, processing, real estate primitives
- [Features](./features.md) — current implementation contracts
- [AGENTS.md](../AGENTS.md) — EnforceScript rules and verification ladder
