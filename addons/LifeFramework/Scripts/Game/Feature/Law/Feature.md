# Law — Design Requirements

> The law book: the codes an officer charges, how they combine, and what they
> cost. Context for the booking desk and charge sheet (#4151). Written against
> `docs/design.md` (in-world first: the book is read at a lectern, the charge
> sheet is paperwork).

## Intent

A crime code is one row of the crime catalog (`Feature/Crime/LF_CrimeCatalog.c`,
`LF_CrimeEntry`). The law book adds three facts to the row so a charge sheet can
total and stack by rule instead of by officer judgment.

## Codes (L1, #4202)

- **Section** (`m_eLawSection`, `LF_LawSection`): `CRIMINAL`, `ROAD`, `NARCOTICS`,
  `WEAPONS`. The lectern reads one section at a time. New codes default to `CRIMINAL`.
- **Stacking group** (`m_sStackGroup`): codes of one non-empty group on a sheet
  do not add; the higher counts. Empty means the code stacks with nothing.
- **Jail only** (`m_bJailOnly`): the code refuses a fine. It adds nothing to the
  fine total and `LF_LawCodeRules.CanFine` says no. Its time still counts.

Rules live in `Law/LF_LawCodeRules.c` (pure functions over entries: `Counted`,
`TotalFine`, `TotalSentenceSeconds`). Names and lookups (`SectionKey`,
`ParseSection`, `CollectSection`, `Resolve`) are in `Law/LF_LawCode.c`.

Owner config: the `crime` section's record takes `"lawSection"` (`criminal`, `road`,
`narcotics`, `weapons`), `"stackGroup"` and `"jailOnly"`. A bad section is refused
and logged (`[LFDebug:Law]`); the shipped value stays.

## Not here yet

- The Everon amounts, groups and jail-only flags per code: L2 (pack law book).
  Core rows ship with the defaults until then.
- The charge sheet total, rank cap, repeat doubling and the Sergeant override are
  `Law/LF_ChargeSheetRules.c` (L3, #4209); the booking desk that uses them is L5.

## Charge sheet paper (L4, #4426)

Booking opens a paper, not an offer. The officer ticks each open report, sees the stacked and
over-cap rows greyed and the total at the foot, and signs. Client: `LF_ChargeSheetScreen.c`
(model, screen, harness fixture). Server: `LF_ChargeSheetScreenDesk.c` (open, sign, the two RPCs).
The paper only previews: the server recomputes from the ticked ids with its own account's rank and
duty in `LF_BookingDesk.Offer`; a total or rank the client claims is never read.
