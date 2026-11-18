# Laundering

Dirty money as items (M7, parent #1025). No dirty balance anywhere: money stays physical (#644).

- `LF_MarkedCash.c`: marked notes, the `markedcash` slot item. `Add`/`Count`/`Take` like
  `LF_MoneyUtils`, but never counted as cash and never on the cash ledger. Logs `[LFDebug:Laundering]`.
- `LF_LaunderConfig.c`: `Configs/Crime/LF_Launder.conf`. `m_bCrimePaysMarked` (core off) makes till
  robberies and bank vault bags pay marked notes (#2851); chop shop and fence stay cash.
- `LF_LaunderStationComponent.c` / `LF_LaunderStationAction.c`: the laundering station (#2853). Takes
  marked notes first, pays clean cash minus the cut to the loader only, warms its chain site.

Docs: `docs/features/laundering.md`. Tests: `LF_Test_MarkedCash`, `LF_Test_DirtyPayout`, `LF_Test_LaunderStation`.
