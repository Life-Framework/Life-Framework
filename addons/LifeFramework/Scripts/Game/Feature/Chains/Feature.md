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
- **Stations (#417).** `LF_ChainStationComponent` on a prop (`LF_ChainBarrel.et` is the barrel),
  with `LF_ChainStationAction` in three modes: load, work (a held action, one unit of work), collect.
  Progress belongs to the station, so more hands finish it sooner. Stations with the same site id
  share one heat number (`LF_ChainSites`). A loaded station is saved by its station key (payload
  `chainstations`, save version 20).
- **Trace (#418).** `LF_ChainTrace` sweeps on a 5 s timer while a site is warm: smoke level per station, smell cue and `LF_OnChainSmell` per listener, a police lead per site. **Searches (#419)** read the site heat and the chain's goods.
- **Searches (#419).** A chain flagged `m_bContraband` makes its last step's outputs chain goods; `m_iBulk` is how bulky one unit is. An on-duty officer's search (`LF_ConfiscateAction` on a player, `LF_VehicleSearchAction` on a vehicle) rolls on the server: the chance grows with load (units x bulk), so bulky goods of harder tiers are harder to hide. A find seizes the goods and files `#LF-Offence_ChainGoods` through `LF_WantedReports.Report` (bus: `LF_OnCrimeCommitted`). A vehicle charges its occupants; #414 adds the plate's registered owner at `LF_ChainSearch.SuspectsOf`.

## Rules

- The server decides gates, progress, and payouts; the client only asks.
- Equipment is an item the worker carries. Owning it is inventory, so it persists with the
  inventory and needs no save change.
- Chain names, items, and prices are pack config; the core ships one neutral example (`cider`).

## Dependencies

`Processing` (recipe rows), `Resources` (the config file), `License` and `Level` (gates),
`Quantity` (inventory amounts), the event bus.
