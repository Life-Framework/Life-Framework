# Agent playtest 2026-11-27: two players, one medic, on Everon

**Build played:** `ws/playtest-medic-1127` on `origin/main` `122b9bf7` (`green`), the 0.3.0 feature freeze. Game client 1.9.0.42.
**Setup:** one `--profile full` island session on lf-testbox (`up --mission island --clients 2 --hold 20`), two fresh accounts (rules quiz, form R-89, $100 cash and $2,000 bank each), about 13 minutes up, `session down` straight after. Client 1 is the patient, client 2 the medic.
**Route:** client 2 clocks in at the Saint-Philippe clinic duty desk; client 1 goes down at the dock, 320 m away; the medic draws an ambulance from the clinic's motor pool, drives to the dock, walks the last stretch and revives. Client 1 goes down a second time and gives up (death screen, hospital respawn, wake bill). Client 1 goes down a third time beside clinic bed 1; the medic puts them in the bed. Client 1 pays a bill at the desk; the medic clocks out and is paid.

**Harness caveats.** The harness has no fall or crash damage, and a player cannot shoot themselves (`shoot 1` from client 1 answered `no_target`), so client 2 downed client 1 with `shoot 1` each time. That makes the medic the downer (wanted 3 by the end), which no real medic would be. `stand` teleports, so only the ambulance drive was a real move. Toasts last 3 to 8 s and some captures came after them; where a toast was missed the text is quoted from `state toast=` and the server's `Notify` lines.

Findings are ranked by what a player hits. Text was read for US English: every medic, bill, bed, duty and downed string the route showed is US English (no "licence", "colour" or "centre").

## Findings

| # | Rank | Finding | Where | Pictures | What I did |
|---|---|---|---|---|---|
| 1 | High | **A player who goes down sends no call to the medic.** Client 1 lay at the dock for three minutes. The on-duty medic got no pager card, no board row, no map mark and no toast (`seen` read `dispatches=0` throughout). The downed player cannot reach a payphone. The medic job's calls are NPC casualty rolls and third-party payphone calls; a player alone on the ground has no way to get help | `LF_OnPlayerDowned` has no listener besides the incident log | medic [c2](../../website/public/docs-images/playtest-medic-1127/c2_after_down.jpg), patient [c1](../../website/public/docs-images/playtest-medic-1127/c1_downed_0s.jpg) | **Issue #5152** (a new call source in the freeze: the lead's call) |
| 2 | High | **"Put patient in bed" leaves the patient on the floor under the bed.** The bed action ran for a joined client (the #5111 fix works) and both screens got their toast, but client 1 woke under the bed frame, prone, camera inside the frame, and was still there a minute later. Server: `no room for a body beside bed Fit_saint_philippe_clinic_lf_hospital_bed ...; it wakes at <4435.79,10.95,10782.2> anyway`. The hospital respawn after give-up woke them at the same spot | Clinic bed 1, Saint-Philippe | patient [c1](../../website/public/docs-images/playtest-medic-1127/c1_bed_wake.jpg), [c1 a minute later](../../website/public/docs-images/playtest-medic-1127/c1_bed_wake_later.jpg), medic [c2](../../website/public/docs-images/playtest-medic-1127/c2_bed_done.jpg) | **Issue #5153** (a town placement; the maintainer moves beds by hand) |
| 3 | Medium | **Revive is the ninth row on a downed body.** The medic facing the patient sees "Introduce yourself" on top, then greyed "Show ID" and "Give cash"; Revive comes after ladder, loot, inspect, the vanilla load, Introduce, Show ID, Give cash and Give item | Player character actions (`Character_Roleplay.et`) | medic [c2](../../website/public/docs-images/playtest-medic-1127/c2_at_patient.jpg) | **Issue #5155** (needs a sort-priority check in a session) |
| 4 | Medium | **A VME on the server and both clients when the patient gives up.** `NULL pointer to instance` in vanilla `SCR_CharacterHelper.GetCharacterControlType` (no `SCR_PossessingManagerComponent` in the game mode), reached from `LF_Downed.Finish` because the give-up death names the downer as a player instigator. The death screen and respawn still ran. `debug-log-triage.md` called this VME harness only | Give-up and bleed-out while the downer is online | none (log) | **Issue #5154**; the triage row is corrected in this PR |
| 5 | Medium | **The downed screen goes silent after 8 s.** The only text is the toast at the fall; no seconds left, no "you can give up now", nothing about a medic. At about 140 s left the screen showed the vignette and a "The town clerk 439 m" waypoint, nothing else | Downed player HUD | [c1, 140 s left](../../website/public/docs-images/playtest-medic-1127/c1_downed_late.jpg) | **Issue #5155** for a timer line. **Fixed in this PR**, the first toast: it now reads "You are down and bleeding out. Wait for help, or give up (hold X) after 30 seconds." (was "...You can give up (hold X) in 30 seconds.", which never said help could come) |
| 6 | Medium | **The too-early give-up toast always said "30 seconds".** About 25 s after going down, holding X answered "You can give up in 30 seconds from going down.": the whole delay, not the 5 s left. It reads as if the wait started over | `LF_BleedOut.GiveUp`, `#LF-Medic_GiveUpTooEarly` | [c1, old text](../../website/public/docs-images/playtest-medic-1127/c1_giveup_early.jpg) | **Fixed in this PR**: `LF_BleedOut.SecondsUntilGiveUp` passes the seconds still to wait (never under 1); the toast reads "You can give up in 5 seconds." Test `medic/bleed-out-rules` |
| 7 | Low | **Clocking in takes the medic's cash with no word.** The readout went "-$100 $0" and the toast said only "You are now on duty". The $100 is in the duty locker with the clothes and came back at clock-out, but nothing said so; an orderly with $0 in the pocket cannot buy a drink. Police desks do the same | Duty desk (`LF_DutyAction`, `LF_DutyLocker`) | [desk before](../../website/public/docs-images/playtest-medic-1127/c2_desk_before.jpg), [after clock-in](../../website/public/docs-images/playtest-medic-1127/c2_clockin.jpg) | **Fixed in this PR**: the clock-in toast (`#LF-Duty_ClockedIn`, 6 s) reads "You are now on duty. Your own clothes, cash and things wait in the duty locker until you clock out." |

Fixes 5, 6 and 7 are toast text; they have no picture of their own (the session was down before they were written). The red run for 6: `SecondsUntilGiveUp` returning the whole delay failed `the too-early toast names the seconds still to wait` (expected 5, got 30); green after the fix, with `DutyDeskActions`, `MedicDuty`, `PoliceStation`, `Workplace` and `DutyDeskRosterRefusal` (12 of 12).

## Pay and money

| What | Amount | Read from |
|---|---|---|
| Medic shift, orderly, 9.6 min (18:37:38 to 18:47:14) | **$75** (about $469/h; the rung is $480/h, the rest is rounding) | `[LFDebug:DutyPay] shift pay +75` |
| Duty bonus (one revive $10, one bed delivery $15; the revive bonus was replaced by the delivery) | $25 | `[LFDebug:DutyBonus]` |
| Medic share of the bill the patient paid | $50 (half of $100) | `[LFDebug:Bill] medic share +50` |
| Medic cash after clock-out | $250 (the $100 from clock-in back, plus $150) | `state cash=250` |
| Patient: revive bill, wake bill, bed bill | $100, $150, $100 (three paper bills) | `[LFDebug:Bill]` |
| Patient paid the oldest at the desk | -$100, "Bill paid: $100." | [c1](../../website/public/docs-images/playtest-medic-1127/c1_bill_paid.jpg) |

`tools\cli economy`: medic orderly $480/h, EMT $500, paramedic $530, doctor $560, at most $600/h with the bonus; police patrol $350/h up to lieutenant $450/h. Medic pay sits above police at every rung, as the 2026-11-22 order wants, and the measured shift matches the orderly rung.

## What read well

- **The death screen.** "You are dead. You wake at the hospital. The bill is $150. What you carried stays with your body." and one Respawn button ([c1](../../website/public/docs-images/playtest-medic-1127/c1_after_giveup.jpg)).
- **The hospital bill sheet.** Bill number and date, "Treatment: $150", "Pay it in cash at the hospital desk.", "You don't remember how you got here.", the New Life Rule (15 minutes), "Put the bill away" ([c1](../../website/public/docs-images/playtest-medic-1127/c1_hospital_wake.jpg)).
- **The motor pool.** A "MOTOR POOL" board beside the clinic with one F row; the ambulance stands at the curb when it is pressed ([c2](../../website/public/docs-images/playtest-medic-1127/c2_ambulance_drawn.jpg)). The ambulance drove 443 m at up to 48 km/h.
- **The revive itself.** One 4 s hold, patient up (`downed=0`), "You brought them back." for the medic, a $100 bill for the patient. A "Medic, Level 2, Apprentice. Opens: First aid certificate: treat the injured" card on the bed delivery ([c2](../../website/public/docs-images/playtest-medic-1127/c2_bed_done.jpg)).
- **Clock-out.** "Clock in" is back on the desk, the readout shows the pay ([c2](../../website/public/docs-images/playtest-medic-1127/c2_clockout.jpg)).
- **No desync seen.** Both clients and the server agreed on the downed flag, the revive, the bed wake and the bills (`peers`, `state`, `seen`).

## Not reached (say so, do not read it as a pass)

- **Loading the patient into the ambulance.** The dock's bar gate stopped the ambulance 46 m from the patient (`gate_ahead_shut_door_BarGate_01`), outside the 5 m load range; with the bleed-out clock running I switched to the give-up leg. The bed leg was done with the body beside the bed, not from the ambulance.
- **A natural injury.** No fall or crash; every down was a harness shot by the medic.
- **The ambulance ride with a patient**, the siren, the stretcher seat, and **first aid** (bandage on a downed player).
- **A full bleed-out.** Every down ended in a revive, a give-up or the bed.
- **The patient standing up.** After both revives `state` read `stance=PRONE` for over a minute; the harness has no stand-up verb, so whether a player can stand at once was not checked.
- **Audio** (`-noSound`).

## Noticed (no action)

- The ambulance row reads "Take the Orderly ambulance": the rank name in the row, which reads like a vehicle model. No toast came when the ambulance appeared.
- The bill says "Pay it in cash at the hospital desk". The clinic's till, the obvious desk, offers only "Rob the till"; the Pay bill row is on the duty desk beside the punch clock ([c1](../../website/public/docs-images/playtest-medic-1127/c1_till_prompt.jpg)).
- The death screen says "What you carried stays with your body", but the $100 cash was still in the pocket after the respawn.
- A patient revived and then dying anyway keeps both the $100 revive bill and the $150 wake bill.
- The "The town clerk" waypoint stays on screen while downed and draws over the death panel.
- At the clock-out the readout animated "+$249" for a $250 change (mid-count).
- `harness vehicle enter driver r=30` without a `veh_` filter picked a beige S105 parked beside the clinic over the ambulance.

## Tooling notes

1. A player cannot `shoot` themselves; downing a patient needs a second client, and then that client is the downer (wanted, the give-up instigator). A `hurt <player> fall|crash` verb with no instigator would make medic proofs honest.
2. `parkat` needs a placed entity, not a player name (`no_entity Player1`), so the ambulance cannot be set beside a patient who lies outside a named place.
3. `session.mjs` captures land as BMPs under `client<n>/logs/<run>/el-captures/`; `bmpFileToPng` in `agent-box/bmp.mjs` makes PNGs.
