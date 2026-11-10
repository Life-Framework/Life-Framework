# Chains — Design Requirements

> Illicit production chains. Scope card: `docs/roadmap/scopes/crime/drug-chains.md`.
> Written against `docs/design.md` (in-world first).

## Intent

A chain is a sequence of hands-on steps at real stations in the world, from raw goods to a
product. Solo is possible and slow; a team is fast, because the work is physical and spread over
several stations. No menus and no progress bars.

## Shape

- **Config (#416).** `m_aChains` in `Configs/Resources/ResourceChains.conf`. A chain lists steps,
  the station type each needs, inputs and outputs, a licence and level gate, an equipment id,
  and heat settings. A pack adds a chain by overriding the config. A malformed chain disables
  itself (`LF_ChainCatalog`) and nothing else.
- **Stations (#417).** Physical station components and a site that holds several of them and one
  heat number.
- **Trace (#418) and searches (#419)** read the site heat and the chain's goods.

## Rules

- The server decides gates, progress, and payouts; the client only asks.
- Equipment is an item the worker carries. Owning it is inventory, so it persists with the
  inventory and needs no save change.
- Chain names, items, and prices are pack config; the core ships one neutral example (`cider`).

## Dependencies

`Processing` (recipe rows), `Resources` (the config file), `License` and `Level` (gates),
`Quantity` (inventory amounts), the event bus.
