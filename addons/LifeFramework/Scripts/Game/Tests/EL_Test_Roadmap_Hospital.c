// red-proof: expected wanted 0 after RobTill and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - till rob raises wanted once (expected 0, got 1)`.
// Restored the expected value to 1.

// tier: LOGIC
class EL_Test_Roadmap_Hospital : EL_Test
{
	override string GetName()
	{
		return "roadmap/hospital";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.EqualStr("{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et", LF_Hospital.CashPrefab(), "till pays MoneyStack");
		ctx.False(LF_Hospital.CooldownActive(-1, 1000, 60), "a till that has not been robbed is not on cooldown");
		ctx.True(LF_Hospital.CooldownActive(1000, 1000, 60), "the same timestamp is still inside a 60 second cooldown");
		ctx.True(LF_Hospital.CooldownActive(1000, 1059, 60), "59 seconds is still inside a 60 second cooldown");
		ctx.False(LF_Hospital.CooldownActive(1000, 1060, 60), "60 seconds clears a 60 second cooldown");
		ctx.False(LF_Hospital.CooldownActive(1000, 1000, 0), "a zero cooldown never blocks");

		string tillId = "logic-hospital-till";
		string policeTill = "logic-hospital-till-police";
		string factionTill = "logic-hospital-till-faction";
		string uid = "roadmap-hospital-logic";
		string officerUid = "roadmap-hospital-officer";

		EL_BankAccount bank = EL_ATMManager.GetInstance().CreateAccount(uid);
		ctx.NotNull(bank, "robber bank account exists");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(0, bank.GetBalance(), "bank starts at 0");

		LF_Hospital.ConfigureTill(policeTill, LF_Hospital.DEFAULT_PAYOUT, 60, 1);
		ctx.False(LF_Hospital.CanRobTill(policeTill, uid), "minimum police gate refuses when nobody is on duty");
		ctx.False(LF_Hospital.RobTill(policeTill, uid, null), "till rob refuses below the police minimum");
		ctx.Equal(0, LF_Hospital.GetTillAlertCount(policeTill), "a refused till rob does not alert");
		ctx.Equal(0, bank.GetBalance(), "a refused till rob does not touch the bank");

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		LF_Hospital.ConfigureTill(factionTill, LF_Hospital.DEFAULT_PAYOUT, 60, 0);
		ctx.False(LF_Hospital.CanRobTill(factionTill, officerUid), "police faction cannot rob the till");
		ctx.False(LF_Hospital.RobTill(factionTill, officerUid, null), "till rob refuses a non-civilian");
		ctx.Equal(0, officer.GetWantedLevel(), "a refused civilian gate does not raise wanted");
		ctx.Equal(0, LF_Hospital.GetTillAlertCount(factionTill), "a refused civilian gate does not alert");

		LF_Hospital.ConfigureTill(tillId, LF_Hospital.DEFAULT_PAYOUT, 60, 0);
		ctx.Equal(LF_Hospital.DEFAULT_PAYOUT, LF_Hospital.GetConfiguredPayout(tillId), "till payout is the configured cash amount");
		ctx.True(LF_Hospital.CanRobTill(tillId, uid), "civilian can rob a configured till");
		ctx.True(LF_Hospital.CanRobTill(tillId, uid), "offer refresh can be asked again");
		ctx.Equal(0, LF_Hospital.GetTillAlertCount(tillId), "offer refresh does not alert");

		EL_PlayerAccount robber = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.Equal(0, robber.GetWantedLevel(), "robber starts clean");
		ctx.True(LF_Hospital.RobTill(tillId, uid, null), "till rob succeeds");
		ctx.Equal(LF_Hospital.DEFAULT_PAYOUT, LF_Hospital.GetCashDelivered(tillId), "till delivered the MoneyStack payout");
		ctx.Equal(0, bank.GetBalance(), "till does not touch the bank");
		ctx.Equal(1, robber.GetWantedLevel(), "till rob raises wanted once");
		ctx.Equal(1, LF_Hospital.GetTillAlertCount(tillId), "till alerts once");
		ctx.False(LF_Hospital.CanRobTill(tillId, uid), "till is on cooldown after the rob");
		ctx.False(LF_Hospital.RobTill(tillId, uid, null), "second till rob inside the cooldown refuses");
		ctx.Equal(1, LF_Hospital.GetTillAlertCount(tillId), "cooldown refusal does not alert again");
		ctx.Equal(1, robber.GetWantedLevel(), "cooldown refusal does not raise wanted");
		ctx.Equal(0, bank.GetBalance(), "cooldown refusal does not touch the bank");

		array<ResourceName> configured = {};
		LF_Hospital.FillDefaultLoot(configured);
		array<ResourceName> resolved = {};
		ctx.Equal(LF_Hospital.DefaultLootCount(), LF_Hospital.ResolveLoot(configured, resolved), "cabinet loot count is the config list");
		ctx.Equal(4, resolved.Count(), "default cabinet loot is four prefabs");
		if (resolved.Count() == 4)
		{
			ctx.True(resolved.Get(0) == LF_Hospital.LOOT_FIELD_DRESSING, "loot line 0 is field dressing");
			ctx.True(resolved.Get(1) == LF_Hospital.LOOT_MORPHINE, "loot line 1 is morphine");
			ctx.True(resolved.Get(2) == LF_Hospital.LOOT_SALINE, "loot line 2 is saline");
			ctx.True(resolved.Get(3) == LF_Hospital.LOOT_TOURNIQUET, "loot line 3 is tourniquet");
		}

		ResourceName missing = "{00000000DEADBEEF}Prefabs/Items/Medicine/NotARealHospitalItem.et";
		array<ResourceName> withGap = {};
		withGap.Insert(LF_Hospital.LOOT_FIELD_DRESSING);
		withGap.Insert(missing);
		withGap.Insert(LF_Hospital.LOOT_MORPHINE);
		array<ResourceName> skipped = {};
		ctx.Equal(2, LF_Hospital.ResolveLoot(withGap, skipped), "a missing prefab is skipped");
		if (skipped.Count() == 2)
		{
			ctx.True(skipped.Get(0) == LF_Hospital.LOOT_FIELD_DRESSING, "the line before the gap remains");
			ctx.True(skipped.Get(1) == LF_Hospital.LOOT_MORPHINE, "the line after the gap remains");
		}
		bool gapPresent = false;
		for (int i = 0; i < skipped.Count(); i++)
		{
			if (skipped.Get(i) == missing)
				gapPresent = true;
		}
		ctx.False(gapPresent, "the missing prefab is not in the resolved list");

		string cabinetId = "logic-hospital-cabinet";
		string cabinetUid = "roadmap-hospital-cabinet";
		EL_BankAccount cabinetBank = EL_ATMManager.GetInstance().CreateAccount(cabinetUid);
		ctx.NotNull(cabinetBank, "cabinet robber bank account exists");
		if (ctx.FailureCount() > 0)
			return;

		LF_Breach.Configure(cabinetId, 0, "");
		array<ResourceName> cabinetLoot = {};
		LF_Hospital.FillDefaultLoot(cabinetLoot);
		LF_Hospital.ConfigureCabinet(cabinetId, cabinetLoot);

		ctx.True(LF_Hospital.CanBreach(cabinetId), "configured cabinet can be breached");
		ctx.True(LF_Hospital.CanBreach(cabinetId), "cabinet offer refresh can be asked again");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount(cabinetId), "cabinet offer refresh does not alert");

		EL_PlayerAccount cabinetRobber = EL_PlayerAccountManager.GetOrCreate(cabinetUid);
		ctx.True(LF_Hospital.BreachCabinet(cabinetId, cabinetUid), "cabinet breach arms the shared breach");
		ctx.False(LF_Breach.IsLocked(cabinetId), "zero fuse opens the cabinet");
		ctx.Equal(1, cabinetRobber.GetWantedLevel(), "cabinet breach raises wanted once");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(cabinetId), "cabinet alerts once");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(cabinetId), "cabinet yields the four medicine prefabs");
		ctx.True(LF_Hospital.GetDeliveredPrefab(cabinetId, 0) == LF_Hospital.LOOT_FIELD_DRESSING, "delivered line 0 is field dressing");
		ctx.True(LF_Hospital.GetDeliveredPrefab(cabinetId, 1) == LF_Hospital.LOOT_MORPHINE, "delivered line 1 is morphine");
		ctx.True(LF_Hospital.GetDeliveredPrefab(cabinetId, 2) == LF_Hospital.LOOT_SALINE, "delivered line 2 is saline");
		ctx.True(LF_Hospital.GetDeliveredPrefab(cabinetId, 3) == LF_Hospital.LOOT_TOURNIQUET, "delivered line 3 is tourniquet");
		ctx.Equal(0, cabinetBank.GetBalance(), "cabinet breach does not touch the bank");

		ctx.False(LF_Hospital.CanBreach(cabinetId), "open cabinet is not offered again");
		ctx.False(LF_Hospital.BreachCabinet(cabinetId, cabinetUid), "second breach inside the open state refuses");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(cabinetId), "second breach spawns nothing");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(cabinetId), "second breach does not alert again");
		ctx.Equal(1, cabinetRobber.GetWantedLevel(), "second breach does not raise wanted");
	}
}

// tier: WORLD
class EL_Test_Roadmap_HospitalWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-hospital";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource cash = Resource.Load(LF_Hospital.CashPrefab());
		ctx.True(cash != null && cash.IsValid(), "MoneyStack.et loads");
		for (int i = 0; i < LF_Hospital.DefaultLootCount(); i++)
		{
			ResourceName prefab = LF_Hospital.DefaultLootPrefab(i);
			Resource medicine = Resource.Load(prefab);
			ctx.True(medicine != null && medicine.IsValid(), "medicine prefab loads: " + prefab);
		}

		IEntity till = GetGame().GetWorld().FindEntityByName(LF_Hospital.TILL_ID);
		ctx.True(till != null, "Roadmap_Hospital_Till is placed");
		IEntity cabinet = GetGame().GetWorld().FindEntityByName(LF_Hospital.CABINET_ID);
		ctx.True(cabinet != null, "Roadmap_Hospital_Cabinet is placed");
		if (ctx.FailureCount() > 0)
			return;

		ctx.NotNull(EL_Component<LF_HospitalTillComponent>.Find(till), "till carries LF_HospitalTillComponent");
		ctx.NotNull(EL_Component<LF_HospitalCabinetComponent>.Find(cabinet), "cabinet carries LF_HospitalCabinetComponent");
		ctx.NotNull(EL_Component<LF_BreachTargetComponent>.Find(cabinet), "cabinet is a breach target");

		EL_LockComponent cabinetLock = EL_Component<EL_LockComponent>.Find(cabinet);
		ctx.NotNull(cabinetLock, "cabinet carries EL_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(HasRobAction(till), "till uses EL_RobAction");
		ctx.True(HasCabinetAction(cabinet), "cabinet action is on the cabinet");
		ctx.True(cabinetLock.IsLocked(), "cabinet is locked before the breach");

		string uid = "roadmap-hospital-e2e";
		EL_BankAccount bank = EL_ATMManager.GetInstance().CreateAccount(uid);
		ctx.NotNull(bank, "e2e bank account exists");
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "e2e robber account exists");
		if (ctx.FailureCount() > 0)
			return;

		int alertsBefore = LF_Hospital.GetTillAlertCount(LF_Hospital.TILL_ID);
		ctx.True(LF_Hospital.CanRobTill(LF_Hospital.TILL_ID, uid), "placed till can be robbed");
		ctx.Equal(alertsBefore, LF_Hospital.GetTillAlertCount(LF_Hospital.TILL_ID), "placed till offer refresh does not alert");
		ctx.True(LF_Hospital.RobTill(LF_Hospital.TILL_ID, uid, null), "placed till pays once");
		ctx.False(LF_Hospital.RobTill(LF_Hospital.TILL_ID, uid, null), "placed till refuses a second rob inside the cooldown");
		ctx.Equal(0, bank.GetBalance(), "placed till does not touch the bank");
		ctx.Equal(alertsBefore + 1, LF_Hospital.GetTillAlertCount(LF_Hospital.TILL_ID), "placed till alerts once");

		int cabinetAlerts = LF_Breach.GetPoliceNotifyCount(LF_Hospital.CABINET_ID);
		ctx.True(LF_Hospital.CanBreach(LF_Hospital.CABINET_ID), "placed cabinet can be breached");
		ctx.Equal(cabinetAlerts, LF_Breach.GetPoliceNotifyCount(LF_Hospital.CABINET_ID), "placed cabinet offer refresh does not alert");
		ctx.True(LF_Hospital.BreachCabinet(LF_Hospital.CABINET_ID, uid), "placed cabinet breach opens it");
		ctx.False(cabinetLock.IsLocked(), "placed cabinet IsLocked() is false after the zero fuse");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(LF_Hospital.CABINET_ID), "placed cabinet yields four medicine prefabs");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_Hospital.CABINET_ID, 0) == LF_Hospital.LOOT_FIELD_DRESSING, "placed loot 0 is field dressing");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_Hospital.CABINET_ID, 1) == LF_Hospital.LOOT_MORPHINE, "placed loot 1 is morphine");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_Hospital.CABINET_ID, 2) == LF_Hospital.LOOT_SALINE, "placed loot 2 is saline");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_Hospital.CABINET_ID, 3) == LF_Hospital.LOOT_TOURNIQUET, "placed loot 3 is tourniquet");
		ctx.False(LF_Hospital.BreachCabinet(LF_Hospital.CABINET_ID, uid), "placed cabinet refuses a second breach");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(LF_Hospital.CABINET_ID), "placed cabinet does not spawn a second set");
		ctx.Equal(cabinetAlerts + 1, LF_Breach.GetPoliceNotifyCount(LF_Hospital.CABINET_ID), "placed cabinet alerts once");
		ctx.Equal(0, bank.GetBalance(), "placed cabinet does not touch the bank");
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasRobAction(IEntity entity)
	{
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (EL_RobAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasCabinetAction(IEntity entity)
	{
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (LF_HospitalCabinetAction.Cast(action))
				return true;
		}

		return false;
	}
}
