// red-proof: 2026-11-18, before LF_BuyerNpc existed the test did not compile; with the spawn act
//   skipped (RedProof() on, `cli test --no-build --red LF_Test_BuyerNpc`) the first sweep spawns nobody and
//   `the buyer stands at the open spot` fails. The run that went red and then green is recorded in the PR.
//! laundering/buyer-npc (#2919): the illicit buyer stands at the open `chain_buyer` spot only. Three spots
//! and a forced period: with a player near, one buyer stands at the open spot and no other; a second sweep
//! spawns no duplicate; the next period moves him (old spot empty, new spot occupied, one despawn); with no
//! player near none is spawned and a live buyer is removed past the despawn radius.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuyerNpc : LF_Test
{
	protected static const string SEED = "buyer-npc-test-seed";
	protected static const string SPOT_A = "bn-a";
	protected static const string SPOT_B = "bn-b";
	protected static const string SPOT_C = "bn-c";
	protected static const int PERIOD_HOURS = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/buyer-npc";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(vector at)
	{
		return LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
	}

	//------------------------------------------------------------------------------------------------
	protected static float DistXZ(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex breach door anchors the fixtures");
		if (!anchor)
			return;

		LF_BuyerRotation.ResetStatics();
		LF_BuyerRotation.SetSeedForTests(SEED);
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(PERIOD_HOURS);
		LF_BuyerNpc.ResetStatics();

		vector at = anchor.GetOrigin() + "0 -60 0";
		IEntity a = Spawn(at);
		IEntity b = Spawn(at + "400 0 0");
		IEntity c = Spawn(at + "800 0 0");
		ctx.True(a && b && c, "three fixture spots spawn");
		if (a && b && c)
			RunWith(ctx, a, b, c);

		LF_BuyerNpc.DespawnAll();
		LF_BuyerNpc.ResetStatics();
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_A);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_B);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_C);
		SCR_EntityHelper.DeleteEntityAndChildren(a);
		SCR_EntityHelper.DeleteEntityAndChildren(b);
		SCR_EntityHelper.DeleteEntityAndChildren(c);
		LF_BuyerRotation.ResetStatics();
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, IEntity a, IEntity b, IEntity c)
	{
		int period = PERIOD_HOURS * 3600;
		int start = 125001 * period;

		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_A, a), "spot a is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_B, b), "spot b is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_C, c), "spot c is placed");

		string open1 = LF_BuyerRotation.OpenSpot(start);
		string open2 = LF_BuyerRotation.OpenSpot(start + period);
		ctx.False(open1.IsEmpty() || open2.IsEmpty() || open1 == open2, "two periods in a row open different spots");
		IEntity spot1 = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open1);
		IEntity spot2 = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open2);
		if (!spot1 || !spot2)
		{
			ctx.True(false, "the open spots resolve to entities");
			return;
		}

		LF_Test_BuyerNpc_Players players = new LF_Test_BuyerNpc_Players();

		// No player near: nobody is spawned.
		LF_BuyerNpc.SweepAt(start, players);
		ctx.Equal(0, LF_BuyerNpc.SpawnedTotal(), "with no player near no buyer is spawned");
		ctx.True(LF_BuyerNpc.GetLive() == null, "no buyer stands with nobody near");

		// A player at the open spot: one buyer stands there, and no other spot has one.
		players.m_aAt.Insert(spot1.GetOrigin() + "30 0 0");
		if (!RedProof())
			LF_BuyerNpc.SweepAt(start, players);

		ctx.Equal(1, LF_BuyerNpc.SpawnedTotal(), "one buyer is spawned");
		IEntity buyer = LF_BuyerNpc.GetLive();
		ctx.NotNull(buyer, "the buyer stands at the open spot");
		ctx.EqualStr(open1, LF_BuyerNpc.LiveSpot(), "the buyer is tied to the open spot");
		if (buyer)
		{
			ctx.EqualFloat(0, DistXZ(buyer.GetOrigin(), spot1.GetOrigin()), 0.5, "the buyer stands on the open spot");
			ctx.True(DistXZ(buyer.GetOrigin(), spot2.GetOrigin()) > 100, "he is not at the next period's spot");
		}

		// A second sweep in the same period leaves the one buyer.
		LF_BuyerNpc.SweepAt(start + 60, players);
		ctx.Equal(1, LF_BuyerNpc.SpawnedTotal(), "a second sweep spawns no duplicate");

		// The next period: he leaves the old spot; with the player at the new spot he appears there.
		players.m_aAt.Clear();
		players.m_aAt.Insert(spot1.GetOrigin() + "30 0 0");
		players.m_aAt.Insert(spot2.GetOrigin() + "30 0 0");
		LF_BuyerNpc.SweepAt(start + period, players);
		ctx.Equal(1, LF_BuyerNpc.DespawnedTotal(), "the rotation moving removes him from the old spot");
		LF_BuyerNpc.SweepAt(start + period, players);
		ctx.Equal(2, LF_BuyerNpc.SpawnedTotal(), "he appears at the new spot");
		ctx.EqualStr(open2, LF_BuyerNpc.LiveSpot(), "the buyer is tied to the new open spot");
		IEntity moved = LF_BuyerNpc.GetLive();
		if (moved)
			ctx.EqualFloat(0, DistXZ(moved.GetOrigin(), spot2.GetOrigin()), 0.5, "the buyer stands on the new open spot");

		// A player beyond the despawn radius removes him; with the player out of spawn range none appears.
		players.m_aAt.Clear();
		players.m_aAt.Insert(spot2.GetOrigin() + "250 0 0");
		LF_BuyerNpc.SweepAt(start + period, players);
		ctx.True(LF_BuyerNpc.GetLive() == null, "no player near his spot removes him");
		ctx.Equal(2, LF_BuyerNpc.DespawnedTotal(), "the second removal is counted");
		LF_BuyerNpc.SweepAt(start + period, players);
		ctx.Equal(2, LF_BuyerNpc.SpawnedTotal(), "a player out of spawn range spawns nobody");
	}
}

//------------------------------------------------------------------------------------------------
//! Stand-in players for the buyer test: wherever the test puts them.
class LF_Test_BuyerNpc_Players : LF_ResidentPlayers
{
	ref array<vector> m_aAt = {};

	override int Positions(notnull array<vector> into)
	{
		foreach (vector at : m_aAt)
			into.Insert(at);

		return m_aAt.Count();
	}
}
