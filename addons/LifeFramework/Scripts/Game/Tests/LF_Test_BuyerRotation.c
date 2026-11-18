// red-proof: 2026-11-18, `cli test --no-build --red LF_Test_BuyerRotation`: RED-OK, "a whole cycle opens each of the three spots once (expected 3, got 2)". The first draft (keys sorted by a trailing-counter hash) opened the spots in id order every cycle; the "not always the next id" check guards that.
//! laundering/buyer-rotation (#2856): the illicit buyer's open spot is a pure function of unix time and
//! the server's rotation seed. Three spots found as LF_Places kind chain_buyer, a 4 h period: every time in
//! one period gives the same spot, a whole cycle (3 periods) visits each spot once so the next period
//! differs, a restart (statics dropped, same seed) lands on the same spot, a spot added mid-period joins
//! at the next period, and with no spot placed the answer is none.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuyerRotation : LF_Test
{
	protected static const string SEED = "buyer-rotation-test-seed";
	protected static const string SPOT_A = "br-a";
	protected static const string SPOT_B = "br-b";
	protected static const string SPOT_C = "br-c";
	protected static const string SPOT_D = "br-d";
	protected static const int PERIOD_HOURS = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/buyer-rotation";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(vector at)
	{
		return LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
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

		vector at = anchor.GetOrigin() + "0 -60 0";
		IEntity a = Spawn(at);
		IEntity b = Spawn(at + "3 0 0");
		IEntity c = Spawn(at + "6 0 0");
		IEntity d = Spawn(at + "9 0 0");
		ctx.True(a && b && c && d, "four fixture spots spawn");
		if (a && b && c && d)
			RunWith(ctx, a, b, c, d);

		SCR_EntityHelper.DeleteEntityAndChildren(a);
		SCR_EntityHelper.DeleteEntityAndChildren(b);
		SCR_EntityHelper.DeleteEntityAndChildren(c);
		SCR_EntityHelper.DeleteEntityAndChildren(d);
		LF_BuyerRotation.ResetStatics();
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, IEntity a, IEntity b, IEntity c, IEntity d)
	{
		int period = PERIOD_HOURS * 3600;
		// 125001 is a multiple of 3: a period that opens a cycle of three, so the three periods after it are a whole cycle.
		int first = 125001;
		int start = first * period;
		ctx.Equal(first, LF_BuyerRotation.PeriodOf(start, period), "the fixture time sits at the start of its period");

		// No spot placed: none, and asking again is still none.
		ctx.EqualStr("", LF_BuyerRotation.OpenSpot(start), "no spot placed gives no open spot");
		ctx.EqualStr("", LF_BuyerRotation.OpenSpot(start + 5), "still none the second time");

		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_A, a), "spot a is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_B, b), "spot b is placed");
		if (!RedProof())
			ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_C, c), "spot c is placed");

		array<string> seen = {};
		for (int p = 0; p < 3; p++)
		{
			int t = start + p * period;
			string open = LF_BuyerRotation.OpenSpot(t);
			ctx.False(open.IsEmpty(), string.Format("period %1 has an open spot", p));
			ctx.EqualStr(open, LF_BuyerRotation.OpenSpot(t + 1), string.Format("the start of period %1 and one second on match", p));
			ctx.EqualStr(open, LF_BuyerRotation.OpenSpot(t + period - 1), string.Format("the last second of period %1 matches its start", p));
			if (seen.Find(open) < 0)
				seen.Insert(open);
		}

		ctx.Equal(3, seen.Count(), "a whole cycle opens each of the three spots once");
		ctx.True(LF_BuyerRotation.OpenSpot(start) != LF_BuyerRotation.OpenSpot(start + period) || LF_BuyerRotation.OpenSpot(start + period) != LF_BuyerRotation.OpenSpot(start + 2 * period), "the open spot changes in the next period");

		// A restart keeps it: nothing is saved, the statics are dropped and the same seed gives the same spot.
		string before = LF_BuyerRotation.OpenSpot(start + period);
		LF_BuyerRotation.ForgetPeriod();
		ctx.EqualStr(before, LF_BuyerRotation.OpenSpot(start + period), "after dropping the statics the same time gives the same spot");

		// The order is a real shuffle, not the id order, and it depends on the seed (a public number
		// could be read off by anyone who saw the buyer once).
		bool notIdOrder = false;
		bool otherSeed = false;
		for (int cyc = 0; cyc < 12; cyc++)
		{
			for (int pos = 0; pos < 3; pos++)
			{
				int idx = LF_BuyerRotation.PickIndex(SEED, cyc * 3 + pos, 3);
				if (idx != pos)
					notIdOrder = true;

				if (idx != LF_BuyerRotation.PickIndex("another-seed", cyc * 3 + pos, 3))
					otherSeed = true;
			}
		}

		ctx.True(notIdOrder, "over twelve cycles the open spot is not always the next id");
		ctx.True(otherSeed, "another seed opens different spots");

		// A spot added mid-period joins at the next period, not now.
		string held = LF_BuyerRotation.OpenSpot(start + 3 * period);
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_D, d), "spot d is placed mid-period");
		ctx.EqualStr(held, LF_BuyerRotation.OpenSpot(start + 3 * period + 7), "the period already under way keeps its spot");

		bool joined = false;
		for (int q = 4; q <= 11; q++)
		{
			if (LF_BuyerRotation.OpenSpot(start + q * period) == SPOT_D)
				joined = true;
		}

		ctx.True(joined, "the spot added later joins the rotation within two cycles");
	}
}
