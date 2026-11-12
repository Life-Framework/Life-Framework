// red-proof: observed 2026-11-09 with `toolsi test --no-build --only LF_Test_MedicDutySwap`: LF_ServiceCalls.JobFor never returning MEDIC failed it with `- routing sees the on-duty medic as the medic job (expected 4, got 0)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! medic/duty-swap (#258): the hospital desk clocks a player in as a medic. Clock-in stores the
//! civilian inventory and issues the medic kit with a pager; the routing then sees them as the
//! medic job; clock-out returns the civilian inventory, stores the medic one in its own slot and
//! pays duty pay. The police desk keeps working (police/duty-swap).
// tier: WORLD
class LF_Test_MedicDutySwap : LF_Test
{
	protected static const string UID = "medic-duty-swap";
	protected static const ResourceName DRESSING_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";

	override string GetName()
	{
		return "medic/duty-swap";
	}

	override void Run(LF_TestContext ctx)
	{
		// The medic role is open: no roster entry needed.
		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, false);
		string characterId = account.GetActiveCharacter().GetId();
		ctx.EqualStr("", LF_DutyLocker.RefuseClockIn(account, LF_Faction.MEDIC), "a player off the police roster may clock in as a medic");
		ctx.EqualStr("#LF-Duty_NotOnRoster", LF_DutyLocker.RefuseClockIn(account, LF_Faction.POLICE), "the same player still cannot clock in as police");

		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body || ctx.FailureCount() > 0)
		{
			Release(characterId);
			LF_TestCharacterFixture.Delete(body);
			return;
		}

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.MEDIC), "the player clocks in as a medic");
		ctx.True(account.IsOnDuty(), "on duty after clock-in");
		ctx.Equal(LF_Faction.MEDIC, account.GetFaction(), "medic while clocked in");
		ctx.True(LF_MedicDuty.IsOnDuty(account), "the account reads as an on-duty medic");
		ctx.True(LF_MedicDuty.IsOnDutyState(LF_AccountStateBits.FromAccount(account)), "a client reads the on-duty medic from the replicated state");
		ctx.True(LF_TestCharacterFixture.HasItem(body, DRESSING_PREFAB), "the medic kit carries a field dressing");
		ctx.True(LF_TestCharacterFixture.HasItem(body, LF_Test_Pager.PAGER_PREFAB), "the medic kit carries a pager");
		ctx.False(LF_TestCharacterFixture.HasItem(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "no civilian apple stays on the medic");
		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_CIVILIAN);
		ctx.NotNull(civilian, "the civilian inventory is stored at the desk");
		if (civilian)
			ctx.Equal(3, civilian.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "the stored civilian inventory holds the 3 apples");
		ctx.EqualStr("#LF-Duty_WrongDesk", LF_DutyLocker.RefuseClockOut(account, LF_Faction.POLICE), "the police desk does not clock a medic out");

		// Routing: the on-duty medic is the medic job and receives medic calls; off duty they do not.
		int job = LF_ServiceCalls.JobFor(account, LF_EJobType.UNEMPLOYED);
		ctx.Equal(LF_EJobType.MEDIC, job, "routing sees the on-duty medic as the medic job");
		array<ref LF_ServiceCandidate> players = {};
		players.Insert(LF_ServiceCandidate.Make(1, false, job));
		players.Insert(LF_ServiceCandidate.Make(2, false, LF_EJobType.UNEMPLOYED));
		LF_ServiceRoute route = LF_ServiceCalls.Check("medic", players);
		ctx.Equal(1, route.m_aReceivers.Count(), "one player receives a medic call");
		if (route.m_aReceivers.Count() == 1)
			ctx.Equal(1, route.m_aReceivers[0], "the clocked-in medic receives it");

		// The shift's seconds are pay, not police rank.
		LF_PoliceRanks.OnDutyChangedAt(account, true, 1000);
		LF_PoliceRanks.SettleDutyAt(account, false, 101000);
		ctx.Equal(100, account.GetUnpaidDutySeconds(), "100 on-duty seconds are owed as medic pay");
		ctx.True(LF_DutyPay.RateFor(LF_Faction.MEDIC) > 0, "the flagship config pays medics");
		int owed = LF_DutyPay.Owed(100, LF_DutyPay.RateFor(LF_Faction.MEDIC));
		ctx.Equal(owed, LF_DutyPay.TakeShiftPay(account, LF_DutyPay.RateFor(LF_Faction.MEDIC)), "a medic is paid the shift at the medic rate");
		ctx.Equal(0, account.GetUnpaidDutySeconds(), "the paid seconds are spent");

		// On shift: pick up 2 apples, then clock out.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB, 2), "the medic picks up 2 apples");
		ctx.EqualStr("", LF_DutyLocker.ClockOut(account, body, LF_Faction.MEDIC), "the medic clocks out");
		ctx.False(account.IsOnDuty(), "off duty after clock-out");
		ctx.Equal(LF_Faction.CIVILIAN, account.GetFaction(), "civilian after clock-out");
		ctx.False(LF_MedicDuty.IsOnDuty(account), "no longer an on-duty medic");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_ServiceCalls.JobFor(account, LF_EJobType.UNEMPLOYED), "routing no longer sees the medic job");
		ctx.False(LF_TestCharacterFixture.HasItem(body, DRESSING_PREFAB), "the medic kit does not leave with the civilian");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "the civilian's 3 apples are back, the medic 2 are not");
		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_MEDIC);
		ctx.NotNull(stored, "the medic inventory is stored in its own slot");
		if (stored)
			ctx.Equal(2, stored.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "the stored medic inventory holds the 2 apples");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_POLICE), "nothing lands in the police slot");

		// Clock in again: the same medic inventory.
		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.MEDIC), "the medic clocks in again");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "the medic inventory's 2 apples are back");
		LF_DutyLocker.ClockOut(account, body, LF_Faction.MEDIC);

		Release(characterId);
		LF_TestCharacterFixture.Delete(body);
	}

	protected void Release(string characterId)
	{
		LF_StorageSnapshot.Clear(characterId, LF_DutyLocker.SLOT_MEDIC);
		LF_TestPoliceKit.Release(UID);
	}
}

//------------------------------------------------------------------------------------------------
//! medic/duty-desk (#258): the hospital desk is placed in the playtest town and its punch clock
//! carries the clock-in action for the medic role.
// tier: WORLD
class LF_Test_MedicDutyDesk : LF_Test
{
	override string GetName()
	{
		return "medic/duty-desk";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_DUTY_DESK);
		ctx.NotNull(desk, "Annex_Hospital_Duty_Desk is placed");
		if (!desk)
			return;

		IEntity clock = LF_ActionSite.FindNear(desk.GetOrigin(), 1.5, "LF_DutyAction");
		ctx.NotNull(clock, "the desk's punch clock carries the duty action");
		if (!clock)
			return;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(clock.FindComponent(BaseActionsManagerComponent));
		ctx.NotNull(manager, "the punch clock has an actions manager");
		if (!manager)
			return;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		LF_DutyAction duty;
		foreach (BaseUserAction action : actions)
		{
			duty = LF_DutyAction.Cast(action);
			if (duty)
				break;
		}

		ctx.NotNull(duty, "the clock carries LF_DutyAction");
		if (duty)
			ctx.Equal(LF_Faction.MEDIC, duty.GetRole(), "the hospital desk clocks in the medic role");
	}
}
