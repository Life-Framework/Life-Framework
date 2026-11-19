// red-proof: Observed before LF_GarbageRound.c existed: `tools\cli test --no-build --only LF_Test_GarbageRound` failed to compile ("Unknown type 'LF_GarbageRound'"), so the test could not pass. Then green on the feature; `--red` records RedProof() skipping the tip.
//! jobs/garbage-round (#3325): a round of three bins emptied in order then tipped at the dump pays the
//! configured amount once. A bin emptied out of order is refused (the refusal has a toast key) and
//! changes nothing; tipping before the last bin, or with the truck away from the dump, pays nothing.
//! The route is the bins nearest the depot, found by component, never by name.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GarbageRound : LF_Test
{
	protected static const string UID = "garbage-round-test";
	protected static const ResourceName BIN = "{C53D93D86C6CD2EE}Prefabs/Props/Bins/LF_StreetBin.et";
	protected static const ResourceName DEPOT = "{9C1E4B7A2D58F360}Prefabs/Jobs/LF_GarbageDepot.et";
	protected static const ResourceName DUMP = "{E48A06D3B1F7C925}Prefabs/Jobs/LF_GarbageDump.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const vector SPOT = "40 0 60"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/garbage-round";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}

	//------------------------------------------------------------------------------------------------
	protected int BinItems(IEntity binEntity)
	{
		LF_BinComponent bin = LF_Component<LF_BinComponent>.Find(binEntity);
		if (!bin)
			return -1;

		return bin.GetItems().Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_GarbageRoundConfig config = LF_GarbageRoundConfig.CreateDefault();
		config.m_iStops = 3;
		config.m_iRatePerStop = 20;
		config.m_iRoundBonus = 10;
		config.m_iMinRoundSeconds = 0;
		LF_GarbageRoundConfig.UseForTest(config);

		IEntity depot = SpawnAt(DEPOT, SPOT);
		array<IEntity> bins = {};
		for (int i = 0; i < 3; i++)
			bins.Insert(SpawnAt(BIN, SPOT + Vector(12 + i * 5, 0, 0)));

		IEntity dump = SpawnAt(DUMP, SPOT + Vector(0, 0, 25));
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(depot, "the depot spawns");
		ctx.NotNull(dump, "the dump spawns");
		ctx.NotNull(user, "the worker spawns");
		if (depot && dump && user && bins[0] && bins[1] && bins[2])
			CheckRound(ctx, depot, bins, dump, user);

		LF_GarbageRound.Close(UID);
		foreach (IEntity bin : bins)
			LF_TestCharacterFixture.Delete(bin);

		LF_TestCharacterFixture.Delete(depot);
		LF_TestCharacterFixture.Delete(dump);
		LF_TestCharacterFixture.Delete(user);
		LF_GarbageRoundConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRound(LF_TestContext ctx, IEntity depot, array<IEntity> bins, IEntity dump, IEntity user)
	{
		int cash = LF_MoneyUtils.GetCash(user);
		if (cash > 0)
			LF_MoneyUtils.RemoveCash(user, cash);

		int paid;
		ctx.True(LF_GarbageRound.EmptyBin(user, UID, bins[0]) == LF_EGarbageResult.NO_ROUND, "a bin with no round open is refused");
		ctx.True(LF_GarbageRound.Start(user, UID, depot) == LF_EGarbageResult.OK, "the round starts at the depot");
		ctx.True(LF_GarbageRound.HasRound(UID), "the round is open");

		LF_GarbageRun run = LF_GarbageRound.GetRound(UID);
		if (!run)
			return;

		ctx.Equal(3, run.m_aBins.Count(), "the route is three bins");
		for (int i = 0; i < 3; i++)
			ctx.True(run.m_aBins[i] == bins[i].GetID(), string.Format("route bin %1 is the one %1 from the depot", i));

		// A bin with rubbish in it is emptied for real.
		LF_InventoryUtils.AddAmount(bins[0], APPLE, 1);
		ctx.Equal(1, BinItems(bins[0]), "bin 0 holds an apple");

		LF_EGarbageResult skipped = LF_GarbageRound.EmptyBin(user, UID, bins[2]);
		ctx.True(skipped == LF_EGarbageResult.OUT_OF_ORDER, "the last bin first is refused");
		ctx.False(LF_GarbageRound.RefusalKey(skipped).IsEmpty(), "the refusal carries a toast");
		ctx.Equal(0, run.m_Rules.StopsDone(), "a refusal changes nothing");

		ctx.True(LF_GarbageRound.EmptyBin(user, UID, bins[0]) == LF_EGarbageResult.OK, "bin 0 is emptied");
		ctx.Equal(0, BinItems(bins[0]), "bin 0 is empty after");
		ctx.True(LF_GarbageRound.EmptyBin(user, UID, bins[0]) == LF_EGarbageResult.OUT_OF_ORDER, "bin 0 twice is refused");
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.NOT_ENOUGH, "a tip before the last bin is refused");
		ctx.Equal(0, paid, "and pays nothing");
		ctx.True(LF_GarbageRound.EmptyBin(user, UID, bins[1]) == LF_EGarbageResult.OK, "bin 1 is emptied");
		ctx.True(LF_GarbageRound.EmptyBin(user, UID, bins[2]) == LF_EGarbageResult.OK, "bin 2 is emptied");

		// The truck must stand at the dump.
		IEntity truck = GetGame().GetWorld().FindEntityByID(run.m_Truck);
		ctx.NotNull(truck, "the company truck stands by the depot");
		if (!truck)
			return;

		vector home = truck.GetOrigin();
		truck.SetOrigin(home + Vector(60, 0, 0));
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.TRUCK_FAR, "a tip with the truck away is refused");
		truck.SetOrigin(home);

		LF_GarbageRoundConfig.Get().m_iMinRoundSeconds = 100000;
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.TOO_FAST, "a round finished faster than the minimum time pays nothing");
		ctx.Equal(0, paid, "a too-fast tip pays nothing");
		LF_GarbageRoundConfig.Get().m_iMinRoundSeconds = 0;

		LF_EGarbageResult tipped = LF_EGarbageResult.NO_ROUND;
		if (!RedProof())
			tipped = LF_GarbageRound.Tip(user, UID, dump, paid);

		ctx.True(tipped == LF_EGarbageResult.OK, "the tip at the dump succeeds");
		ctx.Equal(70, paid, "the round pays 3 x 20 + 10");
		ctx.Equal(70, LF_MoneyUtils.GetCash(user), "the cash is in the worker's pocket");
		ctx.False(LF_GarbageRound.HasRound(UID), "the round is closed");
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.NO_ROUND, "a second tip is refused");
		ctx.Equal(70, LF_MoneyUtils.GetCash(user), "and pays nothing more");
	}
}
