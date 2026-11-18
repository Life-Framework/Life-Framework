// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PlantHarvest,LF_Test_PlantPersist`: `RED PROOF OK`, the store read skipped failed `two plants after the load (load 2) (expected 2, got 0)`. Green with the read. Review (2026-11-18): with ApplyAll dropping rows when no catalogue loads, `--only LF_Test_PlantPersist` failed `with no catalogue every saved plant is kept (expected 2, got 0)`; green with the keep.
//! farming/plant-persist (#2664): the "plants" payload round trips rows {plantId, cropId, position,
//! plantedAt, wateredAt, ownerKey} through the real store write and read, positions set on the ground;
//! a second load and a second apply give no duplicate; a save from before this payload loads no plants.
//! Across a restart (`tools\cli wt e2e`): the save boot plants one crop, the reload boot finds it with
//! its clocks.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_PlantPersist : LF_Test
{
	//! The core catalogue's neutral crop, so the reload boot (no test catalogue) keeps the row.
	protected static const string CROP = "example_crop";
	protected static const string E2E_ID = "plant-e2e";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(LF_CropCatalogue.Find(CROP), "the core catalogue has the example crop");
		if (!persistence)
			return;

		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion("plants");
#ifdef LF_E2E_LOAD_BOOT
		LF_PlantRow back = LF_Plants.Find(E2E_ID);
		if (!LF_E2E.SaveHas("plants"))
		{
			// The guard's pre-move e2e writes pass 1 with the build before this payload (#2812).
			LF_Debug.Info(LF_Plants.FEATURE, string.Format("e2e restart: boot save version %1 predates plants (%2), skipping the plant check", LF_SaveStamp.BootSaveVersion(), since));
			ctx.True(!back, "a save older than the plants payload loads no plant");
			return;
		}

		ctx.NotNull(back, "the plant is back after the restart");
		if (back)
			ctx.Equal(-500, back.m_iPlantedAt - LF_Plants.Now(), "the plant keeps its age after the restart");
		return;
#endif

		ctx.True(since > 0, "plants is a registered payload");
		if (since <= 1)
			return;

		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		RoundTrip(ctx, persistence, since);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);

#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one plant, planted 500 s ago and watered 100 s ago.
		array<ref LF_PlantRow> e2eRows = LF_Plants.ExportAll();
		e2eRows.Insert(Row(E2E_ID, "2000 0 2000", -500, -100, "plant-e2e-owner"));
		LF_Plants.ApplyAll(e2eRows);
		ctx.NotNull(LF_Plants.Find(E2E_ID), "the save boot stored the e2e plant");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlantRow Row(string id, vector position, int plantedAt, int wateredAt, string owner)
	{
		LF_PlantRow row = new LF_PlantRow();
		row.m_sPlantId = id;
		row.m_sCropId = CROP;
		row.m_vPosition = position;
		row.m_iPlantedAt = plantedAt;
		row.m_iWateredAt = wateredAt;
		row.m_sOwnerKey = owner;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence, int since)
	{
		LF_Plants.ResetStatics();
		array<ref LF_PlantRow> rows = {};
		rows.Insert(Row("plant-persist-a", "1000 500 1000", -300, -120, "plant-persist-owner-a"));
		rows.Insert(Row("plant-persist-b", "1003 0 1000", -50, -50, "plant-persist-owner-b"));
		rows.Insert(Row("plant-persist-gone", "1006 0 1000", -10, -10, "plant-persist-owner-b"));
		rows[2].m_sCropId = "plant-persist-no-such-crop";
		ctx.Equal(2, LF_Plants.ApplyAll(rows), "two plants before the save, the unknown crop dropped");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("plant-persist-a"), "the save carries the plants");

		vector groundSpot = LF_Plants.SnapToGround("1000 500 1000");
		float ground = groundSpot[1];
		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live plants empty before each load, so only the save can bring them back.
			LF_Plants.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			ctx.Equal(2, LF_Plants.Count(), "two plants after the load" + tag);
			LF_PlantRow a = LF_Plants.Find("plant-persist-a");
			ctx.NotNull(a, "plant a is back" + tag);
			if (!a)
				continue;

			ctx.EqualStr(CROP, a.m_sCropId, "a keeps its crop" + tag);
			ctx.EqualStr("plant-persist-owner-a", a.m_sOwnerKey, "a keeps its owner" + tag);
			ctx.Equal(-300, a.m_iPlantedAt - LF_Plants.Now(), "a keeps its planted-at" + tag);
			ctx.Equal(-120, a.m_iWateredAt - LF_Plants.Now(), "a keeps its watered-at" + tag);
			ctx.True(Math.AbsFloat(a.m_vPosition[0] - 1000) < 0.01 && Math.AbsFloat(a.m_vPosition[2] - 1000) < 0.01, "a keeps its place" + tag);
			ctx.True(Math.AbsFloat(a.m_vPosition[1] - ground) < 0.01, "a stands on the ground, not where it was saved" + tag);
			ctx.Equal(1, LF_Plants.CountByOwner("plant-persist-owner-b"), "b's one plant is back" + tag);
		}

		// A second apply of the same rows on a live table makes no duplicate.
		LF_Plants.ApplyAll(LF_Plants.ExportAll());
		ctx.Equal(2, LF_Plants.Count(), "a second apply keeps two plants");

		// A crop catalogue that failed to load keeps every saved row, so the next save does not lose them.
		LF_Plants.ResetStatics();
		LF_Plants.SimulateNoCatalogue(true);
		array<ref LF_PlantRow> unchecked = {};
		unchecked.Insert(Row("plant-persist-nocat-a", "1000 0 1000", -30, -30, "plant-persist-owner-a"));
		unchecked.Insert(Row("plant-persist-nocat-b", "1003 0 1000", -30, -30, "plant-persist-owner-b"));
		unchecked[1].m_sCropId = "plant-persist-no-such-crop";
		ctx.Equal(2, LF_Plants.ApplyAll(unchecked), "with no catalogue every saved plant is kept");
		ctx.Equal(2, LF_Plants.ExportAll().Count(), "with no catalogue the next save still carries them");
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(rows);

		// A save from the version before this payload holds no plants.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, since - 1);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains("plant-persist-a"), "the old save has no plant rows");
		LF_Plants.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_Plants.Count(), "a pre-payload save loads no plants");
	}
}
