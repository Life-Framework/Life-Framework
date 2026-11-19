// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_TreeRegrowPersist`: RED PROOF OK; RedProof() skips the load and the regrow tick, so the read-back, hide-again and regrow assertions fail. The first `--only` run was green (the unwritten feature could not compile before it).
//! Felled trees grow back and ride the real save path (#3262, the "lumber" save section): a tree and a
//! synthetic key come back from a save with the seconds they had left, loading twice gives the same
//! ledger, a row whose time is up is dropped, the loaded tree is hidden again, the regrow tick stands a
//! tree whose time has come and clears its key from the table clients mirror, and a save without the
//! section loads no felled tree. Across a restart (`tools\cli wt e2e`): every non-reload run leaves one
//! tree down for E2E_SECONDS; the reload boot finds it down with time left.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_TreeRegrowPersist : LF_Test
{
	protected static const ResourceName PINE = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_2f.et";
	//! Synthetic keys in LF_TreeFelling.KeyOf's format; no map tree stands there, so a load queues them.
	protected static const string KEY_A = "50000:60000:Prefabs/Vegetation/Tree/lf_test_a.et";
	protected static const string KEY_B = "50010:60000:Prefabs/Vegetation/Tree/lf_test_b.et";
	protected static const string KEY_E2E = "50020:60000:Prefabs/Vegetation/Tree/lf_test_e2e.et";
	//! Seconds the e2e save boot leaves its tree down for: far longer than a reload boot takes.
	static const int E2E_SECONDS = 3600;
	protected static const int SPREAD_S = 3;

	override string GetName()
	{
		return "lumber/tree-regrow-persist";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ResetAll()
	{
		LF_TreeRegrow.ResetStatics();
		LF_FelledTrees.ResetStatics();
		LF_FelledTreesReplication.ResetStatics();
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

#ifdef LF_E2E_LOAD_BOOT
		// The reload boot: the e2e tree came back from the real save, when it carried the section.
		if (LF_E2E.RequiresSection(LF_LumberSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_LumberSavePayload.NAME), "lumber is a registered save section");
		ResetAll();

		// Two synthetic trees are down: one for 600 s, one for 1000 s.
		int now = LF_FelledTrees.NowS();
		LF_FelledTrees.Ledger().Fell(KEY_A, now, 600);
		LF_FelledTrees.Ledger().Fell(KEY_B, now, 1000);
		ctx.Equal(2, LF_FelledTrees.Count(), "two trees are down");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LumberSavePayload.NAME), "the save carries the lumber section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_LumberSavePayload.NAME), "lumber is not a legacy positional payload");

		// Restart: nothing is down.
		ResetAll();
		ctx.Equal(0, LF_FelledTrees.Count(), "a fresh boot has no tree down");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckDown(ctx, KEY_A, 600, "after the load");
		CheckDown(ctx, KEY_B, 1000, "after the load");
		ctx.Equal(2, LF_FelledTreesReplication.Collect().Count(), "clients are told about both trees");
		ctx.Equal(2, LF_TreeRegrow.PendingCount(), "no map tree stands at a synthetic key, so both wait for it");
		ctx.True(LF_TreeRegrow.IsTicking(), "a load with a tree down starts the regrow tick");

		// Loading twice replaces, never accumulates; a tree the save lacks stands again.
		LF_FelledTrees.Ledger().Fell("50030:60000:Prefabs/Vegetation/Tree/lf_test_extra.et", LF_FelledTrees.NowS(), 50);
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		ctx.Equal(2, LF_FelledTrees.Count(), "two loads leave the two saved trees down");
		CheckDown(ctx, KEY_A, 600, "after a second load");
		CheckDown(ctx, KEY_B, 1000, "after a second load");

		// A row whose time is up, a negative one and a bad key are dropped; a duplicate keeps the longer time.
		array<ref LF_TreeRegrowRow> rows = {};
		rows.Insert(LF_TreeRegrowRow.Create(KEY_A, 90));
		rows.Insert(LF_TreeRegrowRow.Create(KEY_A, 120));
		rows.Insert(LF_TreeRegrowRow.Create(KEY_B, 0));
		rows.Insert(LF_TreeRegrowRow.Create("50040:60000:Prefabs/Vegetation/Tree/lf_test_c.et", -5));
		rows.Insert(LF_TreeRegrowRow.Create("not a tree key", 100));
		now = LF_FelledTrees.NowS();
		ctx.Equal(1, LF_LumberSavePayload.ApplyAll(rows, now), "only the live row loads");
		CheckDown(ctx, KEY_A, 120, "from a duplicate row, the longer time");
		ctx.False(LF_FelledTrees.Ledger().IsFelled(KEY_B, now), "a row with no time left is dropped on load");
		ctx.Equal(1, LF_FelledTreesReplication.Collect().Count(), "the tree that is not in the rows stands again for the clients");

		// The regrow tick: a tree whose time has come stands, its key leaves the table and the ledger.
		ResetAll();
		now = LF_FelledTrees.NowS();
		LF_FelledTrees.Ledger().Fell(KEY_A, now, 5);
		LF_FelledTrees.Ledger().Fell(KEY_B, now, 500);
		LF_FelledTreesReplication.Publish(KEY_A);
		LF_FelledTreesReplication.Publish(KEY_B);
		ctx.Equal(0, LF_TreeRegrow.Regrow(now + 2), "a tree with time left does not grow back");
		int grown = 0;
		if (!RedProof())
			grown = LF_TreeRegrow.Regrow(now + 5 + SPREAD_S);

		ctx.Equal(1, grown, "the tree whose time came grows back");
		ctx.False(LF_FelledTrees.Ledger().IsFelled(KEY_A, now + 5 + SPREAD_S), "a regrown tree leaves the ledger");
		ctx.Equal(1, LF_FelledTrees.Count(), "the other tree is still down");
		ctx.Equal(1, LF_FelledTreesReplication.Collect().Count(), "a regrown tree is retracted from the clients' table");
		ctx.True(LF_FelledTrees.Ledger().IsFelled(KEY_B, now + 5 + SPREAD_S), "the other tree is still in the ledger");

		CheckRealTree(ctx, persistence);

		// A save without this section (an older save) on a boot: no tree is down.
		ResetAll();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_LumberSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LumberSavePayload.NAME), "the older save has no lumber section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_FelledTrees.Count(), "a pre-section save leaves no tree down");

		ResetAll();

		// Every non-reload run (the e2e save boot among them) leaves one tree down for E2E_SECONDS.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A real map tree: felled and saved, it is standing again after a fresh boot (nothing hid it), and the
	//! load of the section hides it; the tick stands it when its time has come.
	protected void CheckRealTree(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		ResetAll();
		// The fixture character is only the anchor that tells where the test ground is.
		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor)
			return;

		BaseWorld world = GetGame().GetWorld();
		vector at = anchor.GetOrigin() + Vector(8, 0, 0);
		at[1] = world.GetSurfaceY(at[0], at[2]);
		IEntity tree = LF_Utils.SpawnEntityPrefab(PINE, at);
		ctx.NotNull(tree, "the pine spawns");
		if (!tree)
		{
			LF_TestCharacterFixture.Delete(anchor);
			return;
		}

		string key = LF_FellAction.KeyOfTree(tree);
		ctx.False(key.IsEmpty(), "the tree has a ledger key");
		int now = LF_FelledTrees.NowS();
		LF_FelledTrees.Ledger().Fell(key, now, 600);
		LF_FelledTrees.Hide(tree, key);
		LF_FelledTreesReplication.Publish(key);
		ctx.True(LF_FelledTrees.IsHidden(tree), "the felled tree is hidden");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();

		// A restart: the map stands the tree again and the ledger is empty until the save is read.
		LF_TreeRegrow.ShowKey(key);
		ResetAll();
		ctx.False(LF_FelledTrees.IsHidden(tree), "after a fresh boot the tree stands");
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.True(LF_FelledTrees.IsHidden(tree), "the load hides the felled tree again");
		ctx.True(LF_FelledTrees.Ledger().IsFelled(key, LF_FelledTrees.NowS()), "the loaded tree is in the ledger");
		ctx.Equal(0, LF_TreeRegrow.PendingCount(), "a tree that is on the map is hidden at once, not queued");

		// Its time comes: the tick stands it.
		if (!RedProof())
			ctx.Equal(1, LF_TreeRegrow.Regrow(LF_FelledTrees.NowS() + 700), "the tree's time has come");

		ctx.False(LF_FelledTrees.IsHidden(tree), "a regrown tree stands again");
		ctx.Equal(0, LF_FelledTrees.Count(), "the ledger is empty after the regrow");
		ctx.Equal(0, LF_FelledTreesReplication.Collect().Count(), "the clients' table is empty after the regrow");

		ResetAll();
		LF_TestCharacterFixture.Delete(tree);
		LF_TestCharacterFixture.Delete(anchor);
	}

	//------------------------------------------------------------------------------------------------
	//! The key is down with between 1 and `seconds` left on the world clock now.
	protected static void CheckDown(LF_TestContext ctx, string key, int seconds, string when)
	{
		int now = LF_FelledTrees.NowS();
		ctx.True(LF_FelledTrees.Ledger().IsFelled(key, now), string.Format("%1 is down %2", key, when));
		int left = LF_FelledTrees.Ledger().SecondsLeft(key, now);
		ctx.True(left > seconds - SPREAD_S - 2 && left <= seconds, string.Format("%1 has about %2 s left %3 (%4)", key, seconds, when, left));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the lumber section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_LumberSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_LumberSavePayload.NAME), "the lumber section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the lumber section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one tree is down for E2E_SECONDS in the live ledger.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_FelledTrees.Ledger().Fell(KEY_E2E, LF_FelledTrees.NowS(), E2E_SECONDS);
		ctx.True(LF_FelledTrees.IsFelled(KEY_E2E), "the e2e tree is down before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same tree is down with its time left, and the clients are told.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.True(LF_FelledTrees.IsFelled(KEY_E2E), "the e2e tree is still down after the restart");
		int left = LF_FelledTrees.Ledger().SecondsLeft(KEY_E2E, LF_FelledTrees.NowS());
		ctx.True(left > 0 && left <= E2E_SECONDS, string.Format("the e2e tree keeps its time left across the restart (%1 s)", left));
		ctx.True(LF_FelledTreesReplication.Collect().Find(KEY_E2E) >= 0, "the restarted server tells clients the e2e tree is down");
		LF_Debug.Info(LF_TreeRegrow.FEATURE, string.Format("e2e restart: %1 is down with %2 s left", KEY_E2E, left));
	}
}
