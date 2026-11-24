// red-proof: observed 2026-11-24 `tools/cli test --no-build --red LF_Test_HarnessCheckpoint`: RED PROOF OK (the apply skipped, so cash, bank, levels, licence and underworld tier read back unchanged). Green with `--only`.
//! The harness checkpoint (#4825): `checkpoint 7` puts a fresh character at hour 7 of the 10-hour arc. This test
//! applies agent-box/checkpoints/hour-7.json (the runner mirrors it to $profile:LifeFramework/checkpoints/) to a
//! fixture character and reads back cash, bank, the MINER and HAULING levels, the driving licence and the
//! underworld tier through the public accessors; the gated entry refuses outside a harness build.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HarnessCheckpoint : LF_Test
{
	static const string UID = "test-checkpoint-hour7";

	override string GetName()
	{
		return "harness/checkpoint-hour-7";
	}

	//------------------------------------------------------------------------------------------------
	//! A jobs table with the two trades the hour-7 file names: levels 2 to 10 at 100 xp steps of growing size.
	protected static LF_JobsConfig MakeConfig()
	{
		LF_JobsConfig config = new LF_JobsConfig();
		array<int> steps = {100, 300, 600, 1000, 1500, 2100, 2800, 3600, 4500};
		config.m_aTracks = {};
		config.m_aTracks.Insert(LF_JobTrackDef.Make("MINER", steps));
		config.m_aTracks.Insert(LF_JobTrackDef.Make("HAULING", steps));
		config.m_aTracks.Insert(LF_JobTrackDef.Make("FARMER", steps));
		config.m_aXpAwards = {};
		config.m_aLicenceGates = {};
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
		string json = LF_Checkpoint.ReadHour(7);
		ctx.True(!json.IsEmpty(), "agent-box/checkpoints/hour-7.json is mirrored into the test profile");
		if (json.IsEmpty())
			return;

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_JobsConfig.UseForTest(MakeConfig());
		LF_Progression.Forget(UID);

		int cashBefore = LF_MoneyUtils.GetCash(character);
		// Outside a harness build the gated entry refuses (the suite is built without LF_HARNESS) and changes nothing.
		if (!LF_DevGate.HarnessBuild())
		{
			ctx.Equal(-1, LF_Checkpoint.Apply(UID, character, json), "a build without LF_HARNESS refuses the checkpoint");
			ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(character), "the refusal left the cash alone");
		}

		// RedProof(): the checkpoint is not applied, so every read-back below fails (`--red`).
		int failed = -2;
		if (!RedProof())
			failed = LF_Checkpoint.ApplyUnchecked(UID, character, json);

		ctx.Equal(0, failed, "the hour-7 file applies with no part failed");
		ctx.Equal(300, LF_MoneyUtils.GetCash(character), "cash is the hour-7 amount");
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(UID);
		ctx.NotNull(bank, "the character has a bank account");
		if (bank)
			ctx.Equal(6200, bank.GetBalance(), "the bank holds the hour-7 amount");

		ctx.Equal(10, LF_JobLevels.LevelOf(UID, "MINER"), "MINER is level 10");
		ctx.Equal(10, LF_JobLevels.LevelOf(UID, "HAULING"), "HAULING is level 10");
		ctx.Equal(3, LF_JobLevels.LevelOf(UID, "FARMER"), "FARMER is level 3");
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(character);
		ctx.NotNull(licences, "the character carries a licence manager");
		if (licences)
		{
			ctx.True(licences.HasLicense(LF_ELicenseType.DRIVING_LICENCE), "the driving licence is held");
			ctx.True(licences.HasLicense(LF_ELicenseType.MINER_IRON), "the iron mining licence is held");
		}

		ctx.Equal(100, LF_Reputation.Points(UID, "UNDERWORLD"), "underworld standing is 100 points");
		ctx.Equal(1, LF_Reputation.Tier(UID, "UNDERWORLD"), "underworld standing is tier 1");

		// A second apply is exact, not additive: the same state, down or up.
		if (!RedProof())
		{
			LF_MoneyUtils.AddCash(character, 55, "test");
			LF_Checkpoint.ApplyUnchecked(UID, character, json);
		}

		ctx.Equal(300, LF_MoneyUtils.GetCash(character), "a second apply leaves exactly the hour-7 cash");

		// A broken file and a missing character change nothing and never abort.
		ctx.Equal(-1, LF_Checkpoint.ApplyUnchecked(UID, character, "{not json"), "a file that is not JSON is refused");
		ctx.Equal(-1, LF_Checkpoint.ApplyUnchecked(UID, null, json), "no character is refused");
		ctx.Equal(300, LF_MoneyUtils.GetCash(character), "the refusals left the cash alone");
		ctx.True(LF_Checkpoint.ReadHour(11).IsEmpty(), "an hour past 10 reads nothing");

		LF_DebugMoneyTopUp.ReleaseCash(UID);
		LF_Progression.Forget(UID);
		LF_JobsConfig.Reload();
		LF_TestCharacterFixture.Delete(character);
	}
}
