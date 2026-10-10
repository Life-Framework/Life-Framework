# Police missions

Phase: After release
Extends: [ranks](../police/ranks.md), [tasks](tasks.md), `Police`

## Player loop

An on-duty officer takes a call from the station board: a placed scene with a suspect, a fine, or an escort back to jail. Completing it pays the officer in cash and can count toward rank time.

## In scope

- Calls as tasks restricted to on-duty police.
- One example call: go to a marker, arrest or fine an eligible target, return.
- Higher ranks see calls the lower rank does not, using the rank config.

## Out of scope

- Random crime simulation across the whole map. That is [events](events.md).
- New police vehicles or weapons.

## Assets

Station is `Villa_E_2I01_Police.et`. Call list and reasons: `LNGC_DispatchCallsDialog`, `LNGC_IssueFineAction`, `LNGC_FineReasonsCatalog`. Fine payment stays on `EL_PoliceUtils` and MoneyStack, not LNGC virtual cash. A civilian opens the call at the station board. Do not require `LNGC_Phone*`. See [sources](../../sources.md).

## First slice

One board, one call that completes when the officer fines a wanted player at the marker.

## Depends on

- [Police ranks](../police/ranks.md)
- [Jailing](../police/jailing.md)
- [Tasks](tasks.md)

## Proof

- Off-duty accept is refused and logged.
- Completion pays cash once.
