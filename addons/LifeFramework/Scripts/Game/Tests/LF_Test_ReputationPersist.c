// red-proof: observed 2026-11-18 with `tools/cli test --no-build --red LF_Test_ReputationPersist`: RED-OK reputation/persist (fails with the hook on, 23 assertions), first `TOWN points are back (expected 60, got 0)`. Green with `--only`; the restart is `tools/cli wt e2e slice-2698`.
// tier: PERSISTENCE
//! Standing rides the real save path (#2698, the "reputation" payload, save version 53): rows are
//! written, an emptied store reads the same points back, a 0 row is not written, a removed faction is
//! skipped with a warning, points clamp to the current thresholds, and loading twice gives the same
//! store. The restart itself is proved by `tools\cli wt e2e` (SeedE2E / VerifyE2E below).
class LF_Test_ReputationPersist : LF_Test
{
	static const string UID = "test-reputation-persist";
	static const string ZERO = "test-reputation-persist-zero";
	static const string HIGH = "test-reputation-persist-high";
	//! The e2e character (LF_Test_E2E_SaveState pass 1 seeds it, pass 2 reads it after the restart).
	static const string E2E_UID = "e2e-reputation";

	override string GetName()
	{
		return "reputation/persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_ReputationRow> before = LF_Reputation.ExportAll();
		LF_ReputationConfig.UseForTest(LF_Test_ReputationRules.MakeConfig());
		LF_Reputation.ApplyAll(null);
		ForgetAll();

		ctx.Equal(60, LF_Reputation.Add(UID, "TOWN", 60, "test"), "TOWN gives 60 before the save");
		ctx.Equal(-150, LF_Reputation.Add(UID, "POLICE", -150, "test"), "POLICE takes 150 before the save");
		LF_Reputation.Add(ZERO, "POLICE", 150, "test");
		LF_Reputation.Add(ZERO, "POLICE", -150, "test");
		ctx.Equal(2, LF_Reputation.ExportAll().Count(), "a 0 standing is not written (two rows)");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the character key");

		LF_Reputation.ApplyAll(null);
		ctx.Equal(0, LF_Reputation.Points(UID, "TOWN"), "the restarted session has an empty store");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the save is written at the current version (reputation is payload 53)");
		// RedProof: skip the load, so every read-back assertion fails (`--red`).
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "reputation");

		ctx.Equal(60, LF_Reputation.Points(UID, "TOWN"), "TOWN points are back");
		ctx.Equal(-150, LF_Reputation.Points(UID, "POLICE"), "POLICE points are back");
		ctx.Equal(-2, LF_Reputation.Tier(UID, "POLICE"), "the POLICE tier reads back from the thresholds");
		ctx.Equal(2, LF_Reputation.ExportAll().Count(), "the loaded store holds the two rows");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "reputation");
		ctx.Equal(2, LF_Reputation.ExportAll().Count(), "loading twice gives the same store");
		ctx.Equal(60, LF_Reputation.Points(UID, "TOWN"), "loading twice does not add points");

		array<ref LF_ReputationRow> rows = {};
		rows.Insert(LF_ReputationRow.Create(UID, "TOWN", 60));
		rows.Insert(LF_ReputationRow.Create(UID, "UNDERWORLD_REMOVED", 300));
		rows.Insert(LF_ReputationRow.Create(HIGH, "TOWN", 5000));
		rows.Insert(LF_ReputationRow.Create("", "TOWN", 10));
		ctx.Equal(2, LF_Reputation.ApplyAll(rows), "a removed faction and a row with no character are skipped");
		ctx.Equal(0, LF_Reputation.Points(UID, "UNDERWORLD_REMOVED"), "the removed faction holds nothing");
		ctx.Equal(1000, LF_Reputation.Points(HIGH, "TOWN"), "points clamp to the last threshold on load");
		ctx.Equal(0, LF_Reputation.Points(UID, "POLICE"), "the load replaced the store");

		ForgetAll();
		LF_ReputationConfig.Reload();
		LF_Reputation.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the test characters' points and session clocks (daily cap), so a second run in one boot starts clean.
	protected void ForgetAll()
	{
		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(ZERO);
		LF_Reputation.Forget(HIGH);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one character loses police standing and gains town standing.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_Reputation.Forget(E2E_UID);
		ctx.Equal(-150, LF_Reputation.Add(E2E_UID, LF_Reputation.FACTION_POLICE, -150, "e2e"), "e2e police standing drops 150 before the save");
		ctx.Equal(40, LF_Reputation.Add(E2E_UID, LF_Reputation.FACTION_TOWN, 40, "e2e"), "e2e town standing rises 40 before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same points, and so the same tiers, after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.Equal(-150, LF_Reputation.Points(E2E_UID, LF_Reputation.FACTION_POLICE), "e2e police standing survives the restart");
		ctx.Equal(40, LF_Reputation.Points(E2E_UID, LF_Reputation.FACTION_TOWN), "e2e town standing survives the restart");
		LF_ReputationFactionDef police = LF_Reputation.FactionDef(LF_Reputation.FACTION_POLICE);
		ctx.NotNull(police, "e2e the police faction is in the reputation config");
		if (police)
			ctx.Equal(police.TierFor(-150), LF_Reputation.Tier(E2E_UID, LF_Reputation.FACTION_POLICE), "e2e police tier reads back after the restart");

		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("e2e restart: %1 POLICE %2 tier %3, TOWN %4 tier %5", E2E_UID, LF_Reputation.Points(E2E_UID, LF_Reputation.FACTION_POLICE), LF_Reputation.Tier(E2E_UID, LF_Reputation.FACTION_POLICE), LF_Reputation.Points(E2E_UID, LF_Reputation.FACTION_TOWN), LF_Reputation.Tier(E2E_UID, LF_Reputation.FACTION_TOWN)));
	}
}
