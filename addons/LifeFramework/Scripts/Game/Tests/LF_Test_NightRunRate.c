// red-proof: `toolscli test --no-build --red LF_Test_NightRunRate`: RedProof() leaves every run pay row at no night rate, so the 23:00 fuel run pays the base and the night assertions fail first (expected base plus the night extra).
//! #3148: a delivery run finished between dusk and dawn on the world clock pays a night rate, and the
//! pay toast names each part. The pure part: the window (21 to 5, wrapping midnight), the extra
//! (round(base x (rate - 1))), and the toast key and argument built from the parts. The world part: with
//! the clock at 12:00 a fuel run pays its base; at 23:00 base plus the night extra, the night kept apart
//! from the bonus; a contract run at night reports the contract and standing bonus as one part and the
//! night as another, the base untouched; a world with no night rate row pays the base at night.
//! The test puts the clock back and reloads the jobs config.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NightRunRate : LF_Test
{
	static const string UID = "test-night-run-rate";

	override string GetName()
	{
		return "jobs/night-run-rate";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		array<int> percents = {0, 0, 0, 5, 10, 15};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aPays = {};
		config.m_aPays.Insert(LF_ReputationPayDef.Make("TOWN", percents));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationContractsConfig MakeContracts()
	{
		LF_ReputationContractsConfig config = new LF_ReputationContractsConfig();
		config.m_fCooldownSeconds = 0;
		config.m_aContracts = {};
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_town", "PARCEL", "", "TOWN", 1.5, 10, 15));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_CivilianJob.ForgetCareer(UID);
		LF_Reputation.Forget(UID);
		LF_ReputationContracts.Forget(UID);
		LF_ReputationPay.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the PARCEL, FUEL_DELIVERY and TAXI rows `rate` (RedProof leaves them at no night rate).
	protected void SetRates(LF_JobsConfig config, float rate)
	{
		if (RedProof())
			return;

		foreach (LF_JobRunPayDef row : config.m_aRunPays)
			row.m_fNightRate = rate;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Start(LF_TestContext ctx, LF_EJobType job, string what)
	{
		ctx.True(LF_CivilianJob.SignUp(UID, job), what + ": the job signs up");
		bool started = LF_CivilianJob.StartRun(UID, job);
		ctx.True(started, what + ": the run starts");
		return started;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPure(LF_TestContext ctx)
	{
		ctx.True(LF_CivilianJob.IsNightHour(23, 21, 5), "23:00 is inside the 21 to 5 window");
		ctx.True(LF_CivilianJob.IsNightHour(2.5, 21, 5), "02:30 is inside it");
		ctx.True(LF_CivilianJob.IsNightHour(21, 21, 5), "21:00 itself is night");
		ctx.False(LF_CivilianJob.IsNightHour(5, 21, 5), "05:00 itself is day");
		ctx.False(LF_CivilianJob.IsNightHour(12, 21, 5), "noon is day");
		ctx.True(LF_CivilianJob.IsNightHour(1, 0, 6), "a window that does not wrap midnight holds 01:00");
		ctx.False(LF_CivilianJob.IsNightHour(23, 22, 22), "equal hours make no night");

		ctx.Equal(21, LF_CivilianJob.NightExtra(85, 1.25), "85 at 1.25 adds 21");
		ctx.Equal(0, LF_CivilianJob.NightExtra(85, 1), "a rate of 1 adds nothing");
		ctx.Equal(0, LF_CivilianJob.NightExtra(85, 0), "a rate of 0 adds nothing");
		ctx.Equal(0, LF_CivilianJob.NightExtra(0, 1.25), "no base adds nothing");

		LF_JobPayParts plain = LF_JobPayParts.Make(85, 0, 0);
		ctx.EqualStr("#EL-Job_Paid", plain.ToastKey(), "a plain run keeps the plain toast");
		ctx.EqualStr("85", plain.ToastArg(), "its argument is the total");

		LF_JobPayParts night = LF_JobPayParts.Make(106, 0, 21);
		ctx.EqualStr("#LF-Job_Paid_Night", night.ToastKey(), "a night run names the night part");
		ctx.EqualStr("~106|85|21", night.ToastArg(), "total, run, night");

		LF_JobPayParts bonus = LF_JobPayParts.Make(119, 34, 0);
		ctx.EqualStr("#LF-Job_Paid_Bonus", bonus.ToastKey(), "a contract run names the bonus part");
		ctx.EqualStr("~119|85|34", bonus.ToastArg(), "total, run, bonus");

		LF_JobPayParts both = LF_JobPayParts.Make(140, 34, 21);
		ctx.EqualStr("#LF-Job_Paid_NightBonus", both.ToastKey(), "a night contract run names both");
		ctx.EqualStr("~140|85|21|34", both.ToastArg(), "total, run, night, bonus");
		ctx.Equal(85, both.m_iBase, "the run part is what is left");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorld(LF_TestContext ctx, LF_JobsConfig config, TimeAndWeatherManagerEntity clock)
	{
		// Five parcel runs by day open fuel delivery (level 2).
		clock.SetTimeOfTheDay(12, true);
		LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL);
		for (int i = 0; i < 5; i++)
		{
			LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL);
			LF_CivilianJob.CompleteRun(UID);
		}

		int fuelBase = LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY);

		// Noon: the base.
		if (!Start(ctx, LF_EJobType.FUEL_DELIVERY, "noon fuel"))
			return;

		ctx.Equal(fuelBase, LF_CivilianJob.CompleteRun(UID), "at 12:00 a fuel run pays the base");
		ctx.Equal(0, LF_CivilianJob.TakeLastNight(UID), "and no night part");

		// 23:00: base times the rate, the night kept apart.
		clock.SetTimeOfTheDay(23, true);
		if (!Start(ctx, LF_EJobType.FUEL_DELIVERY, "night fuel"))
			return;

		int extra = LF_CivilianJob.NightExtra(fuelBase, 1.25);
		int paid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(fuelBase + extra, paid, "at 23:00 a fuel run pays the base times the night rate");
		ctx.Equal(extra, LF_CivilianJob.TakeLastNight(UID), "the night part is the extra");
		ctx.Equal(0, LF_CivilianJob.TakeLastNight(UID), "and taken once");

		// A job with no night rate row pays the base at night.
		SetRates(config, 1);
		if (!Start(ctx, LF_EJobType.FUEL_DELIVERY, "no-rate fuel"))
			return;

		ctx.Equal(fuelBase, LF_CivilianJob.CompleteRun(UID), "with no night rate a night fuel run pays the base");

		// A contract run at night: the bonus and the night are separate parts and the base is untouched.
		SetRates(config, 1.25);
		LF_Reputation.Add(UID, "TOWN", 1000, "test");
		if (!Start(ctx, LF_EJobType.PARCEL, "night contract"))
			return;

		ctx.True(LF_ReputationContracts.Take(UID, "c_town", LF_JobGuards.Now()), "the contract is taken");
		int parcelBase = LF_CivilianJob.PayFor(LF_EJobType.PARCEL);
		int contractBonus = Math.Round(parcelBase * 1.5) - parcelBase;
		int standingBonus = Math.Round(parcelBase * 0.15);
		int nightPart = LF_CivilianJob.NightExtra(parcelBase, 1.25);
		int contractPaid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(parcelBase + contractBonus + standingBonus + nightPart, contractPaid, "a night contract run pays base, contract, standing bonus and night, none compounding");
		int bonus = LF_ReputationContracts.TakeLastBonus(UID) + LF_ReputationPay.TakeLastBonus(UID);
		int night = LF_CivilianJob.TakeLastNight(UID);
		LF_JobPayParts parts = LF_JobPayParts.Make(contractPaid, bonus, night);
		ctx.Equal(contractBonus + standingBonus, parts.m_iBonus, "the bonus is reported as its own part");
		ctx.Equal(nightPart, parts.m_iNight, "the night is its own part");
		ctx.Equal(parcelBase, parts.m_iBase, "the run part is the plain base");
		ctx.EqualStr("#LF-Job_Paid_NightBonus", parts.ToastKey(), "the toast names both");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunPure(ctx);

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		ctx.True(world != null, "a ChimeraWorld is loaded");
		if (!world)
			return;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		ctx.True(clock != null, "the world has a TimeAndWeatherManager");
		if (!clock)
			return;

		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the jobs config loads");
		if (!config)
			return;

		float oldTime = clock.GetTimeOfTheDay();
		bool oldAuto = clock.GetIsDayAutoAdvanced();
		clock.SetIsDayAutoAdvanced(false);

		Clean();
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.UseForTest(MakeContracts());
		SetRates(config, 1.25);

		RunWorld(ctx, config, clock);

		Clean();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_JobsConfig.Reload();
		clock.SetIsDayAutoAdvanced(oldAuto);
		clock.SetTimeOfTheDay(oldTime, true);
	}
}
