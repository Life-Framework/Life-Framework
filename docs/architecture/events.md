# Server event bus

`LF_EventBus` (`Scripts/Game/Core/Events/`) lets a feature announce what just
happened without knowing who cares. A crime raises a wanted report, dispatch
pages, the incident log records it: each listens; none calls the others. A pack
or third-party addon subscribes the same way. This is part of the supported API
(M3 #223); decided 2026-11-08 (`docs/decisions.md`).

## Rules

- **Server only.** `Publish` from a client logs `LFDebug:Events` Error and does
  nothing. The bus takes no input from clients; publishers publish after their
  own server-side validation. Never put a client RPC payload straight into an event.
- **Typed.** An event is a class extending `LF_Event` with plain fields (uids,
  ints, strings; never `IEntity` or `EntityID`). A subscriber receives events of
  the exact class it subscribed to; a subclass is a different event.
- **Ordered.** Subscribers run in the order they subscribed.
- **Isolated.** Enforce Script has no exceptions, so isolation means: every
  subscriber is called individually; a `Handle` that returns `false` logs
  `LFDebug.Error` (`[LFDebug:Events] <Event>: subscriber <Name> reported an
  error`) and the rest still run; a subscriber deleted mid-flight is skipped.
  Handlers return `false` themselves when their work fails. A script VM
  exception inside a handler is a bug in that handler; the bus cannot catch it.
- **Safe to change during a publish.** The subscriber list is copied before the
  calls. A handler that unsubscribes (or subscribes) takes effect from the next
  publish; a subscriber removed mid-publish still receives the current event.
- **No per-frame events.** Publish on state changes only. A handler may publish
  another event, nested up to `MAX_DEPTH` (8); beyond that the publish is refused
  and logged.
- **Static state.** The table lives in statics; `LF_StaticReset.ResetAll` clears it
  at world end. Subscribe from `OnPostInit` or a manager `Init`, not once and forget.

## Using it

```c
class LF_IncidentLogCrime : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnCrimeCommitted crime = LF_OnCrimeCommitted.Cast(busEvent);
		if (!crime)
			return false;

		// read crime.m_sPlayerUid, crime.m_sOffence
		return true;
	}
}

LF_EventBus.Subscribe(LF_OnCrimeCommitted, new LF_IncidentLogCrime());
```

Hold the subscriber you want to remove: `LF_EventBus.Unsubscribe(LF_OnCrimeCommitted, sub)`.
The bus keeps its own strong reference.

## Events

| Event | Fields | Published by (where it already logs the change) |
|---|---|---|
| `LF_OnCrimeCommitted` | `m_sPlayerUid`, `m_sOffence` (offence key) | `LF_WantedReports.Report`: every robbery, theft, break-in, murder, and an unpaid ticket, just before the report is filed ; also counted by `LF_CrimeRecord` (#270) |
| `LF_OnMoneyMoved` | `m_sPlayerUid`, `m_iAmount` (positive), `m_sSource` (reason key), `m_sAccount` (`cash`/`bank`), `m_bCredit`, `m_iBalanceAfter`, `m_sCounterpartyUid` | Every cash and bank change, one event per account side: `LF_MoneyUtils.AddCash`/`RemoveCash` (player targets) and `LF_BankAccount.Deposit`/`Withdraw`/`Charge`. An ATM deposit is a cash debit and a bank credit. `LF_Ledger` subscribes (`docs/features.md`) |
| `LF_OnPlayerKilled` | `m_sVictimUid`, `m_sKillerUid` (empty if no player killer), `m_vPosition`, `m_sWeapon` (killer's held weapon prefab), `m_bMurder` (LF_Murder rule) | `LF_Murder.OnCharacterKilled`, player victims only, before the wanted decision |
| `LF_OnPlayerDamaged` | `m_sVictimUid`, `m_sAttackerUid`, `m_fDamage` (total in the window), `m_vPosition`, `m_sWeapon` | `LF_IncidentDamage.OnHit` from `SCR_CharacterDamageManagerComponent.HijackDamageHandling`: player-on-player only, at most one per victim/attacker pair per `m_fDamageSampleSeconds` (default 5), never per hit |
| `LF_OnArrested` | `m_sPlayerUid`, `m_iSentenceSeconds`, `m_iWantedLevel` | `LF_Jail` when a sentence starts |
| `LF_OnJobCompleted` | `m_sPlayerUid`, `m_iJob`, `m_iPay` | `LF_JobWorker` run completion (`LF_CivilianJob.CompleteRun`) |
| `LF_OnVehicleBought` | `m_sBuyerUid`, `m_sCatalogPrefab`, `m_iPrice` | `LF_VehicleOwnership` purchase |
| `LF_OnServiceCall` | `m_sCallerUid`, `m_sService`, `m_sPlace`, `m_sSummary`, `m_sChannels`, `m_vPosition`, `m_aReceiverIds` | `LF_ServiceCalls.Place`, once per call with at least one receiver on duty; `LF_Dispatch.Report`, once per crime report (police, even with nobody on duty; adds `m_sClothing`, `m_sVehicle`, `m_iOnDuty`). `LF_ServiceCallDispatch` (radio and dispatch log), `LF_ServiceBoard` (dispatch boards) and `LF_PagerSubscriber` (pagers) subscribe |
| `LF_OnPlayerDowned` | `m_sPlayerUid`, `m_sDownerUid` (empty if no player did it) | `LF_Downed.Down`: lethal damage to a player left them alive and unconscious. The death, and `LF_OnPlayerKilled`, come later |
| `LF_OnFirstAid` | `m_sPlayerUid`, `m_sHelperUid`, `m_sKind` (bandage, tourniquet, saline), `m_fSeconds` | `LF_FirstAid.Apply`: a vanilla medical item added bleed-out time to a downed player (#257) |
| `LF_OnServiceCallTaken` | `m_sCallerUid`, `m_sService`, `m_sTakerUid`, `m_sPlace`, `m_sSummary`, `m_iTakers` | `LF_ServiceTake.ServerTake`, after the server checked the taker is on duty for the service and the call had room (`m_iMaxTakers`). `LF_ServiceCallTakenNotifier` tells the caller; the board marks the call taken |
| `LF_OnFenceSale` | `m_sBuyerUid`, `m_sPrefab`, `m_iQuantity`, `m_iPrice` | `LF_ShopComponent.Buy` on a fence shop, after the cash was taken and the tool delivered |
| `LF_OnHoldUp` | `m_sActorUid`, `m_sTargetUid`, `m_vPosition` (the actor's) | `LF_HoldUpRole.Perform` via `LF_HoldUp.Attempt`, after the server checked weapon in hand, range and cooldown. `LF_IncidentLog` subscribes (kind `holdup`) |
| `LF_OnAdminAction` | `m_sAdminUid`, `m_sTargetUid` (may be empty), `m_sAction`, `m_vPosition` | `LF_AdminAction.Report` (server): every server-side Game Master editor action (`SCR_BaseActionsEditorComponent.ActionPerform`), vanilla `#kick`/`#ban`, `!wadd`, `!handled`. `LF_IncidentLog` subscribes (kind `admin`) |
| `LF_OnAimedAt` | `m_sActorUid`, `m_sTargetUid`, `m_vPosition` (the actor's) | `LF_AimIntent.Tick` (server timer) when a raised weapon stayed on a player for the aim window. `LF_IncidentLog` subscribes (kind `aim`) |

Adding an event: a class in `LF_Events.c`, a row here, a publish where the feature
already logs the change, and one line in `docs/features.md`.

## Trace

Every publish logs `[LFDebug:Events] <Event> published to N subscriber(s)` (or
`no subscribers`). A ticket left unpaid shows
`[LFDebug:FineTicket] ticket #N written` ... `[LFDebug:Events] LF_OnCrimeCommitted published`
then `[LFDebug:Wanted] crime report #N <offence> by <uid>`.
