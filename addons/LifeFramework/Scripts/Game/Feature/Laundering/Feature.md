# Laundering

Dirty money as items (M7, parent #1025). No dirty balance anywhere: money stays physical (#644).

- `LF_MarkedCash.c`: marked notes, the `markedcash` slot item. `Add`/`Count`/`Take` like
  `LF_MoneyUtils`, but never counted as cash and never on the cash ledger. Logs `[LFDebug:Laundering]`.
- `LF_LaunderConfig.c`: `Configs/Crime/LF_Launder.conf`. `m_bCrimePaysMarked` (core off) makes till
  robberies and bank vault bags pay marked notes (#2851); chop shop and fence stay cash.
- `LF_LaunderStationComponent.c` / `LF_LaunderStationAction.c`: the laundering station (#2853). Takes
  marked notes first, pays clean cash minus the cut to the loader only, warms its chain site.

Docs: `docs/features/laundering.md`. Tests: `LF_Test_MarkedCash`, `LF_Test_DirtyPayout`, `LF_Test_LaunderStation`.
- `LF_BuyerRotation.c`, `LF_BuyerSpotComponent.c`: which spot of the illicit buyer is open (#2856). Spots
  are `LF_Places` kind `chain_buyer`; the open one is a pure function of unix time and a secret seed kept
  in `$profile:LifeFramework/buyer-rotation-seed.txt` (never in the repo), rotating every
  `m_iBuyerPeriodHours`. Nothing is saved. `LF_Places.FindIds` (modded here) lists a kind's ids.
- `LF_BuyerNpc.c`: the buyer character (`Prefabs/Characters/LF_IllicitBuyer.et`) stands at the open spot while
  a player is within 150 m, moves when the rotation does, loiters (#2919). `Start()` is called from the game mode.
- `LF_InformantHook.c`: the informant's tip-off (#2921). Dialogue hook `Informant` takes `m_iInformantFee` cash from a
  player at underworld tier `m_iInformantMinTier` (not police on duty) and puts a private `LF_MapShares` mark on the open
  buyer spot until the period ends. Server re-checks all; every refusal is in words with nothing taken.
