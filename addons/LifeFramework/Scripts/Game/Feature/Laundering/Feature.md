# Laundering

Dirty money as items (M7, parent #1025). No dirty balance anywhere: money stays physical (#644).

- `LF_MarkedCash.c`: marked notes, the `markedcash` slot item. `Add`/`Count`/`Take` like
  `LF_MoneyUtils`, but never counted as cash and never on the cash ledger. Logs `[LFDebug:Laundering]`.

Docs: `docs/features/laundering.md`. Test: `LF_Test_MarkedCash`.
