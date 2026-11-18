// red-proof: observed 2026-11-18 with `tools/cli test --no-build --red LF_Test_ReputationSync`: RED PROOF OK (send unwired: "a town tier change queues exactly one owner send (expected 1, got 0)"). Green with `--only`. Spawn before the account (review of #2788): red on the old handler with `--only` ("a spawn before the account sends no tiers yet (expected 0, got 1)"), green after.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! The owner learns their own tiers (#2704): the packed tiers round-trip, a town or police tier change
//! journals exactly one owner send with one toast key, an underworld change one send and no toast,
//! points moving inside a tier send nothing, and no controller field holds standing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationSync : LF_Test
{
	static const string UID = "test-reputation-sync";
	//! A player id no account is cached for.
	static const int NO_ACCOUNT_PLAYER = 987654;

	override string GetName()
	{
		return "reputation/sync";
	}

	//------------------------------------------------------------------------------------------------
	static LF_ReputationConfig MakeConfig()
	{
		LF_ReputationConfig config = new LF_ReputationConfig();
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make(LF_Reputation.FACTION_TOWN, thresholds, 0));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make(LF_Reputation.FACTION_POLICE, thresholds, 0));
		LF_ReputationFactionDef underworld = LF_ReputationFactionDef.Make(LF_Reputation.FACTION_UNDERWORLD, thresholds, 0);
		underworld.m_bShown = false;
		config.m_aFactions.Insert(underworld);
		config.m_aAwards = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig.UseForTest(MakeConfig());
		LF_Reputation.Forget(UID);

		RunPacking(ctx);

		// RedProof: the send is unwired, so no tier change reaches the journal (`--red`).
		if (RedProof())
			LF_ReputationSync.Uninstall();
		else
			LF_ReputationSync.Install();

		RunSends(ctx);
		RunSpawnBeforeAccount(ctx);
		RunNoRplProp(ctx);

		LF_ReputationSync.Install();
		LF_ReputationSync.CancelSpawnRetries();
		LF_ReputationSync.ForgetClientTiers();
		LF_ReputationSync.ClearJournal();
		LF_Reputation.Forget(UID);
		LF_ReputationConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPacking(LF_TestContext ctx)
	{
		int packed = LF_ReputationSync.Pack(3, -2, 1);
		int tier;
		ctx.True(LF_ReputationSync.Unpack(packed, LF_Reputation.FACTION_TOWN, tier), "a packed value unpacks");
		ctx.Equal(3, tier, "town tier round-trips");
		LF_ReputationSync.Unpack(packed, LF_Reputation.FACTION_POLICE, tier);
		ctx.Equal(-2, tier, "police tier round-trips");
		LF_ReputationSync.Unpack(packed, LF_Reputation.FACTION_UNDERWORLD, tier);
		ctx.Equal(1, tier, "underworld tier round-trips");

		packed = LF_ReputationSync.Pack(0, 0, 0);
		LF_ReputationSync.Unpack(packed, LF_Reputation.FACTION_POLICE, tier);
		ctx.Equal(0, tier, "all-zero tiers round-trip");
		ctx.False(LF_ReputationSync.Unpack(0, LF_Reputation.FACTION_TOWN, tier), "0 is never a packed value");
		ctx.False(LF_ReputationSync.Unpack(packed, "NO_SUCH_FACTION", tier), "an unknown faction does not unpack");

		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(2, -1, 0));
		ctx.True(LF_ReputationSync.ClientTier(LF_Reputation.FACTION_TOWN, tier), "the client keeps the tiers it received");
		ctx.Equal(2, tier, "the client reads its own town tier");
		LF_ReputationSync.ClientTier(LF_Reputation.FACTION_POLICE, tier);
		ctx.Equal(-1, tier, "the client reads its own police tier");

		ctx.EqualStr("#LF-Reputation_Toast_Town_Up", LF_ReputationSync.ToastKey(LF_Reputation.FACTION_TOWN, 0, 1), "town up has its toast");
		ctx.EqualStr("#LF-Reputation_Toast_Police_Down", LF_ReputationSync.ToastKey(LF_Reputation.FACTION_POLICE, 0, -1), "police down has its toast");
		ctx.EqualStr("", LF_ReputationSync.ToastKey(LF_Reputation.FACTION_UNDERWORLD, 0, 1), "the underworld has no toast");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSends(LF_TestContext ctx)
	{
		array<string> journal = {};

		LF_ReputationSync.ClearJournal();
		LF_Reputation.Add(UID, LF_Reputation.FACTION_TOWN, 40, "test");
		LF_ReputationSync.GetJournal(journal);
		ctx.Equal(0, journal.Count(), "points inside a tier send nothing");

		LF_ReputationSync.ClearJournal();
		LF_Reputation.Add(UID, LF_Reputation.FACTION_TOWN, 100, "test");
		LF_ReputationSync.GetJournal(journal);
		ctx.Equal(1, journal.Count(), "a town tier change queues exactly one owner send");
		if (journal.Count() == 1)
		{
			string expected = string.Format("%1|%2|#LF-Reputation_Toast_Town_Up", UID, LF_ReputationSync.Pack(1, 0, 0));
			ctx.EqualStr(expected, journal[0], "the town send carries the new tiers and the town toast");
		}

		LF_ReputationSync.ClearJournal();
		LF_Reputation.Add(UID, LF_Reputation.FACTION_POLICE, -50, "test");
		LF_ReputationSync.GetJournal(journal);
		ctx.Equal(1, journal.Count(), "a police tier change queues exactly one owner send");
		if (journal.Count() == 1)
		{
			string expectedPolice = string.Format("%1|%2|#LF-Reputation_Toast_Police_Down", UID, LF_ReputationSync.Pack(1, -1, 0));
			ctx.EqualStr(expectedPolice, journal[0], "the police send carries the new tiers and the police toast");
		}

		LF_ReputationSync.ClearJournal();
		LF_Reputation.Add(UID, LF_Reputation.FACTION_UNDERWORLD, 120, "test");
		LF_ReputationSync.GetJournal(journal);
		ctx.Equal(1, journal.Count(), "an underworld tier change still sends the tiers to the owner");
		if (journal.Count() == 1)
		{
			string expectedUnder = string.Format("%1|%2|", UID, LF_ReputationSync.Pack(1, -1, 1));
			ctx.EqualStr(expectedUnder, journal[0], "the underworld send carries no toast");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A spawn whose account is not cached yet sends nothing under the account key and waits once
	//! (review of #2788: the record key is the account uid until the account loads).
	protected void RunSpawnBeforeAccount(LF_TestContext ctx)
	{
		array<string> journal = {};
		LF_ReputationSync.ClearJournal();
		LF_ReputationSync.CancelSpawnRetries();

		LF_EventBus.Publish(LF_OnPlayerSpawnedEvent.Create(NO_ACCOUNT_PLAYER, "", true));

		LF_ReputationSync.GetJournal(journal);
		ctx.Equal(0, journal.Count(), "a spawn before the account sends no tiers yet");
		ctx.Equal(1, LF_ReputationSync.PendingSpawnRetries(), "a spawn before the account waits once for it");
		LF_ReputationSync.CancelSpawnRetries();
	}

	//------------------------------------------------------------------------------------------------
	//! Standing travels only by the Owner RPC: no field on the player controller names it.
	protected void RunNoRplProp(LF_TestContext ctx)
	{
		typename controller = SCR_PlayerController;
		int found = 0;
		for (int v = 0; v < controller.GetVariableCount(); v++)
		{
			string name = controller.GetVariableName(v);
			name.ToLower();
			if (name.Contains("standing") || name.Contains("reputation"))
				found++;
		}

		ctx.Equal(0, found, "no player controller field holds standing (no RplProp)");
	}
}
