// red-proof: 2026-11-23 run against the unchanged pack row (0 police, 60 s) failed "rung 2: one officer on duty (expected 1, got 0)", "rung 2: a 45 minute cooldown (expected 2700, got 60)", "nobody on duty: the rule refuses" and "with nobody on duty the box refuses a robbery"; the batched `tools\cli test --no-build --red LF_Test_EveronStoreBoxSafe` printed RED PROOF OK (RedProof() skips the pack overlay and the robbery acts). Green with the row set (pack leg).
//! crime/store-box (#2331, #4435): a shop till has a `store_box` safe beside it, rung 2 of the robbery
//! ladder (docs/roadmap/next-release-0.3.0.md "Robbery police minimums"). Under the Everon 1989 header (the
//! pack leg) the pack's `store_box` row says one officer on duty, a 45 minute cooldown, one bag of
//! 120 cash, no tool and a 10 s fuse, and the store alarm. The grid cell `store-box` places a till and a
//! LF_StoreBox next to it: a robbery with nobody on duty is refused, then (the officer's count met) the
//! fuse runs out and exactly one haul spills, and a second try inside the cooldown is refused. The
//! island box (kit `general-store`) holds one haul and refills it each 45 minutes (STOCK, REFILL_SECONDS),
//! so a shop cannot be farmed; the test runs that stock on the grid box.
//! Lives in the pack because it names a pack prefab and the pack's overlay (the card's core test
//! path cannot: validate-addon-boundary).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronStoreBoxSafe : LF_Test
{
	static const ResourceName STORE_BOX = "{FDF16EF9B78A92F7}Prefabs/Props/Safes/LF_StoreBox.et";
	static const string TILL_NAME = "Grid_store_box_R0_1";
	static const string BOX_NAME = "Grid_store_box_R0_2";
	static const float BESIDE_METRES = 2;
	static const int PACK_HAUL = 120;
	static const float STORE_FUSE = 10;
	static const int POLICE_NEEDED = 1;
	static const float COOLDOWN_SECONDS = 2700;	// lf-fixed: the plan's 45 min rung-2 cooldown
	static const int STOCK = 1;	// lf-fixed: the hauls the island box holds (kit general-store, safe row)
	static const float REFILL_SECONDS = 2700;	// lf-fixed: one haul back per 45 min (kit general-store, safe row)

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/store-box-safe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_Test_SafeTypes.SafeTypeOf(STORE_BOX) == LF_SafeTypeConfig.TYPE_STORE_BOX, "the pack store box prefab names the store_box type");

		LF_SafeTypeRow row = LayeredRow(ctx);
		ctx.NotNull(row, "the store_box row survives the pack overlay");
		if (!row)
			return;

		ctx.Equal(POLICE_NEEDED, row.m_iMinPoliceOnDuty, "rung 2: one officer on duty");
		ctx.EqualFloat(COOLDOWN_SECONDS, row.m_fCooldownSeconds, 0.001, "rung 2: a 45 minute cooldown");
		ctx.Equal(1, row.m_iBagCount, "one bag");
		ctx.Equal(PACK_HAUL, row.m_iCashPerBag, "the pack's 1989 takings: 120 a bag");
		ctx.EqualFloat(STORE_FUSE, row.m_fFuseSeconds, 0.001, "the box cracks on a 10 s fuse");
		ctx.True(row.m_sAlarmKind == "store", "the store alarm calls the police");
		ctx.True(row.m_sRequiredTools.IsEmpty(), "no fence tool is needed");
		ctx.True(!LF_Bank.PoliceOnDutyAllows(0, row.m_iMinPoliceOnDuty), "nobody on duty: the rule refuses");
		ctx.True(LF_Bank.PoliceOnDutyAllows(1, row.m_iMinPoliceOnDuty), "one officer on duty: the rule allows");

		IEntity till = GetGame().GetWorld().FindEntityByName(TILL_NAME);
		IEntity box = GetGame().GetWorld().FindEntityByName(BOX_NAME);
		ctx.NotNull(till, "the store-box grid cell places a till");
		ctx.NotNull(box, "the store-box grid cell places the store box");
		if (!till || !box)
			return;

		ctx.True(vector.Distance(till.GetOrigin(), box.GetOrigin()) < BESIDE_METRES, "the box stands beside the till");
		RunBox(ctx, box, row);
	}

	//------------------------------------------------------------------------------------------------
	//! The core table with the running header's pack overlays over it. The red proof leaves the overlay off.
	protected LF_SafeTypeRow LayeredRow(LF_TestContext ctx)
	{
		Resource container = BaseContainerTools.LoadContainer(LF_SafeTypeConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			ctx.Fail("the core safe type table does not load");
			return null;
		}

		LF_SafeTypeConfig table = LF_SafeTypeConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		ctx.NotNull(table, "the core table reads");
		if (!table)
			return null;

		if (!RedProof())
			table.ApplyLayers();

		return table.Find(LF_SafeTypeConfig.TYPE_STORE_BOX);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBox(LF_TestContext ctx, IEntity box, LF_SafeTypeRow row)
	{
		ctx.NotNull(LF_Component<LF_LockComponent>.Find(box), "the box has the lock a breach and a robbery need");
		LF_HeistVaultComponent vault = LF_Component<LF_HeistVaultComponent>.Find(box);
		ctx.NotNull(vault, "the box is a heist vault target");
		if (!vault)
			return;

		string vaultId = vault.GetVaultId();
		ctx.True(vaultId == BOX_NAME, "the box is registered under its entity name");
		ctx.True(vault.GetActionName() == "#LF-Safe_BoxRob", "the type gives it its own action name");

		// What the type says it holds and how it cracks.
		ctx.Equal(1, LF_Bank.GetConfiguredBagCount(vaultId), "it holds one bag");
		ctx.Equal(PACK_HAUL, LF_Bank.GetCashPerBag(vaultId), "the pack's 1989 takings: 120 a bag");
		ctx.EqualFloat(10, LF_Bank.GetDrillSeconds(vaultId), 0.001, "the drill time is the type's");

		// No officer on duty in this run (no player is an officer): the box refuses, whatever the tool.
		string robber = "store-box-test-robber";
		bool refused = false;
		if (!RedProof())
			refused = !LF_Bank.Rob(vaultId, robber);

		ctx.True(refused, "with nobody on duty the box refuses a robbery");
		ctx.Equal(0, LF_Bank.GetLooseBagCount(vaultId), "a refused robbery spills nothing");

		// One officer on duty: the same box, its numbers from the row, the police gate met.
		LF_Bank.Configure(vaultId, row.m_iBagCount, row.m_iCashPerBag, row.m_fCooldownSeconds, 0, row.m_fFuseSeconds);
		LF_Bank.ConfigureHeist(vaultId, "bags", "", "", false, STOCK, REFILL_SECONDS);
		ctx.Equal(STOCK, LF_Bank.GetStock(vaultId), "the island box holds one haul");

		bool armed = false;
		if (!RedProof())
			armed = LF_Bank.Rob(vaultId, robber);

		ctx.True(armed, "with the police gate met the box arms");
		ctx.Equal(0, LF_Bank.GetLooseBagCount(vaultId), "an armed box has spilled nothing yet");
		bool open = false;
		if (armed)
			open = LF_Bank.Advance(vaultId, STORE_FUSE);

		ctx.True(open, "the box opens when its 10 s fuse runs out");
		ctx.Equal(1, LF_Bank.GetLooseBagCount(vaultId), "one bag spills");
		ctx.Equal(PACK_HAUL, LF_Bank.GetBagCash(vaultId, 0), "the bag holds the shop's takings");
		ctx.Equal(0, LF_Bank.GetStock(vaultId), "the haul is gone from stock");

		// The shop is not farmed: a second try inside the cooldown is refused, the haul returns after 45 min.
		ctx.True(!LF_Bank.Rob(vaultId, robber), "a second try inside the cooldown is refused");
		LF_Bank.Advance(vaultId, COOLDOWN_SECONDS);
		ctx.Equal(STOCK, LF_Bank.GetStock(vaultId), "the haul refills after 45 minutes");

		for (int i = LF_Bank.GetLooseBagCount(vaultId) - 1; i >= 0; i--)
			SCR_EntityHelper.DeleteEntityAndChildren(LF_Bank.GetLooseBag(vaultId, i));

		LF_Bank.Restock(vaultId);
	}
}
