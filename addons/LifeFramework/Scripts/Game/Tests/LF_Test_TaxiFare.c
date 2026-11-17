// red-proof: Observed 2026-11-17 before LF_TaxiFare.c existed: `tools\cli test --no-build --only LF_Test_TaxiFare` failed ("Unknown type 'LF_TaxiFareConfig'", "Can't compile Game script module"), so the test could not pass. Then green on the feature.
// tier: LOGIC
//! Taxi fare pricing (#2690): LF_TaxiFare.Price turns the straight-line metres between the pickup and
//! the drop-off into a fare. A short hop pays the floor, a longer run pays the floor plus the rate per
//! metre, and a very long run pays the cap. A config that is zero or negative falls back to the
//! shipped numbers (and logs an LF_Debug.Error) instead of paying nothing or an unbounded fare.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TaxiFare : LF_Test
{
	override string GetName()
	{
		return "jobs/taxi-fare";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_TaxiFareConfig Make(int floor, float rate, int cap, float shortHop)
	{
		LF_TaxiFareConfig config = LF_TaxiFareConfig.CreateDefault();
		config.m_iFloor = floor;
		config.m_fRatePerMetre = rate;
		config.m_iCap = cap;
		config.m_fShortHopMetres = shortHop;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TaxiFareConfig stock = LF_TaxiFareConfig.CreateDefault();
		ctx.Equal(80, stock.m_iFloor, "the shipped floor is 80");
		ctx.Equal(300, stock.m_iCap, "the shipped cap is 300");

		// The shipped .conf resolves through its GUID and carries the same numbers.
		LF_TaxiFareConfig.Reload();
		LF_TaxiFareConfig shipped = LF_TaxiFareConfig.Get();
		ctx.NotNull(shipped, "Configs/Jobs/LF_TaxiFares.conf resolves");
		if (shipped)
		{
			ctx.Equal(80, shipped.m_iFloor, "the .conf floor is 80");
			ctx.EqualFloat(0.04, shipped.m_fRatePerMetre, 0.0001, "the .conf rate is 0.04 a metre");
			ctx.Equal(300, shipped.m_iCap, "the .conf cap is 300");
			ctx.True(shipped.IsUsable(), "the .conf numbers can price a fare");
		}

		// Distances are along X so the straight line is exact.
		vector pickup = Vector(1000, 0, 1000);
		LF_TaxiFareConfig.UseForTest(stock);

		ctx.Equal(80, LF_TaxiFare.Price(pickup, pickup + Vector(50, 0, 0)), "50 m pays the floor");
		ctx.Equal(80, LF_TaxiFare.Price(pickup, pickup), "no distance pays the floor");
		ctx.Equal(160, LF_TaxiFare.Price(pickup, pickup + Vector(2000, 0, 0)), "2 km pays the floor plus the rate per metre");
		ctx.Equal(228, LF_TaxiFare.Price(pickup, pickup + Vector(3700, 0, 0)), "Saint-Philippe to Montignac pays 228");
		ctx.Equal(300, LF_TaxiFare.Price(pickup, pickup + Vector(20000, 0, 0)), "20 km pays the cap");

		// A rising distance never lowers the fare.
		int last = 0;
		float metres;
		for (int i = 0; i < 30; i++)
		{
			metres = i * 1000;
			int fare = LF_TaxiFare.Price(pickup, pickup + Vector(metres, 0, 0));
			ctx.True(fare >= last, string.Format("the fare at %1 m (%2) is not below the one before (%3)", metres, fare, last));
			ctx.True(fare >= 80 && fare <= 300, string.Format("the fare at %1 m (%2) stays between the floor and the cap", metres, fare));
			last = fare;
		}

		// A server's own numbers apply.
		LF_TaxiFareConfig custom = Make(100, 0.1, 500, 50);
		ctx.Equal(100, LF_TaxiFare.PriceFor(custom, 40), "a custom floor pays on a short hop");
		ctx.Equal(300, LF_TaxiFare.PriceFor(custom, 2000), "a custom rate prices the metres");
		ctx.Equal(500, LF_TaxiFare.PriceFor(custom, 50000), "a custom cap caps the fare");

		// A broken config falls back to the shipped numbers.
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(0, 0.04, 300, 100), 2000), "a zero floor falls back to the shipped numbers");
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(-5, 0.04, 300, 100), 2000), "a negative floor falls back to the shipped numbers");
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(80, 0, 300, 100), 2000), "a zero rate falls back to the shipped numbers");
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(80, -1, 300, 100), 2000), "a negative rate falls back to the shipped numbers");
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(80, 0.04, 0, 100), 2000), "a zero cap falls back to the shipped numbers");
		ctx.Equal(160, LF_TaxiFare.PriceFor(Make(80, 0.04, 50, 100), 2000), "a cap below the floor falls back to the shipped numbers");
		ctx.Equal(80, LF_TaxiFare.PriceFor(null, 10), "no config pays the shipped floor");

		LF_TaxiFareConfig.Reload();
	}
}
