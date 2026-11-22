// red-proof: Observed before LF_GarbageRound.c existed: `tools\cli test --no-build --only LF_Test_GarbageRound` failed to compile ("Unknown type 'LF_GarbageRound'"), so the test could not pass. Then green on the feature; `--red` records RedProof() skipping the tip. Review of #3862: the new truck-return and dead-worker checks failed on the unfixed code (truck left after the tip; a dead worker was paid 70) and pass on the fix. Occupied-truck exploit: before the fix an occupied truck was left in place for good (a free truck per round); `--red` skips the sweep after the seats empty and the "taken back once it is empty" check fails. Shared-bins fix 2026-11-21: `--red LF_Test_GarbageRound` RED PROOF OK (with the first round read as abandoned, "the second round does not share a bin the first holds" fails); `--only` passed=2 failed=0.
//! jobs/garbage-round (#3325): a round of three bins emptied in order then tipped at the dump pays the
//! configured amount once. A bin emptied out of order is refused (the refusal has a toast key) and
//! changes nothing; tipping before the last bin, or with the truck away from the dump, pays nothing.
//! The route is the bins nearest the depot, found by component, never by name. The tip takes the truck
//! back, and a dead worker's tip is refused (review of #3862).
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
		LF_VehicleWorldSave.HoldAgain();
		ctx.False(LF_VehicleWorldSave.IsSaved(truck), "the company truck is out of the world save, so a restart puts no free copy back (#4122)");
		if (!truck)
			return;

		EntityID truckId = run.m_Truck;
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

		// The tipped truck goes back to the company: a round never leaves a free truck behind.
		ctx.True(!GetGame().GetWorld().FindEntityByID(truckId), "the company truck is taken back at the tip");

		CheckOccupiedTruck(ctx, depot, bins, dump, user);
		CheckSharedBins(ctx, depot, bins, user);
		CheckDeadWorker(ctx, depot, bins, dump, user);
	}

	//------------------------------------------------------------------------------------------------
	//! Two rounds open at once (review of #3862): each has its own bins. The second round skips the bins
	//! the first holds, cannot empty them, and gets them only once the first is closed or long abandoned.
	protected void CheckSharedBins(LF_TestContext ctx, IEntity depot, array<IEntity> bins, IEntity user)
	{
		string first = UID + "-first";
		string second = UID + "-second";
		LF_GarbageRoundConfig.Get().m_iStops = 2;
		ctx.True(LF_GarbageRound.Start(user, first, depot) == LF_EGarbageResult.OK, "the first of two rounds starts");
		LF_GarbageRun a = LF_GarbageRound.GetRound(first);
		// Red proof: the first round reads as abandoned, so it holds no bins (the unfixed behaviour).
		if (RedProof() && a)
			a.m_fStartedAt -= LF_GarbageRound.CLAIM_SECONDS + 10;

		ctx.True(LF_GarbageRound.Start(user, second, depot) == LF_EGarbageResult.OK, "the second round starts while the first is open");
		LF_GarbageRun b = LF_GarbageRound.GetRound(second);
		if (a && b)
		{
			ctx.True(a.m_aBins.Contains(bins[0].GetID()) && a.m_aBins.Contains(bins[1].GetID()), "the first round holds the two nearest bins");
			foreach (EntityID held : a.m_aBins)
			{
				ctx.False(b.m_aBins.Contains(held), "the second round does not share a bin the first holds");
			}

			ctx.True(b.m_aBins.Contains(bins[2].GetID()), "the second round takes the next free bin");
			ctx.True(LF_GarbageRound.EmptyBin(user, second, bins[0]) == LF_EGarbageResult.NOT_ON_ROUTE, "the second round cannot empty the first's bin");

			// An abandoned round (its owner left) holds nothing past the ceiling.
			LF_GarbageRound.Close(second);
			a.m_fStartedAt -= LF_GarbageRound.CLAIM_SECONDS + 10;
			ctx.True(LF_GarbageRound.Start(user, second, depot) == LF_EGarbageResult.OK, "a round starts again past an abandoned one");
			LF_GarbageRun again = LF_GarbageRound.GetRound(second);
			if (again)
				ctx.True(again.m_aBins.Contains(bins[0].GetID()), "an abandoned round's bins are free again");
		}

		LF_GarbageRound.Close(first);
		LF_GarbageRound.Close(second);
		LF_GarbageRoundConfig.Get().m_iStops = 3;
	}

	//------------------------------------------------------------------------------------------------
	//! A friend sits in the truck at the tip: the truck is not deleted under them, but it is marked, never
	//! lets the round keep it, and is deleted once the seats are empty (a free truck every round otherwise).
	protected void CheckOccupiedTruck(LF_TestContext ctx, IEntity depot, array<IEntity> bins, IEntity dump, IEntity user)
	{
		ctx.True(LF_GarbageRound.Start(user, UID, depot) == LF_EGarbageResult.OK, "a round for the occupied-truck case starts");
		foreach (IEntity bin : bins)
			LF_GarbageRound.EmptyBin(user, UID, bin);

		LF_GarbageRun run = LF_GarbageRound.GetRound(UID);
		if (!run)
			return;

		EntityID truckId = run.m_Truck;
		IEntity truck = GetGame().GetWorld().FindEntityByID(truckId);
		ctx.NotNull(truck, "the second company truck stands by the depot");
		if (!truck)
			return;

		LF_GarbageRound.SetOccupiedForTest(truckId, true);
		int cash = LF_MoneyUtils.GetCash(user);
		int paid;
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.OK, "a tip with someone in the truck still pays");
		ctx.True(GetGame().GetWorld().FindEntityByID(truckId) != null, "the occupied truck is not deleted under its passenger");
		ctx.True(LF_GarbageRound.IsPendingRemoval(truck), "the occupied truck is marked for removal");
		ctx.Equal(1, LF_GarbageRound.PendingCount(), "one truck waits to be taken back");

		LF_GarbageRound.SweepTrucks();
		ctx.True(GetGame().GetWorld().FindEntityByID(truckId) != null, "the sweep leaves a truck with someone aboard");
		ctx.True(LF_GarbageRound.IsPendingRemoval(truck), "and it stays marked");

		// The friend steps out: the truck goes.
		LF_GarbageRound.SetOccupiedForTest(truckId, false);
		if (!RedProof())
			LF_GarbageRound.SweepTrucks();

		ctx.True(!GetGame().GetWorld().FindEntityByID(truckId), "the truck is taken back once it is empty");
		ctx.Equal(0, LF_GarbageRound.PendingCount(), "nothing waits any more");

		if (paid > 0)
			LF_MoneyUtils.RemoveCash(user, LF_MoneyUtils.GetCash(user) - cash);
	}

	//------------------------------------------------------------------------------------------------
	//! A dead (or downed, or cuffed) worker tips nothing.
	protected void CheckDeadWorker(LF_TestContext ctx, IEntity depot, array<IEntity> bins, IEntity dump, IEntity user)
	{
		ctx.True(LF_GarbageRound.Start(user, UID, depot) == LF_EGarbageResult.OK, "a second round starts");
		foreach (IEntity bin : bins)
			LF_GarbageRound.EmptyBin(user, UID, bin);

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(user.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(damage, "the worker has a damage manager");
		if (!damage)
			return;

		damage.Kill(Instigator.CreateInstigator(null));
		int paid;
		ctx.True(LF_GarbageRound.Tip(user, UID, dump, paid) == LF_EGarbageResult.UNFIT, "a dead worker's tip is refused");
		ctx.Equal(0, paid, "and pays nothing");
		ctx.Equal(70, LF_MoneyUtils.GetCash(user), "the dead worker's cash is unchanged");
	}
}
