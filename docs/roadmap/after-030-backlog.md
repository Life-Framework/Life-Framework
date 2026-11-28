# After 0.3.0: backlog from the week's playtest and review findings

Written 2026-11-28 while 0.3.0 is feature-frozen (fixes only). Nothing here is scheduled and no milestone moves. It
collects every "missing feature", "not built", "noticed", "not reached" and "known issue" line from these sources,
drops what has since merged, and ranks the rest by what a player hits:

- Playtests: [`agent-playtest-2026-11-27.md`](../playtest/agent-playtest-2026-11-27.md) (rounds 1 to 5),
  [`agent-playtest-crime-2026-11-27.md`](../playtest/agent-playtest-crime-2026-11-27.md),
  [`agent-playtest-medic-2026-11-27.md`](../playtest/agent-playtest-medic-2026-11-27.md),
  [`release-smoke-2026-11-28.md`](../playtest/release-smoke-2026-11-28.md).
- Dry runs: [`rc-dryrun-4`](../playtest/rc-dryrun-4-2026-11-27.md), [`5`](../playtest/rc-dryrun-5-2026-11-27.md),
  [`6`](../playtest/rc-dryrun-6-2026-11-27.md).
- Reviews: [`pre-rc-review-2026-11-27.md`](../testing/pre-rc-review-2026-11-27.md),
  [`jip-sweep-2026-11-27.md`](../testing/jip-sweep-2026-11-27.md),
  [`restart-proof-2026-11-27.md`](../testing/restart-proof-2026-11-27.md).
- Release notes: [`0.3.0-notes.md`](../releases/0.3.0-notes.md) "Known issues", and the issues those pages cite.

Size: S is a text, prefab or config change a builder finishes in one slice; M is one feature area with a test and a
session proof; L needs a design pass and several slices. Issues #5196 to #5202 were filed with this page (label
`playtest`, no milestone).

## Ranked backlog

| # | What | Where seen | Issue | Size | Player value |
|---|---|---|---|---|---|
| 1 | Two friends who found a crew have nothing to rob together on Everon: the crew till stands only in the debug grid and the island bank needs two cops on duty | Playtest round 5 | #3407, #781, #780 | M | A crew's first night out is a heist they can start without waiting for the police |
| 2 | A leased hideout has no stash and no perk: the farm compound locks to members, but no island layer places `LF_HideoutStash.et` and perks are not built | Round 5, JIP sweep | #4146 | L | The reason to hold a compound: a members' stash and a perk that pays |
| 3 | Medic job entry: the first ambulance draw can be refused "a vehicle stands in the bay" with nothing there | 0.3.0 known issues | #5196 (police bay twin: #4623) | S | The medic loop starts with this press; a refusal reads as a broken job |
| 4 | Nobody has carried a patient by ambulance in a session: the dock bar gate stopped the ambulance 46 m from a dock casualty, so load, ride, siren and stretcher seat are untested | Medic playtest | #5197 | M | The medic's main trip, and a gate that may keep an ambulance from every dock casualty |
| 5 | A bought Everon house is never open to guests without a key, and a key changes hands only by dropping it (a key cannot be held, so "Give item" skips it) | Round 5, restart proof | #3208, #2435 | M | Houses become homes for a crew or a family, not a locked box |
| 6 | After a hospital respawn the bill says "pay at the hospital desk" but the clinic till offers only "Rob the till"; the death screen says carried things stay but the cash stays in the pocket; a revived patient who dies keeps both bills | Medic playtest | #5198 (related: #3152) | S | A new player's first death reads true and shows where to pay |
| 7 | No tool seller at Highstone mine: the pickaxe is sold in town, so the first trip to the mine can end in a walk back; the Miner's supply sheet gives no answer to "Sell these" with no row chosen | Round 3 | #5199 | S | Hour 3 to 6 mining starts without a detour |
| 8 | Merged fixes no round replayed: cars twice after a restart, the vault drill, duty NPC buttons, farming and fishing, the premium-buyer overflow, bench repair, gang marks and bars, gang radio voice, second-player jail chores and hospital bed, furniture storage in use, marked-note laundering, saw board drops, rebel camp at night, sounds by ear, a human ten-hour run | Release notes, "Not reached" lists, pre-RC review | #5200 (also #4978, #4216, #4271, #4150) | M | Finds out which "merged" fixes are real before the next release |
| 9 | The Courier is on sale but a fresh world has no weekly edition to read | 0.3.0 known issues | #4235 | M | A newsstand item that does something |
| 10 | Supermarket shelves can look empty in a new world until the store restocks; stocked shelf items are not written to the world save | 0.3.0 known issues | #3127, #4819, #4901 | M | A shop that looks open on the first visit |
| 11 | The north rebel camp's lamps and fire were added after the last night test | 0.3.0 known issues | #4978 | S | The camp reads at 22:00 and the quartermaster can be found |
| 12 | Small HUD and prompt gaps: the arrival waypoint draws over the death panel and through a cop's arrest, the sell row is greyed with no reason when the pockets are empty, the map's "Zoom out" label overlaps the scale bar, the locker toast names no price | Medic, crime, rounds 2 to 4 | #5201 | S | Removes four small moments of confusion |
| 13 | Harness gaps: no `hurt` verb (so the medic is the downer), no walk verb, no cash grant, `parkat` needs a placed entity, the hold cap passes mid-run, `t3_background_wait` returns at once | Medic playtest, rounds 2 to 5, dry runs 4 to 6 | #5202 | M | Proofs that test what a player does, not what a teleport allows |
| 14 | First-hour trades (apples $250/h, fishing $300/h plus $90 of tackle) sit under the police entry rung ($350/h). Read as the entry step before courier work, nothing changed; a call for whoever owns the ladder | Round 2, "Pay against the ladder" | none (a balance call, not filed) | S | New players are not paid less than a patrol officer for long |
| 15 | A prisoner's own HUD jail clock can show up to 15 s late after a rejoin (the next write waits for a 15 s step) | JIP sweep | none (minor, not filed) | S | A true jail timer after a reconnect |
| 16 | A fresh account (bank $2,000) cannot pay bail on a 15 min sentence ($2,250) until about 2 minutes have passed | Crime playtest | none (a balance call, not filed) | S | Decide if the first sentence is meant to be unpayable |

Rows 1 to 7 are what a first-week player hits. Rows 8 to 11 are proof and polish for what 0.3.0 already ships. Rows
12 to 16 are low impact.

## Closed since the playtests (merged, in-dev; not in the table)

The rounds each fixed their own findings, and the three open ones the reports filed are merged too. Nothing below
needs work.

| Finding | Merged fix |
|---|---|
| Sale toast "Sold item for $18"; market card title; garage showed three cards; F3 guide on a finished player; F2 "Found a crew $400/h" (#5119) | #5124, #5125 and the round 1 PR |
| Courier toast pointed the wrong way and hid the $25 deposit | #5130 |
| Opening the map logged 60 "Can't find image" lines (#5127) | #5132 |
| Civilian cars drew no plate (#5129) | #5134 |
| House no-cash toast, processor "parts" toast; outpost toasts cut to a quote; house id in the purchase toast; "Alley yard lease" | round 3 and 4 PRs (`1f69f677c`, `3066a7411`) |
| Furniture could not be placed in an island house | `0fe2e7827` (`placement/client-offer-marker`) |
| The house sign still read FOR SALE after a purchase | #5173, relanded `6cb591371` |
| "Sentence served" after bail; a permit "seized" that was never held; the suspect not told what the search took | #5150 |
| Charge-sheet row cut off and worth $0 (#5147); first-arrest toast said "book at the cell" (#5149); dispatch said "Factory" | #5156 |
| Officer search toast named a suspect never met (#5145); clock-in showed -$100 with no word on the locker (#5148) | #5158 |
| A downed player paged nobody (#5152); the bed left the patient under the frame (#5153); VME on give-up (#5154); Revive was the ninth row, no timer (#5155) | #5159, #5160, #5162 |
| Downed line missing after a rejoin or for a listen host | `564236ff2` |
| Truck yard sheet said nothing of the truck license; crane toast read "$1040" and the short count did not say how many were missing | #5144, `fdeb116d9` |
| Proofs read "$1,040" as 1 | `491c9b7ab` |
| Test `investigation/locker-fill` red alone (#5166) | #5170 |
| Evidence cards showed `Fuel Station^Saint Philippe` | `4b462f944` (pre-RC review part 4) |

## Not filed, and why

- **Other six languages still say "item" for a sale.** They carry the English text by design; only en_us is edited
  (round 1).
- **Pole plates are single-sided; "VEGETABLE PLOTS" stands behind an elder bush; the outpost chalk board is cut off
  from the gate; the post counter lists fuel and taxi sign-up rows before sign-up.** Town placement the maintainer
  edits by hand; no agent town cleanup.
- **The ATM opened on the Transfer page once.** Not reproduced; likely the harness's first key.
- **The `ActivityDef` attribute help still says "0 for unknown"** though a 0 now reads "no wage". No shipped row is
  affected (pre-RC review, #5125).
- **Chrome or untextured props for a frame after a teleport** (outpost, clock-in desk): engine streaming at low fps.
- **`drive-haul-proof` driven legs, the `booking-desk-proof` frame gap, the `starter-gates-proof` order, the
  `export-crane-proof` money regex and the `downed-flow-proof` menu order** were harness faults or load, fixed or
  dropped by the RC proof work.
- **Missing features nobody asked for yet:** a bank loan, a civilian vault, a property locker for a jailed player
  (an unlicensed pistol is seized for good, by design).
- **The hostel clerk audio, a walk timed on foot, and a human ten-hour run** need a person at the keys; they are on
  the #5200 checklist.
