// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_HiveRestart`: RED-OK farming/hive-restart; RedProof() skips the first load, so "the load gives the hive its three combs back (expected 3, got 0)" fails.
//! A beehive rides the real save path (#3447, the "hives" save section): a placed hive with three combs
//! is saved, emptied as a fresh boot would be, loaded with three combs again (twice, same answer), and
//! taking gives three honeycomb items once. A saved hive whose entity is not there waits for it, and a
//! save without the section loads no hive. Across a restart (`tools\cli wt e2e`): every non-reload run
//! leaves a waiting three-comb hive row; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_HiveRestart : LF_Test
{
	protected static const ResourceName HIVE_PREFAB = "{D4E1B8A27C3F5096}Prefabs/Props/Farming/LF_Beehive.et";
	//! A key no placed hive uses, so a row for it waits.
	protected static const string KEY_E2E = "hive:lf_test_e2e";

	override string GetName()
	{
		return "farming/hive-restart";
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
		if (LF_E2E.RequiresSection(LF_HiveSavePayload.NAME))
		{
			ctx.Equal(3, LF_Hives.PendingCombs(KEY_E2E), "the e2e hive comes back from the restart with three combs");
			LF_Debug.Info(LF_Hives.FEATURE, string.Format("e2e restart: %1 holds %2 comb(s)", KEY_E2E, LF_Hives.PendingCombs(KEY_E2E)));
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_HiveSavePayload.NAME), "hives is a registered save section");
		LF_HiveConfig config = LF_HiveConfig.Get();
		ctx.NotNull(config, "the hive config loads");
		if (!config)
			return;

		LF_Hives.ResetStatics();
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		if (!user)
			return;

		BaseWorld world = GetGame().GetWorld();
		vector at = user.GetOrigin() + Vector(1.5, 0, 0);
		at[1] = world.GetSurfaceY(at[0], at[2]);
		IEntity hiveEntity = LF_Utils.SpawnEntityPrefab(HIVE_PREFAB, at);
		ctx.NotNull(hiveEntity, "the beehive spawns");
		LF_HiveComponent hive;
		if (hiveEntity)
			hive = LF_Component<LF_HiveComponent>.Find(hiveEntity);

		ctx.NotNull(hive, "the beehive carries LF_HiveComponent");
		if (!hive)
		{
			LF_TestCharacterFixture.Delete(hiveEntity);
			LF_TestCharacterFixture.Delete(user);
			return;
		}

		RunHive(ctx, persistence, config, user, hive);

		LF_TestCharacterFixture.Delete(hiveEntity);
		LF_TestCharacterFixture.Delete(user);
		ctx.Equal(0, LF_Hives.Live().Count(), "a deleted hive leaves the live table");

		// A save without the section (an older save): no hive waits.
		LF_Hives.ResetStatics();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_HiveSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_HiveSavePayload.NAME), "the older save has no hives section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_Hives.ExportAll().Count(), "a pre-section save leaves no hive");

		// Every non-reload run (the e2e save boot among them) leaves a waiting three-comb hive.
		LF_Hives.ResetStatics();
		array<ref LF_HiveRow> seed = {};
		seed.Insert(LF_HiveRow.Create(KEY_E2E, 3, 30));
		LF_Hives.ApplyAll(seed);
		ctx.Equal(3, LF_Hives.PendingCombs(KEY_E2E), "the e2e hive waits with three combs before the save");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHive(LF_TestContext ctx, LF_PersistenceComponent persistence, LF_HiveConfig config, IEntity user, LF_HiveComponent hive)
	{
		ctx.False(hive.GetHiveKey().IsEmpty(), "a placed hive gets a key from its position");
		ctx.True(LF_Hives.Find(hive.GetHiveKey()) == hive, "the placed hive registers");

		// Three combs and an hour toward the fourth, worked at noon in fair weather.
		ctx.Equal(0, LF_Hives.AdvanceAll(config.m_fHoursPerComb * 3, true, 12), "rain fills nothing");
		ctx.Equal(3, LF_Hives.AdvanceAll(config.m_fHoursPerComb * 3 + 1, false, 12), "fair hours fill three combs");
		ctx.Equal(3, hive.GetCombs(), "the hive holds three combs");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_HiveSavePayload.NAME), "the save carries the hives section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_HiveSavePayload.NAME), "hives is not a legacy positional payload");

		// A fresh boot: the hive is empty until the save is read.
		array<ref LF_HiveRow> none = {};
		LF_Hives.ApplyAll(none);
		ctx.Equal(0, hive.GetCombs(), "a fresh boot's hive is empty");

		// RedProof(): the load is skipped, so the read-back assertions fail (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.Equal(3, hive.GetCombs(), "the load gives the hive its three combs back");
		ctx.True(Math.AbsFloat(hive.GetProgressHours() - 1) < 0.05, string.Format("the load keeps the hour toward the next comb (%1)", hive.GetProgressHours()));
		ReadSection(saved, ctx);
		ctx.Equal(3, hive.GetCombs(), "a second load gives the same three combs");

		// Take: three honeycomb once, then nothing.
		ResourceName comb = hive.GetHoneycombPrefab();
		int before = LF_InventoryUtils.GetAmount(user, comb);
		int given;
		ctx.True(hive.ServerTake(user, given) == LF_EHiveTake.OK, "taking from a full hive works");
		ctx.Equal(3, given, "taking gives three honeycomb");
		ctx.Equal(before + 3, LF_InventoryUtils.GetAmount(user, comb), "the three honeycomb are in the player's inventory");
		ctx.Equal(0, hive.GetCombs(), "the hive is empty after the take");
		int again;
		ctx.True(hive.ServerTake(user, again) == LF_EHiveTake.EMPTY, "a second take is refused as empty");
		ctx.Equal(0, again, "a second take gives nothing");
		ctx.Equal(before + 3, LF_InventoryUtils.GetAmount(user, comb), "the second take adds no honeycomb");
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the hives section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_HiveSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_HiveSavePayload.NAME), "the hives section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the hives section reads");
		context.EndObject();
	}
}
