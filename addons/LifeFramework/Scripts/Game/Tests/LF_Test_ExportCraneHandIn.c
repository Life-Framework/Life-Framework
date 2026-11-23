// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_ExportCraneHandIn`: RED PROOF OK; RedProof() skips the hand-in action, so "the full hand-in pays $920 (expected 920, got 0)" and "the full hand-in takes every bar (expected 0, got 20)" fail.
//! Export order hand-in at the crane spot (#4580): the spot prefab offers "Hand in the export order";
//! a short count takes nothing and pays nothing; the full count leaves the hauler's pockets and pays
//! count x wholesale + 15% once; a second use pays nothing; a hauler away from the spot is refused and
//! keeps the goods.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ExportCraneHandIn : LF_Test
{
	protected static const ResourceName SPOT = "{897C2A6F02D75762}Prefabs/Props/LF_ExportCraneSpot.et";
	protected static const ResourceName GOODS = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const string HAULER = "test-export-crane-hauler";
	protected static const int COUNT = 20;
	protected static const int PAY = 920;	// 20 x $40 + 15%

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/export-crane-hand-in";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderConfig MakeConfig()
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		LF_ExportOrderTemplate iron = new LF_ExportOrderTemplate();
		iron.m_sId = "iron";
		iron.m_Goods = GOODS;
		iron.m_sGoodsName = "iron bars";
		iron.m_sDestination = "the steamer";
		iron.m_iCount = COUNT;
		iron.m_iWholesale = 40;
		iron.m_iGameMinutes = 1200;
		config.m_aTemplates.Insert(iron);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_Reputation.Forget(HAULER);
		LF_JobLevels.Forget(HAULER);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportCraneHandInAction FindAction(IEntity spot)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(spot);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ExportCraneHandInAction handIn = LF_ExportCraneHandInAction.Cast(base);
			if (handIn)
				return handIn;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the first open order for the hauler at the gate level; returns its id or -1.
	protected int TakeOrder()
	{
		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		foreach (LF_ExportOrder order : LF_ExportOrderBook.List())
		{
			if (!order || !order.m_sTakerUid.IsEmpty())
				continue;

			if (LF_ExportOrderBook.Take(HAULER, config.m_iMinLevel, order.m_iId).IsEmpty())
				return order.m_iId;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void Use(LF_ExportCraneHandInAction action, IEntity spot, IEntity hauler)
	{
		if (!RedProof())
			action.PerformAction(spot, hauler);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ExportOrderConfig.UseForTest(MakeConfig());

		IEntity hauler = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(hauler, "the hauler spawns");
		IEntity spot;
		if (hauler)
		{
			LF_Utils.SetUidForTests(hauler, HAULER);
			spot = LF_Utils.SpawnEntityPrefab(SPOT, hauler.GetOrigin() + Vector(2, 0, 0));
			ctx.NotNull(spot, "the crane spot spawns");
		}

		if (hauler && spot)
			RunChecks(ctx, hauler, spot);

		LF_InventoryUtils.RemoveAmount(LF_Component<InventoryStorageManagerComponent>.Find(hauler), GOODS, 1000);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(hauler);
		if (spot)
			SCR_EntityHelper.DeleteEntityAndChildren(spot);

		Clean();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity hauler, IEntity spot)
	{
		ctx.True(spot.FindComponent(LF_ExportOrderCraneComponent) != null, "the spot carries the crane component");
		LF_ExportCraneHandInAction action = FindAction(spot);
		ctx.NotNull(action, "the spot offers the hand-in row");
		if (!action)
			return;

		ctx.True(action.CanBeShownScript(hauler), "the hand-in row is shown");
		string name;
		action.GetActionNameScript(name);
		ctx.EqualStr("#LF-ExportCrane_HandIn", name, "the row reads Hand in the export order");

		int orderId = TakeOrder();
		ctx.True(orderId >= 0, "fixture: the hauler holds an order");
		if (orderId < 0)
			return;

		InventoryStorageManagerComponent pockets = LF_Component<InventoryStorageManagerComponent>.Find(hauler);
		int cashBefore = LF_MoneyUtils.GetCash(hauler);

		// Short: 19 of 20. Nothing leaves the pockets, nothing is paid, the order stays.
		ctx.Equal(COUNT - 1, LF_InventoryUtils.AddAmount(pockets, GOODS, COUNT - 1, false), "fixture: 19 bars in the pockets");
		action.PerformAction(spot, hauler);
		ctx.Equal(COUNT - 1, LF_InventoryUtils.GetAmount(pockets, GOODS), "a short hand-in takes no bar");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(hauler), "a short hand-in pays nothing");
		ctx.NotNull(LF_ExportOrderBook.HeldBy(HAULER), "a short hand-in keeps the order");

		// Away from the spot: refused, the goods stay.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(pockets, GOODS, 1, false), "fixture: the 20th bar");
		vector home = spot.GetOrigin();
		spot.SetOrigin(home + Vector(LF_ExportCraneHandIn.REACH_M + 4, 0, 0));
		spot.Update();
		int farPaid;
		LF_ExportCraneHandIn.Serve(hauler, spot, farPaid);
		ctx.Equal(0, farPaid, "a hauler away from the spot is paid nothing");
		ctx.Equal(COUNT, LF_InventoryUtils.GetAmount(pockets, GOODS), "a hauler away from the spot keeps the bars");
		spot.SetOrigin(home);
		spot.Update();

		// Full count: the bars go, $920 lands, the order closes.
		Use(action, spot, hauler);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(pockets, GOODS), "the full hand-in takes every bar");
		ctx.Equal(cashBefore + PAY, LF_MoneyUtils.GetCash(hauler), "the full hand-in pays $920");
		ctx.True(LF_ExportOrderBook.HeldBy(HAULER) == null, "the handed-in order is closed");

		// Again: nothing held, nothing paid.
		ctx.Equal(COUNT, LF_InventoryUtils.AddAmount(pockets, GOODS, COUNT, false), "fixture: another 20 bars");
		Use(action, spot, hauler);
		ctx.Equal(cashBefore + PAY, LF_MoneyUtils.GetCash(hauler), "a second hand-in pays nothing");
		ctx.Equal(COUNT, LF_InventoryUtils.GetAmount(pockets, GOODS), "a second hand-in takes no bar");
	}
}
