// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_DisconnectCuffedPersist`: `if (false)` in place of the ImprisonOnReturn call in LF_DisconnectRules.Apply failed `- the cuffed logout is jailed` and `- the loaded save has the rejoin jailed`. Restored; green. The restart path is wt e2e.
// tier: PERSISTENCE
//! Acceptance of #691 (`disconnect/cuffed`): a cuffed player who logs out has a sentence and the
//! charge that ride the real save (the "jail" payload and the account), so after the store is
//! wiped and loaded the rejoin is still jailed with the charge. The restart itself is wt e2e.
class LF_Test_DisconnectCuffedPersist : LF_Test
{
	protected static const string UID = "disconnect-cuffed-save-uid";

	override string GetName()
	{
		return "disconnect/cuffed";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_JailRecord> before = LF_Jail.ExportAll();
		LF_Jail.SyncPlacedCell();
		ctx.True(LF_Jail.HasCell(), "DebugWorld has a placed cell");
		LF_ConfigService.Reset();
		LF_DisconnectConfig.Reload();

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		account.SetWantedLevel(1);
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = UID;
		state.m_bCuffed = true;
		state.m_iWanted = 1;
		state.m_bCellPlaced = true;
		LF_DisconnectRules.Apply(state, null);
		ctx.True(LF_Jail.IsServing(UID), "the cuffed logout is jailed");
		float remaining = LF_Jail.GetRemaining(UID);
		ctx.True(remaining >= 300, "the sentence is at least the 300 s floor");
		ctx.Equal(2, account.GetWantedLevel(), "the charge for leaving custody is on the account");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the sentence");

		// Restart: no sentence in memory.
		array<ref LF_JailRecord> none = {};
		LF_Jail.ApplyAll(none);
		ctx.False(LF_Jail.IsServing(UID), "the wiped session has no sentence");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "jail");
		ctx.True(LF_Jail.IsServing(UID), "the loaded save has the rejoin jailed");
		ctx.EqualFloat(remaining, LF_Jail.GetRemaining(UID), 0.5, "with the time left at logout");

		// Load twice converges.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "jail");
		ctx.EqualFloat(remaining, LF_Jail.GetRemaining(UID), 0.5, "a second load leaves the same sentence");

		LF_DisconnectConfig.Reload();
		LF_Jail.ApplyAll(before);
		LF_Jail.SyncPlacedCell();
	}
}
