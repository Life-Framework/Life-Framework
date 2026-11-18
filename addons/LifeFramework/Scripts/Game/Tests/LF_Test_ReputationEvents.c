// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ReputationEvents`: RED-OK reputation/events (hook leaves the subscriber off the bus; e.g. `a chopped car raises the underworld: underworld (expected 12, got 0)`). Green with `--only`.
//! Standing listens to the event bus (#2697): a finished civilian run raises town, an arrest lowers town
//! and police and gives the underworld a little, a witness statement raises police (and costs the witness
//! a little underworld when it describes a suspect), a recovered car raises the officer's police standing,
//! a flagged logout lowers police, a fence sale, a chopped car, a chain collect and heist loot raise the
//! underworld. A wanted level change moves nothing. The points come from the config rows; the shipped
//! config has a row of the right faction and sign for every action the subscriber awards.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationEvents : LF_Test
{
	static const string TOWN = "TOWN";
	static const string POLICE = "POLICE";
	static const string UNDERWORLD = "UNDERWORLD";

	override string GetName()
	{
		return "reputation/events";
	}

	//------------------------------------------------------------------------------------------------
	//! Three factions with no cap and a row per action, with points that differ per row so a wrong row shows.
	static LF_ReputationConfig MakeConfig()
	{
		LF_ReputationConfig config = new LF_ReputationConfig();
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make(TOWN, thresholds));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make(POLICE, thresholds));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make(UNDERWORLD, thresholds));
		config.m_aAwards = {};
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_JOB_COMPLETED, TOWN, 5));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make("job_completed:TAXI", TOWN, 15));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_ARREST_TOWN, TOWN, -20));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_ARREST_POLICE, POLICE, -21));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_ARREST_UNDERWORLD, UNDERWORLD, 4));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_SUSPECT_IDENTIFIED, TOWN, -12));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_WITNESS_STATEMENT, POLICE, 10));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_WITNESS_UNDERWORLD, UNDERWORLD, -5));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_VEHICLE_RECOVERED, POLICE, 11));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_FLAGGED_LOGOUT, POLICE, -13));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_FENCE_SALE, UNDERWORLD, 6));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_VEHICLE_CHOPPED, UNDERWORLD, 12));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_CHAIN_STEP, UNDERWORLD, 3));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(LF_ReputationSubscriber.ACTION_HEIST_LOOT, UNDERWORLD, 16));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig.UseForTest(MakeConfig());
		// Start with the subscriber off the bus: the game mode may have put it there.
		LF_ReputationSubscriber.Unsubscribe();
		// RedProof: leave it off, so no event moves a point (`--red`).
		if (!RedProof())
			ctx.True(LF_ReputationSubscriber.EnsureSubscribed(), "the subscriber is on the bus for every standing event");

		RunJobs(ctx);
		RunPolice(ctx);
		RunUnderworld(ctx);
		RunNoLoss(ctx);

		// Back to what the game mode set up, then the shipped config.
		LF_ReputationSubscriber.EnsureSubscribed();
		RunShippedRows(ctx);
		LF_ReputationConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void Expect(LF_TestContext ctx, string uid, int town, int police, int underworld, string what)
	{
		ctx.Equal(town, LF_Reputation.Points(uid, TOWN), what + ": town");
		ctx.Equal(police, LF_Reputation.Points(uid, POLICE), what + ": police");
		ctx.Equal(underworld, LF_Reputation.Points(uid, UNDERWORLD), what + ": underworld");
		LF_Reputation.Forget(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJobs(LF_TestContext ctx)
	{
		string uid = "test-rep-events-job";
		LF_Reputation.Forget(uid);
		LF_EventBus.Publish(LF_OnJobCompleted.Create(uid, LF_EJobType.PARCEL, 40));
		Expect(ctx, uid, 5, 0, 0, "a parcel run raises town by the general row");

		LF_EventBus.Publish(LF_OnJobCompleted.Create(uid, LF_EJobType.TAXI, 80));
		Expect(ctx, uid, 15, 0, 0, "a taxi run takes its own row");

		LF_EventBus.Publish(LF_OnJobCompleted.Create(uid, LF_EJobType.POLICE, 100));
		LF_EventBus.Publish(LF_OnJobCompleted.Create(uid, LF_EJobType.MEDIC, 100));
		Expect(ctx, uid, 0, 0, 0, "duty pay is not a civilian run");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPolice(LF_TestContext ctx)
	{
		string uid = "test-rep-events-arrested";
		LF_Reputation.Forget(uid);
		LF_EventBus.Publish(LF_OnArrested.Create(uid, 300, 2));
		Expect(ctx, uid, -20, -21, 4, "an arrest costs town and police and gives the underworld a little");

		uid = "test-rep-events-identified";
		LF_EventBus.Publish(LF_OnSuspectIdentified.Create(uid, "id check", 2));
		Expect(ctx, uid, -12, 0, 0, "an identification costs town only");

		uid = "test-rep-events-witness";
		LF_EventBus.Publish(LF_OnWitnessStatement.Create(7, uid, false, "officer", "red jacket", "", ""));
		Expect(ctx, uid, 0, 10, -5, "a statement describing a suspect: police up, underworld down");

		LF_EventBus.Publish(LF_OnWitnessStatement.Create(8, uid, false, "payphone", "", "", ""));
		Expect(ctx, uid, 0, 10, 0, "a statement that describes nobody costs no underworld standing");

		LF_EventBus.Publish(LF_OnWitnessStatement.Create(9, "", true, "payphone", "grey coat", "", ""));
		ctx.Equal(0, LF_Reputation.Points("", POLICE), "a town NPC witness earns nothing");

		uid = "test-rep-events-officer";
		LF_EventBus.Publish(LF_OnVehicleRecovered.Create(uid, "test-rep-events-owner", "veh-1", "AB 123"));
		ctx.Equal(0, LF_Reputation.Points("test-rep-events-owner", POLICE), "the owner of a recovered car earns nothing");
		LF_Reputation.Forget("test-rep-events-owner");
		Expect(ctx, uid, 0, 11, 0, "a recovered car raises the officer's police standing");

		uid = "test-rep-events-logout";
		LF_EventBus.Publish(LF_OnFlaggedLogout.Create(uid, "cuffed,wanted2", "jail min=300s", vector.Zero, 300));
		Expect(ctx, uid, 0, -13, 0, "a flagged logout costs police standing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunUnderworld(LF_TestContext ctx)
	{
		string uid = "test-rep-events-fence";
		LF_EventBus.Publish(LF_OnFenceSale.Create(uid, "{GUID}Prefabs/Tools/LF_TestTool.et", 1, 90));
		Expect(ctx, uid, 0, 0, 6, "a fence sale raises the underworld");

		uid = "test-rep-events-chop";
		LF_EventBus.Publish(LF_OnVehicleChopped.Create(uid, "shop-1", "veh-2", "CD 456", "", 400, 4));
		Expect(ctx, uid, 0, 0, 12, "a chopped car raises the underworld");

		uid = "test-rep-events-chain";
		LF_EventBus.Publish(LF_OnChainStep.Create(uid, "site-1", "chain-1", "step-1", "station-1", "load", 0.5, vector.Zero));
		LF_EventBus.Publish(LF_OnChainStep.Create(uid, "site-1", "chain-1", "step-1", "station-1", "ready", 0.5, vector.Zero));
		Expect(ctx, uid, 0, 0, 0, "loading and finishing a batch is not yet work paid out");
		LF_EventBus.Publish(LF_OnChainStep.Create(uid, "site-1", "chain-1", "step-1", "station-1", "collect", 0.5, vector.Zero));
		Expect(ctx, uid, 0, 0, 3, "collecting the product raises the underworld");

		uid = "test-rep-events-heist";
		LF_EventBus.Publish(LF_OnHeistLoot.Create(uid, "van-1", "bags", 5000, ""));
		Expect(ctx, uid, 0, 0, 16, "heist loot raises the underworld");
	}

	//------------------------------------------------------------------------------------------------
	//! Loss needs proof: a wanted level that changes or one that falls back costs nothing.
	protected void RunNoLoss(LF_TestContext ctx)
	{
		string uid = "test-rep-events-wanted";
		LF_EventBus.Publish(LF_OnWantedChanged.Create(uid, 0, 3));
		LF_EventBus.Publish(LF_OnWantedChanged.Create(uid, 3, 0));
		Expect(ctx, uid, 0, 0, 0, "a wanted level changing moves no standing");
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config: a row of the right faction and sign for every action the subscriber awards.
	protected void RunShippedRows(LF_TestContext ctx)
	{
		LF_ReputationConfig.Reload();
		LF_ReputationConfig shipped = LF_ReputationConfig.Get();
		ctx.NotNull(shipped, "the shipped reputation config loads");
		if (!shipped)
			return;

		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_JOB_COMPLETED, TOWN, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_ARREST_TOWN, TOWN, -1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_ARREST_POLICE, POLICE, -1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_ARREST_UNDERWORLD, UNDERWORLD, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_SUSPECT_IDENTIFIED, TOWN, -1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_WITNESS_STATEMENT, POLICE, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_WITNESS_UNDERWORLD, UNDERWORLD, -1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_VEHICLE_RECOVERED, POLICE, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_FLAGGED_LOGOUT, POLICE, -1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_FENCE_SALE, UNDERWORLD, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_VEHICLE_CHOPPED, UNDERWORLD, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_CHAIN_STEP, UNDERWORLD, 1);
		ExpectRow(ctx, shipped, LF_ReputationSubscriber.ACTION_HEIST_LOOT, UNDERWORLD, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectRow(LF_TestContext ctx, LF_ReputationConfig config, string action, string faction, int sign)
	{
		LF_ReputationAwardDef row = config.FindAward(action);
		ctx.NotNull(row, "the shipped config has a row for " + action);
		if (!row)
			return;

		ctx.True(row.m_sFaction == faction, action + " feeds " + faction);
		if (sign > 0)
			ctx.True(row.m_iPoints > 0, action + " gains standing");
		else
			ctx.True(row.m_iPoints < 0, action + " costs standing");
	}
}
