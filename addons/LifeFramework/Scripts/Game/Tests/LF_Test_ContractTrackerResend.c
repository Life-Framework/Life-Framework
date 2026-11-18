// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ContractTrackerResend`: RED PROOF OK, first `the re-sent line carries the limit less the elapsed time, not the full limit (expected 1680, got 0)`. Green with `--only`: passed=4 failed=0 beside LF_Test_ContractTracker and LF_Test_ReputationContract.
// Review of #3162, observed 2026-11-18 with `--only LF_Test_ContractTrackerResend` on the subscriber that keyed by the account uid: `- a spawn on slot 2 re-sends slot 2's contract line (expected '01a1efb8-...|test_parcel|1800|1|1680', got '')`. Fixed (ResolveCharacterKey); green.
//! #3144: a player who spawns again (a reconnect, a restart of their game) while their contract runs
//! is sent the tracker line again, naming the contract and carrying the seconds actually left, not
//! the full limit. A spawn with no contract running sends nothing. The client counts on from there.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ContractTrackerResend : LF_Test
{
	protected static const string UID = "contract-resend-uid";
	protected static const string CONTRACT = "test_parcel";
	protected static const int PLAYER_ID = 9431;	// no real player: the send is journalled, not delivered
	protected static const int ELAPSED = 120;
	protected static const string ACCOUNT = "contract-resend-account";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	override string GetName()
	{
		return "reputation/contract-tracker-resend";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aGates.Insert(LF_ReputationGateDef.Make("contract:" + CONTRACT, "TOWN", 1));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_CivilianJob.AbandonRun(UID);
		LF_CivilianJob.ForgetCareer(UID);
		LF_Reputation.Forget(UID);
		LF_ReputationContracts.Forget(UID);
		LF_ContractTracker.ClearJournal();
	}

	//------------------------------------------------------------------------------------------------
	protected void Spawned()
	{
		if (!RedProof())
			LF_EventBus.Publish(LF_OnPlayerSpawnedEvent.Create(PLAYER_ID, UID, false));
	}

	//------------------------------------------------------------------------------------------------
	protected int JournalCount()
	{
		array<string> journal = {};
		LF_ContractTracker.GetJournal(journal);
		return journal.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected string LastSend()
	{
		array<string> journal = {};
		LF_ContractTracker.GetJournal(journal);
		if (journal.IsEmpty())
			return string.Empty;

		return journal[journal.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The wire: a fourth field carries the seconds left; a three-field line has the full limit.
		string id;
		int limit;
		bool stake;
		int left;
		ctx.True(LF_ContractTracker.DecodeLeft(LF_ContractTracker.Encode("harbour", 1800, true, 600), id, limit, stake, left), "a re-sent line decodes");
		ctx.Equal(1800, limit, "the limit round trips");
		ctx.Equal(600, left, "the seconds left round trip");
		ctx.True(LF_ContractTracker.DecodeLeft(LF_ContractTracker.Encode("harbour", 1800, true), id, limit, stake, left), "a first line decodes");
		ctx.Equal(1800, left, "a first line has the full limit left");

		// The client counts on from the time left: 600 of 1000 s left is soon, not plenty.
		LF_ContractTracker.Receive(LF_ContractTracker.Encode("harbour", 1000, true, 400), 0);
		ctx.Equal(LF_EContractTimeWord.SOON, LF_ContractTracker.ClientTimeWord(0), "a re-sent line with 400 of 1000 s left reads soon");
		ctx.Equal(LF_EContractTimeWord.EXPIRED, LF_ContractTracker.ClientTimeWord(400000), "and its time runs out 400 s later");
		LF_ContractTracker.ClearClient();
		LF_ContractTrackerHud.Remove();

		// The server path: a spawn while the contract runs re-sends it with the time left.
		LF_ContractTrackerSubscriber.Install();
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.Reload();
		LF_JobsConfig.Reload();
		Clean();
		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(CONTRACT);
		ctx.NotNull(def, "the core ships the test_parcel contract row");
		if (!def || def.m_fTimeLimitSeconds <= ELAPSED)
		{
			ctx.True(def && def.m_fTimeLimitSeconds > ELAPSED, "the test contract has a time limit over the elapsed time");
			Finish();
			return;
		}

		// No contract running: a spawn sends nothing.
		Spawned();
		ctx.Equal(0, JournalCount(), "a spawn with no contract running sends nothing");

		LF_Reputation.Add(UID, "TOWN", 100, "test");
		LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL);
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a parcel run starts");
		// Taken ELAPSED seconds ago on the contract clock.
		ctx.True(LF_ReputationContracts.Take(UID, CONTRACT, LF_JobGuards.Now() - ELAPSED), "the contract binds");
		LF_ContractTracker.ClearJournal();

		Spawned();
		int fullLimit = Math.Round(def.m_fTimeLimitSeconds);
		string expected = UID + "|" + LF_ContractTracker.Encode(CONTRACT, fullLimit, def.m_iStakePoints > 0, fullLimit - ELAPSED);
		ctx.EqualStr(expected, LastSend(), "the spawn re-sends the contract line with the time left");
		ctx.Equal(1, JournalCount(), "one spawn is one send");

		string payload = string.Empty;
		string last = LastSend();
		if (last.Length() > UID.Length())
			payload = last.Substring(UID.Length() + 1, last.Length() - UID.Length() - 1); // lf-bounded: a short journal entry

		ctx.True(LF_ContractTracker.DecodeLeft(payload, id, limit, stake, left), "the re-sent payload is a line");
		ctx.EqualStr(CONTRACT, id, "the re-sent line names the contract");
		ctx.Equal(fullLimit - ELAPSED, left, "the re-sent line carries the limit less the elapsed time, not the full limit");
		Clean();

		// Review of #3162: contracts are keyed by the character's record key; the event carries the
		// account uid. A player on their second character (keyed by its own id) still gets the line.
		SlotTwo(ctx, def, fullLimit);
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void SlotTwo(LF_TestContext ctx, LF_ReputationContractDef def, int fullLimit)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		ctx.NotNull(accounts, "the account manager is up");
		if (!accounts)
			return;

		LF_CharacterConfig.SetMaxPerAccountOverride(2);
		LF_PlayerAccount account = LF_PlayerAccount.Create(ACCOUNT);
		accounts.AddToCache(account);
		account.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Ada", "Lovelace", 36), true);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Builder", 40);
		account.AddCharacter(second, true);
		string key = second.GetRecordKey();
		ctx.False(key == ACCOUNT, "slot 2 has its own record key");

		LF_Reputation.Add(key, "TOWN", 100, "test");
		LF_CivilianJob.SignUp(key, LF_EJobType.PARCEL);
		ctx.True(LF_CivilianJob.StartRun(key, LF_EJobType.PARCEL), "slot 2's parcel run starts");
		ctx.True(LF_ReputationContracts.Take(key, CONTRACT, LF_JobGuards.Now() - ELAPSED), "slot 2's contract binds");
		LF_ContractTracker.ClearJournal();

		LF_EventBus.Publish(LF_OnPlayerSpawnedEvent.Create(PLAYER_ID, ACCOUNT, false));
		string expected = key + "|" + LF_ContractTracker.Encode(CONTRACT, fullLimit, def.m_iStakePoints > 0, fullLimit - ELAPSED);
		ctx.EqualStr(expected, LastSend(), "a spawn on slot 2 re-sends slot 2's contract line");

		LF_CivilianJob.AbandonRun(key);
		LF_CivilianJob.ForgetCareer(key);
		LF_Reputation.Forget(key);
		LF_ReputationContracts.Forget(key);
		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		accounts.RemoveFromCache(ACCOUNT);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		Clean();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
	}
}
