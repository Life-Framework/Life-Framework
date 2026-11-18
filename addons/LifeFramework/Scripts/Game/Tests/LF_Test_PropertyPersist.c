// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_PropertyRecord,LF_Test_PropertyPersist`: `RED-OK property/persist` with the store read skipped, `- the plot is back (load 1)`, `- one property row after the load (load 1) (expected 1, got 0)`. Green with the read.
//! property/persist (#2646): the "properties" payload round trips a property row through the real
//! store write and read, a second load of the same save is idempotent, and a save from the version
//! before this payload loads with no property rows. Across a restart (`tools\cli wt e2e`): the save
//! boot stores a plot; the reload boot finds it. The test puts back the book it found.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_PropertyPersist : LF_Test
{
	protected static const string PLOT = "property-persist-plot";
	protected static const string OWNER = "property-persist-owner";
	protected static const string E2E_PLOT = "property-e2e-plot";
	protected static const string E2E_OWNER = "property-e2e-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/persist";
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

		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion("properties");
#ifdef LF_E2E_LOAD_BOOT
		// --legacy-save and --pre-move load a save older than the "properties" payload: it has no rows.
		if (!LF_E2E.SaveHas("properties"))
		{
			ctx.True(LF_Properties.Get(E2E_PLOT) == null, "a save older than the properties payload loads no property row");
			return;
		}

		LF_PropertyRecord restored = LF_Properties.Get(E2E_PLOT);
		ctx.NotNull(restored, "the plot row is back after the restart");
		if (restored)
		{
			ctx.Equal(3, restored.m_iBuildStage, "the plot keeps its build stage after the restart");
			ctx.EqualStr(E2E_OWNER, restored.m_sOwnerKey, "the plot keeps its owner after the restart");
		}

		// The row is this test's own; it is not saved again.
		LF_Properties.ApplyAll(null);
		return;
#endif

		ctx.True(since > 0, "properties is a registered payload");
		if (since <= 1)
			return;

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);
		LF_Properties.Create(PLOT);
		LF_Properties.SetStage(PLOT, 4);
		LF_Properties.SetOwner(PLOT, OWNER);
		LF_Properties.Get(PLOT).m_sDeposited = "planks=5";

		// Round trip through the real store write and read, loaded twice.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(PLOT), "the save carries the plot row");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live book changes before each load, so only the save can bring the row back.
			LF_Properties.ApplyAll(null);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_PropertyRecord loaded = LF_Properties.Get(PLOT);
			ctx.NotNull(loaded, "the plot is back" + tag);
			if (loaded)
			{
				ctx.Equal(4, loaded.m_iBuildStage, "the plot keeps its stage" + tag);
				ctx.EqualStr(OWNER, loaded.m_sOwnerKey, "the plot keeps its owner" + tag);
				ctx.EqualStr("planks=5", loaded.m_sDeposited, "the plot keeps its deposits" + tag);
			}

			ctx.Equal(1, LF_Properties.ExportAll().Count(), "one property row after the load" + tag);
		}

		// A save from the version before this payload holds no property rows.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, since - 1);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(PLOT), "the old save has no property rows");
		LF_Properties.ApplyAll(null);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_Properties.ExportAll().Count(), "the old save loads with no property rows");

		LF_Properties.ApplyAll(before);
		kept.Restore();
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one plot at stage 3 with an owner.
		LF_Properties.Create(E2E_PLOT);
		LF_Properties.SetStage(E2E_PLOT, 3);
		LF_Properties.SetOwner(E2E_PLOT, E2E_OWNER);
		ctx.NotNull(LF_Properties.Get(E2E_PLOT), "the save boot stored the e2e plot");
#endif
	}
}
