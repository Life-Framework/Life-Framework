// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_DirtyPayout`: RED PROOF OK; the hook leaves the switch off: `switch on: no new cash (expected 200, got 400)`, `the vault bag holds marked notes (expected 250, got 0)`. Green with LF_Test_RobReward, LF_Test_PlacesBankBag, LF_Test_MarkedCash.
//! crime/dirty-payout (#2851): the server's dirty-money switch (LF_LaunderConfig.m_bCrimePaysMarked).
//! Off (the core default): a till robbery pays cash as today. On: the same robbery leaves the robber
//! the same amount of marked notes and no new cash, and a filled vault bag holds marked notes, not cash.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DirtyPayout : LF_Test
{
	protected static const string VAULT_ID = "dirty-payout-vault";
	protected static const int PAYOUT = 200;

	override string GetName()
	{
		return "crime/dirty-payout";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		LF_PlayerAccountManager.Reset();
		LF_RobAction.ResetSession();
		LF_MarkedCash.ResetStatics();
		LF_LaunderConfig.ResetStatics();

		ctx.False(LF_LaunderConfig.CrimePaysMarked(), "the core config ships the switch off");

		string uid = "test-dirty-payout";
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		ctx.NotNull(LF_PlayerAccountManager.GetOrCreate(uid), "robber civilian account exists");

		// Switch off: today's payout, cash.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(false);
		float nowMs = GetGame().GetWorld().GetWorldTime() + 1000;
		float next = 0;
		int paid = LF_RobAction.CommitRobbery(uid, robber, PAYOUT, 0, 0, 60, 0, nowMs, next);
		ctx.Equal(PAYOUT, paid, "switch off: the robbery reports the payout");
		ctx.Equal(PAYOUT, LF_MoneyUtils.GetCash(robber), "switch off: the payout is cash");
		ctx.Equal(0, LF_MarkedCash.Count(robber), "switch off: no marked notes");

		// Switch on: the same robbery after the cooldown pays marked notes and no new cash.
		if (!RedProof())
			LF_LaunderConfig.SetCrimePaysMarkedForTests(true);

		paid = LF_RobAction.CommitRobbery(uid, robber, PAYOUT, 0, 0, 60, next, nowMs + 61000, next);
		ctx.Equal(PAYOUT, paid, "switch on: the robbery reports the payout");
		ctx.Equal(PAYOUT, LF_MarkedCash.Count(robber), "switch on: the payout is marked notes");
		ctx.Equal(PAYOUT, LF_MoneyUtils.GetCash(robber), "switch on: no new cash");

		// Switch on: a filled vault bag holds marked notes.
		IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Bank.BagPrefab(), robber.GetOrigin() + "0 -20 0", "0 0 0", false);
		ctx.NotNull(bag, "a vault bag spawns");
		if (bag && LF_Places.Register(LF_Bank.BAG_PLACE_KIND, VAULT_ID, bag))
		{
			LF_Bank.FillVaultBag(VAULT_ID, 250);
			ctx.Equal(250, LF_Bank.ReadWorldBagMarked(bag), "switch on: the vault bag holds marked notes");
			ctx.Equal(0, LF_Bank.ReadWorldBagCash(bag), "switch on: the vault bag holds no cash");
		}
		else
		{
			ctx.True(false, "the bag is placed under the test vault id");
		}

		LF_LaunderConfig.ResetStatics();
		LF_MarkedCash.ResetStatics();
		if (bag)
			SCR_EntityHelper.DeleteEntityAndChildren(bag);
		LF_TestCharacterFixture.Delete(robber);
	}
}
