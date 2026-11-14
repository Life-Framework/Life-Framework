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

Today: `LF_DutyAction` toggles the on-duty flag; `LF_PoliceMenu` shows the
wanted list with server-guarded Arrest/Fine; `LF_ConfiscateAction` deletes
weapons from the nearest wanted player within 5 m; `LF_OpenPoliceMenuAction`
is the entry point.

## V1 (shippable)

1. **Arrest/Fine are unreachable from a real client**: the menu runs on the
   client but both methods early-return unless `Replication.IsServer()`, with
   no `RpcAsk_*` bridge. Add the client→server ask → server-validate → do path
   (and validate the target on the server, never trust the client's pick).
2. Guard `account.GetActiveCharacter()` (null deref) and replace the hard-coded
   jail position `"0 0 0"`.
3. `LF_ConfiscateAction` deletes items while iterating a live `FindItems`
   array (mutation-during-iteration risk) and gives no compensation/wanted
   reduction.
4. Duty toggle evicts the account (cache churn) — fix via `Account`.
5. ~~`#EL-Fined %1!` builds a non-key~~ — the remote fine is gone (M1-19).

## Iteration path

- **V2** — handcuffs / tasers, physical transport to jail (Phase 3 roadmap).
- **V3** — search & frisk, station terminal (rung 3), records.

## Current state

- Duty desk (M1-11): `Roadmap_Police_Desk` (`Prefabs/Police/LF_PoliceDutyDesk.et`)
  carries `LF_DutyAction` (Clock in / Clock out). `LF_DutyLocker` checks the
  police roster (`LF_WhitelistManager`) on the server and swaps the civilian and
  police inventories (`civilian_stash` / `police_duty` snapshots, persisted).
  A first shift gets the starter police kit (config). Clock-out pays duty pay
  (`LF_DutyPay`). There is no faction screen; the faction is the duty role.
- Wanted board face (#758): `LF_WantedPosters` (server list, events, coalesced) ->
  `LF_WantedPosterBoardComponent` on `LF_WantedBoard.et` (RplSave/RplLoad + broadcast) ->
  `LF_WantedPosterFace` (one render target on `LF_BoardFace.et`, drawn within
  `wantedBoard.drawRadius`, frozen 3 s after each change). Config `LF_WantedPostersConfig`
  (`Configs/Police/LF_WantedPosters.conf`, owner section `wantedBoard`). The dispatch boards
  inherit the component with `m_bDrawPosters 0`.
- Wanted board (M1-11): `Roadmap_Police_WantedBoard` (`Prefabs/Police/LF_WantedBoard.et`),
  `LF_WantedBoardReadAction` -> `LF_WantedBoard` (per-read RPC). Posters come
  from `LF_WantedReports`: a description at the crime, the name once identified
  (ID check to an on-duty officer, or caught in the act).
- Alarm and dispatch (M1-20): every crime calls `LF_Dispatch.Report`. The site
  alarm is a replicated `LF_SiteAlarm` prefab (vanilla siren loop and a
  flashing light; `LF_SiteAlarm_Loud` when nobody is on duty). It stands on the
  nearest `LF_AlarmMountComponent` that reaches the site (a bell box on a wall,
  a van roof; give a robbable place one), else on the surface under the site.
  On-duty officers
  hear a vanilla radio chirp, then read a dispatch line with the place and the
  description, never a name. The wanted board lists recent dispatches for police.
- `LF_PoliceMenu` — arrest entry point; the wanted list moved to the board.
- Fine as a paper ticket (M1-19): Write ticket (player interaction `fine` slot)
  opens the ticket pad; the holder carries `Prefabs/Items/Roleplay/FineTicket.et`
  and pays it in carried cash at the desk punch clock (Pay ticket). Deadlines
  count online time: x2 once, then +1 wanted and an Unpaid fine poster.
  Records persist (save version 12).
- `LF_ConfiscateAction` — weapon confiscation; ⚠️ mutation-during-iteration.
- Handcuffs (#664): `LF_Restraint.c`. Cuff and Uncuff in the interaction menu; the officer holds
  `Prefabs/Items/Roleplay/Handcuffs.et` (`LF_HandcuffsComponent`) and the target is downed,
  stunned, or held up and unarmed (`m_iCompliantSeconds`). The cuffs go onto the suspect and come
  back on release, at the cell (`LF_Jail.Arrest`), on the suspect's death or disconnect. A cuffed
  player has no weapon controls (replicated `m_bLF_Restrained`). No restrained animation exists
  in the base game (#340); the cuffs show on the right hand.
- Cuffing is a hold (`cuffing-open-state`, `LF_Cuffing.c`): about 1.8 s to cuff and 1.5 s to uncuff on
  the vanilla progress bar. The cuffs in the officer's hand swing open at the start of a Cuff (the
  cuffs on the wrist open at the start of an Uncuff), the hold cancels if either person moves out of
  reach, and the end shuts them with a click and puts them on the suspect (or takes them off). The
  server watches the pair, and accepts only an end it saw begin, after most of the duration.
- Spike strip (#646): every rank's armory kit carries the folded strip. Deploy it from the
  ground in front of you (on-duty police, rate limited); the server punctures each wheel whose path
  crosses it through the vanilla wheel damage, and an officer picks it up again. It goes after
  `m_iSpikeStripLifetimeSeconds`. `LF_SpikeStrip.c`, `LF_SpikeStripActions.c`.

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

## Jail work (#127)

`LF_JailWork.c`: job spots in the cell (`LF_JailWorkAction`) take seconds off a serving prisoner's sentence through `LF_Jail.Credit`; the server owns the rules (serving, not busy, per-job cooldown). Pure rules in `LF_JailWorkRules`; test `jail/work`. No persistence change.

## Npc stun (#922)

- **Player sees:** shooting a town NPC with the taser (or any weapon carrying `LF_StunWeaponComponent`) drops it onto its back for the weapon's stun time; the shooter gets an "<name> is stunned" toast; the NPC offers no talk, shop or question action meanwhile; then it stands up where it fell.
- **Owner and side:** `LF_NpcStun` decides on the server; the shot is read from the shooter's `OnProjectileShot` (a dart finds no hit zone on a town NPC); `LF_NpcBaseEntity` replicates the flag and rest pose and each peer lays the body down.
- **Data:** string key `LF-Police_NpcStunned`; stun time and reach are the weapon's (`LF_StunWeaponComponent`); nothing persists (a restart stands the NPC up).
- **Proof:** `police/npc-stun` (LOGIC), `police/npc-stun-world` (WORLD), `node agent-box/session.mjs npc-stun-proof`.

## Robbery alarms (#740)

- **Player sees:** the alarm at a robbery has its own sound: a bell at the bank vault, an intruder alarm at the fuel till, a small buzzer at a store till, a horn at the cash van. The siren stays for crimes with no kind.
- **Owner and side:** `LF_Dispatch.StartAlarm` decides on the server; the alarm prefab replicates and each client plays its sound (`LF_SiteAlarmComponent`).
- **Data:** `m_aAlarmKinds` in `Configs/Police/LF_Police.conf` (`LF_AlarmKind`), the `m_sAlarmKind` attribute on `LF_RobAction` and `LF_BankVaultComponent`, the prefabs `Prefabs/Police/LF_SiteAlarm_<Kind>[Loud].et`, the sounds `Sounds/Alarm/Robbery/`. Nothing is persisted.
- **Proof:** `police/robbery-alarms` (LOGIC), `police/robbery-alarms-shipped` (WORLD), `robbery-alarm-proof`.
