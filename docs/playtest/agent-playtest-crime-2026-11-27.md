# Agent playtest 2026-11-27: crime and police, two players

**Build played:** `ws/playtest-crime-1127` on `origin/main` `eb1857d3` (`compiled-ok`), the 0.3.0 feature freeze (#4836): fixes only.
**Setup:** two `--profile full` island sessions on lf-testbox (`up --mission island --clients 2 --hold 20`, `session down` straight after each), driven by the new proof `agent-box/proofs/crime-playtest-proof.mjs`. Client 1 (**CIV**) is a fresh civilian given the hour-10 purse (`checkpoint 10`: $450 cash, $8,450 bank) and a pistol. Client 2 (**COP**) is a fresh account who signs up at the Saint-Philippe duty desk (alpha approve-on-submit) and clocks in. Every step reads both clients (`state`, `peers`, `seen`) and takes a picture on each.
**Route:** CIV robs the fuel till through the attendant; COP gets the dispatch, asks for hands up, cuffs, searches, escorts and books CIV at the desk; CIV takes the time, then pays bail at the cell phone and walks out without the pistol; COP clocks out and is paid.
**Logs:** no `VME`, `Unable to find` or `Can't find` on the server or either client. The only `SCRIPT (E)` lines are sound events that did not start (dispatcher voice, fuel clerk lines, booking stamp and typewriter): the session clients run without sound, the known case in `docs/debug-log-triage.md`.

Session 1's first run lost the arrest half to the proof's own timing (it answered the hands-up prompt after its 8 s window and cuffed after the 30 s hands-up window); session 1's third run is the clean one used here (16 steps, 14 pass, the two "fails" are design, see below). Sessions 2 and 3 replayed the route on this branch (session 2's pictures were lost when `down` removed the folder before `pull`; its log has the toasts, session 3 has the pictures).

## Findings, ranked by player impact

| # | Rank | Finding | Who sees it | Picture | What I did |
|---|---|---|---|---|---|
| 1 | High | After paying bail the prisoner reads "Bail of $2,250 paid from your bank. You are free to go." and 1.5 s later **"Sentence served"**. The second toast contradicts the first: they did not serve, they paid | CIV | [after bail, before](../../website/public/docs-images/playtest-crime-1127/c1_after_bail_before.jpg) | **Fixed in this PR**: `LF_Jail.EndOnBail` releases with no served toast. Test `police/jail-bail` (red on the unfixed code: `expected 0, got 1`) |
| 2 | High | A robber who never held a firearm permit reads **"Your firearm permit has been seized for 14 days."** in the cell. Nothing was seized; the robbery conviction bars them from getting one | CIV | [in the cell, before](../../website/public/docs-images/playtest-crime-1127/c1_in_cell_permit_before.jpg) | **Fixed in this PR**: a ban on a licence the player did not hold reads "You cannot get a firearm permit for 14 days." (`#LF-LicenceBarred_*`). Test `licence/seizure` (red on the unfixed code) |
| 3 | High | The search takes the suspect's M9 and **tells only the officer** ("Seized from ...: M9"). The suspect's screen shows nothing; they find out when they reach for the gun | CIV | [CIV after the search](../../website/public/docs-images/playtest-crime-1127/c1_searched_before.jpg), [COP](../../website/public/docs-images/playtest-crime-1127/c2_searched.jpg) | **Fixed in this PR**: the suspect gets "The police took: M9" (`LF_ItemSearch.TakenText`). Test `catalogue/search-seized-list` (`--red` OK) |
| 4 | Medium | The officer's search toast names the suspect although the officer was never introduced (`seen`: `known=0`): in the session the name was the client's account name | COP | [COP after the search](../../website/public/docs-images/playtest-crime-1127/c2_searched.jpg) | Issue #5145 |
| 5 | Medium | Charge sheet: the second row reads **"Carrying without a licen"** (cut off) and is worth **$0**. `#LF-Offence_UnlicensedItem` (and the restricted and illegal item codes) carry no fine and no time, so the officer is offered a charge that does nothing | COP | [charge sheet](../../website/public/docs-images/playtest-crime-1127/c2_charge_sheet.jpg) | Issue #5147 |
| 6 | Medium | Clocking in flashes **-$100** on the money HUD with no word that the cash went into the duty locker; it reads as a fee for going on duty. In that shift clock-out flashed **+$153**, the locker cash and the $54 pay together, beside "Duty pay: $54 cash" | COP | [clock in](../../website/public/docs-images/playtest-crime-1127/c2_clocked_in.jpg), [clock out](../../website/public/docs-images/playtest-crime-1127/c2_clocked_out.jpg) | Issue #5148 |
| 7 | Medium | The first-arrest toast tells the officer to "book them at the cell: full sentence, or reduced if they cooperated". On the island booking is at the desk with the charge sheet; the last step is wrong | COP | [first arrest toast](../../website/public/docs-images/playtest-crime-1127/c2_cuffed.jpg) | Issue #5149 |

### Money checks

| What | Expected | Read | Verdict |
|---|---|---|---|
| Police duty pay (lowest rung, #4178) | $350 to $450 an hour | $353/h, $358/h and $338/h over three shifts (`[LFDebug:DutyPay] shift pay +24` for 245 s, `+25` for 251 s, `+19` for 202 s: whole seconds paid, the idle pause applies, so a 3-minute shift settles a little under the band); toast "Duty pay: $34 cash" with the duty bonus | OK |
| Bail | $150 for each started minute left, minimum $500 (pack `LF_JailBreak_Everon1989.conf`) | 15 min left: $2,250 from the bank, bank $8,450 to $6,200, cash untouched (`paid bail 2250 from the bank (left 6200)`) | OK |
| Till robbery | marked notes, not cash (#2851) | HUD cash unchanged at $450; the backpack holds the marked notes; the clerk says "I'm opening it" and the robbery panel counts down | OK (by design) |
| Fine | jail-only robbery: no fine | the sheet's total reads "-", the suspect's only row is "Take the time (15 min)" | OK |

### What worked, on both screens

- The robbery: the attendant dialogue, the "Keep the clerk at gunpoint: 0:20" panel ([CIV](../../website/public/docs-images/playtest-crime-1127/c1_robbery_hold.jpg)), wanted 1, the fuel alarm and the radio dispatch to the officer (`dispatches=1`).
- Hands up: "Put your hands up? Press F7 to raise your hands" ([CIV](../../website/public/docs-images/playtest-crime-1127/c1_handsup_prompt.jpg)), answered, logged `player 1 raised their hands`.
- Cuffs: CIV's own state and COP's view of CIV both read `cuffed=1 worn=1`; COP gets the "Jail booking 117 m" waypoint ([COP](../../website/public/docs-images/playtest-crime-1127/c2_cuffed.jpg)).
- Booking: "Book suspect" opens the charge sheet, signing toasts "Suspect booked: 15 min." ([COP](../../website/public/docs-images/playtest-crime-1127/c2_signed.jpg)); CIV sees "Take the time (15 min)" ([CIV](../../website/public/docs-images/playtest-crime-1127/c1_take_the_time.jpg)), lands in the cell with "Jail: 14:56 left" on the HUD and the clock counts down (895 to 878 s).
- Bail and release: the phone's row is offered in the cell ([CIV](../../website/public/docs-images/playtest-crime-1127/c1_bail_phone.jpg)); after paying, CIV stands in the hall, free of cuffs and warrant, and COP's client agrees (`peers`: `cuffed=0`). The pistol is gone, as the law says for a weapon with no permit ([inventory](../../website/public/docs-images/playtest-crime-1127/c1_inventory_after_release.jpg)); nothing tells the player where it went (finding 3).

### After this PR (session 2)

`crime-playtest-proof` on this branch: PASS, 16 of 16 steps (the robbery step counts the marked-note payout; the gear step checks that the pistol stays seized and that the suspect was told).

| Fix | Before | After |
|---|---|---|
| 1 bail | ["Sentence served" after paying bail](../../website/public/docs-images/playtest-crime-1127/c1_after_bail_before.jpg) | [the bail toast fades out and nothing follows](../../website/public/docs-images/playtest-crime-1127/c1_after_bail_after.jpg); the payer's last toast is `#LF-JailBail_Paid` |
| 2 permit | ["Your firearm permit has been seized for 14 days."](../../website/public/docs-images/playtest-crime-1127/c1_in_cell_permit_before.jpg) | ["You cannot get a firearm permit for 14 days."](../../website/public/docs-images/playtest-crime-1127/c1_in_cell_permit_after.jpg) |
| 3 search | [nothing on the suspect's screen](../../website/public/docs-images/playtest-crime-1127/c1_searched_before.jpg) | ["Police Action: The police took: M9"](../../website/public/docs-images/playtest-crime-1127/c1_searched_after.jpg) |

## Dismissed

- **Hands-up prompt "missed" and cuffing refused** in the first run: the proof captured a picture and read the screen before answering, past the prompt's 8 s life (`LF_HandsUp.PROMPT_MS`); later the cuff started 25 s into the 30 s hands-up window. A player pressing F7 is well inside both. The proof now answers first and pictures after.
- **"COP escorts CIV" did not move CIV**: the harness `stand` verb teleports the officer, so the escorted suspect is left behind; the escort itself started (`escort=ok`). Walking escort is covered by `arrest-flow-proof`.
- **No dispatch toast in the officer's pictures**: the radio toast (`#LF-Dispatch_Radio`, "All units: Robbery at Factory...") was delivered (`dispatches=1`); the picture was taken after it faded. Not a bug from this evidence.
- **Gear "retrieval"**: there is no property locker for a jailed player; an unlicensed pistol is seized for good. That is the law as written, not a bug; finding 3 is the missing feedback.

## Noticed (no change)

- The dispatch names the fuel station's place as "Factory" (`#AR-MapLocation_Factory`, the nearest vanilla map label), not Saint-Philippe.
- In one clock-in picture the desk NPC and props drew as untextured chrome for a frame (streaming at 14 fps); gone in the next picture.
- A fresh account (bank $2,000) cannot pay bail on a 15 min sentence ($2,250) until about 2 minutes have passed; known, #5047.
- The cop clocked in as a "New in town" character keeps the town-clerk waypoint on screen through the whole arrest.

## Re-run

```
node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20 --wait
node agent-box/session.mjs crime-playtest-proof [--name <shot prefix>]
node agent-box/session.mjs down
```

Pictures land in each client's `logs/<run>/el-captures/` as `<prefix>_c<client>_<step>.bmp`.
