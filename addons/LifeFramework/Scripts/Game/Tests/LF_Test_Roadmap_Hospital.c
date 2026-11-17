// red-proof: #2419 removed the dead till script (LF_Hospital.RobTill and helpers) and its assertions here; the cabinet
// assertions are unchanged, and their original observed red run is in the git history of this file (2026-10-11).
// tier: LOGIC
class LF_Test_Roadmap_Hospital : LF_Test
{
	override string GetName()
	{
		return "roadmap/hospital";
	}

	override void Run(LF_TestContext ctx)
	{
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
		LF_BankAccount cabinetBank = LF_ATMManager.GetInstance().CreateAccount(cabinetUid);
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

		LF_PlayerAccount cabinetRobber = LF_PlayerAccountManager.GetOrCreate(cabinetUid);
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
class LF_Test_Roadmap_HospitalWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/roadmap-hospital";
	}

	override void Run(LF_TestContext ctx)
	{
		for (int i = 0; i < LF_Hospital.DefaultLootCount(); i++)
		{
			ResourceName prefab = LF_Hospital.DefaultLootPrefab(i);
			Resource medicine = Resource.Load(prefab);
			ctx.True(medicine != null && medicine.IsValid(), "medicine prefab loads: " + prefab);
		}

		IEntity till = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_TILL);
		ctx.True(till != null, "Annex_Hospital_Till is placed");
		IEntity cabinet = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_CABINET);
		ctx.True(cabinet != null, "Annex_Hospital_Cabinet is placed");
		if (ctx.FailureCount() > 0)
			return;

		ctx.NotNull(LF_Component<LF_HospitalTillComponent>.Find(till), "till carries LF_HospitalTillComponent");
		ctx.NotNull(LF_Component<LF_HospitalCabinetComponent>.Find(cabinet), "cabinet carries LF_HospitalCabinetComponent");
		ctx.NotNull(LF_Component<LF_BreachTargetComponent>.Find(cabinet), "cabinet is a breach target");

		LF_LockComponent cabinetLock = LF_Component<LF_LockComponent>.Find(cabinet);
		ctx.NotNull(cabinetLock, "cabinet carries LF_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(HasRobAction(till), "till uses LF_RobAction");
		ctx.True(HasCabinetAction(cabinet), "cabinet action is on the cabinet");
		ctx.True(cabinetLock.IsLocked(), "cabinet is locked before the breach");

		string uid = "roadmap-hospital-e2e";
		LF_BankAccount bank = LF_ATMManager.GetInstance().CreateAccount(uid);
		ctx.NotNull(bank, "e2e bank account exists");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "e2e robber account exists");
		if (ctx.FailureCount() > 0)
			return;

		int cabinetAlerts = LF_Breach.GetPoliceNotifyCount(LF_TestAnnex.HOSPITAL_CABINET);
		ctx.True(LF_Hospital.CanBreach(LF_TestAnnex.HOSPITAL_CABINET), "placed cabinet can be breached");
		ctx.Equal(cabinetAlerts, LF_Breach.GetPoliceNotifyCount(LF_TestAnnex.HOSPITAL_CABINET), "placed cabinet offer refresh does not alert");
		ctx.True(LF_Hospital.BreachCabinet(LF_TestAnnex.HOSPITAL_CABINET, uid), "placed cabinet breach opens it");
		ctx.False(cabinetLock.IsLocked(), "placed cabinet IsLocked() is false after the zero fuse");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(LF_TestAnnex.HOSPITAL_CABINET), "placed cabinet yields four medicine prefabs");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_TestAnnex.HOSPITAL_CABINET, 0) == LF_Hospital.LOOT_FIELD_DRESSING, "placed loot 0 is field dressing");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_TestAnnex.HOSPITAL_CABINET, 1) == LF_Hospital.LOOT_MORPHINE, "placed loot 1 is morphine");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_TestAnnex.HOSPITAL_CABINET, 2) == LF_Hospital.LOOT_SALINE, "placed loot 2 is saline");
		ctx.True(LF_Hospital.GetDeliveredPrefab(LF_TestAnnex.HOSPITAL_CABINET, 3) == LF_Hospital.LOOT_TOURNIQUET, "placed loot 3 is tourniquet");
		ctx.False(LF_Hospital.BreachCabinet(LF_TestAnnex.HOSPITAL_CABINET, uid), "placed cabinet refuses a second breach");
		ctx.Equal(4, LF_Hospital.GetDeliveredCount(LF_TestAnnex.HOSPITAL_CABINET), "placed cabinet does not spawn a second set");
		ctx.Equal(cabinetAlerts + 1, LF_Breach.GetPoliceNotifyCount(LF_TestAnnex.HOSPITAL_CABINET), "placed cabinet alerts once");
		ctx.Equal(0, bank.GetBalance(), "placed cabinet does not touch the bank");
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasRobAction(IEntity entity)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (LF_RobAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasCabinetAction(IEntity entity)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(entity);
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
