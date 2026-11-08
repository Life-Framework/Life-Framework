# Police — Design Requirements

> Context file for law enforcement. Written against
> `docs/design.md` (in-world first).

## Intent

Policing is **physical**: you go on duty at the station, you arrest at the
scene, you confiscate on the suspect. The management surface (wanted list,
fines, records) is a station terminal — a world-anchored rung 3 — not a menu
that works from anywhere.

## Interaction pattern

Rung 1 for duty/arrest/confiscate; rung 3 target for the station terminal.

Today: `EL_DutyAction` toggles the on-duty flag; `EL_PoliceMenu` shows the
wanted list with server-guarded Arrest/Fine; `EL_ConfiscateAction` deletes
weapons from the nearest wanted player within 5 m; `EL_OpenPoliceMenuAction`
is the entry point.

## V1 (shippable)

1. **Arrest/Fine are unreachable from a real client**: the menu runs on the
   client but both methods early-return unless `Replication.IsServer()`, with
   no `RpcAsk_*` bridge. Add the client→server ask → server-validate → do path
   (and validate the target on the server, never trust the client's pick).
2. Guard `account.GetActiveCharacter()` (null deref) and replace the hard-coded
   jail position `"0 0 0"`.
3. `EL_ConfiscateAction` deletes items while iterating a live `FindItems`
   array (mutation-during-iteration risk) and gives no compensation/wanted
   reduction.
4. Duty toggle evicts the account (cache churn) — fix via `Account`.
5. ~~`#EL-Fined %1!` builds a non-key~~ — the remote fine is gone (M1-19).

## Iteration path

- **V2** — handcuffs / tasers, physical transport to jail (Phase 3 roadmap).
- **V3** — search & frisk, station terminal (rung 3), records.

## Current state

- Duty desk (M1-11): `Roadmap_Police_Desk` (`Prefabs/Police/LF_PoliceDutyDesk.et`)
  carries `EL_DutyAction` (Clock in / Clock out). `LF_DutyLocker` checks the
  police roster (`EL_WhitelistManager`) on the server and swaps the civilian and
  police inventories (`civilian_stash` / `police_duty` snapshots, persisted).
  A first shift gets the starter police kit (config). Clock-out pays duty pay
  (`LF_DutyPay`). There is no faction screen; the faction is the duty role.
- Wanted board (M1-11): `Roadmap_Police_WantedBoard` (`Prefabs/Police/LF_WantedBoard.et`),
  `LF_WantedBoardReadAction` -> `LF_WantedBoard` (per-read RPC). Posters come
  from `LF_WantedReports`: a description at the crime, the name once identified
  (ID check to an on-duty officer, or caught in the act).
- Alarm and dispatch (M1-20): every crime calls `LF_Dispatch.Report`. The site
  alarm is a replicated `LF_SiteAlarm` prefab (vanilla siren loop and a
  flashing light; `LF_SiteAlarm_Loud` when nobody is on duty). On-duty officers
  hear a vanilla radio chirp, then read a dispatch line with the place and the
  description, never a name. The wanted board lists recent dispatches for police.
- `EL_PoliceMenu` — arrest entry point; the wanted list moved to the board.
- Fine as a paper ticket (M1-19): Write ticket (player interaction `fine` slot)
  opens the ticket pad; the holder carries `Prefabs/Items/Roleplay/FineTicket.et`
  and pays it in carried cash at the desk punch clock (Pay ticket). Deadlines
  count online time: x2 once, then +1 wanted and an Unpaid fine poster.
  Records persist (save version 12).
- `EL_ConfiscateAction` — weapon confiscation; ⚠️ mutation-during-iteration.

## Dependencies

- `Crime` (the wanted state it enforces), `Account` (duty, wanted), `Character`
  (inventory), `Houses` (raiding later), `License` (revocation).
## Jail (M1-21)

- The sentence is online time: `LF_Jail.Tick` advances connected prisoners only.
- Sentencing hands a real paper, the sentence slip (`LF_JailSlip.c`,
  `Prefabs/Items/Roleplay/SentenceSlip.et`); the inventory tooltip reads it.
- Release is in the world: cell door opens (vanilla door sound), the prisoner
  stands outside the door, the slip is stamped, then the toast.
- A seated prisoner is dismounted to the cell; the vehicle never moves.
- Later: in-jail work that shortens the sentence (#127).
