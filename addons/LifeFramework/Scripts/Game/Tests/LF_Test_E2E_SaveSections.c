// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_SaveSections,LF_Test_E2E_SaveSections`: RED-OK e2e/save-sections, 48 assertions (with the sections left out of the store every section read fails). Green with them (--only).
//! Save sections through the real store (#2841). In any run: positional saves (version 32, version 55
//! and the last positional one) load clean with every section absent, and the current store, written
//! and read through the engine's binary save context and through JSON, carries its sections. In the
//! e2e save boot the fixture sections are switched on so the shutdown save carries them; the reload
//! boot finds them read back by the restart (`wt e2e`).
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_E2E_SaveSections : LF_Test
{
	static const int E2E_A = 4242;
	static const float E2E_B = 77;

	protected ref BinaryDataContainer m_Data;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/save-sections";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		CheckRestart(ctx);
		return;
#endif

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		// A full load re-applies every payload: keep what the tests after this one found.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();

		CheckPositional(ctx, persistence, 32);
		CheckPositional(ctx, persistence, 55);
		CheckPositional(ctx, persistence, LF_TestSaveVersion.LEGACY);
		CheckCurrent(ctx, persistence, false);
		CheckCurrent(ctx, persistence, true);

		LF_TestSaveSectionFixtureA.Reset();
		LF_TestSaveSectionFixtureB.Reset();
		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		LF_SaveStamp.ResetForLoad();
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		kept.Restore();

#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries both fixture sections.
		LF_TestSaveSectionFixtureA.s_bActive = true;
		LF_TestSaveSectionFixtureA.s_iValue = E2E_A;
		LF_TestSaveSectionFixtureB.s_bActive = true;
		LF_TestSaveSectionFixtureB.s_fValue = E2E_B;
		ctx.Pass("the save boot switched the fixture sections on for the shutdown save");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! The reload boot: the restart read the sections the save boot wrote.
	protected void CheckRestart(LF_TestContext ctx)
	{
#ifdef LF_E2E_PRE_MOVE
		ctx.False(LF_SaveSections.BootHadSection(LF_TestSaveSectionFixtureA.NAME), "e2e the pre-move save has no sections");
		ctx.True(LF_TestSaveSectionFixtureA.s_bAbsent, "e2e the pre-move save marks every section absent");
		return;
#endif
#ifdef LF_E2E_LEGACY_SAVE
		ctx.False(LF_SaveSections.BootHadSection(LF_TestSaveSectionFixtureA.NAME), "e2e the version 23 save has no sections");
		ctx.True(LF_TestSaveSectionFixtureA.s_bAbsent, "e2e the version 23 save marks every section absent");
		return;
#endif
		ctx.True(LF_SaveSections.BootHadSection(LF_TestSaveSectionFixtureA.NAME), "e2e the restart read section A");
		ctx.True(LF_SaveSections.BootHadSection(LF_TestSaveSectionFixtureB.NAME), "e2e the restart read section B");
		ctx.Equal(E2E_A, LF_TestSaveSectionFixtureA.s_iRead, "e2e section A keeps its value across the restart");
		ctx.True(LF_TestSaveSectionFixtureB.s_fRead == E2E_B, "e2e section B keeps its value across the restart");
		ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "e2e the sectioned restart load is not damaged");
	}

	//------------------------------------------------------------------------------------------------
	//! A positional save loads as before: not damaged, every section absent, read twice the same.
	protected void CheckPositional(LF_TestContext ctx, notnull LF_PersistenceComponent persistence, int version)
	{
		ArmFixtures();
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, version);
		string saved = saveContext.SaveToString();
		ctx.False(saved.Contains(LF_SaveSections.OBJECT_PREFIX), string.Format("the version %1 save has no sections", version));

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (version %1 load %2)", version, pass);
			LF_TestSaveSectionFixtureA.s_bAbsent = false;
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the positional save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
			ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the positional save loads clean" + tag);
			ctx.True(LF_TestSaveSectionFixtureA.s_bAbsent, "every section is absent from a positional save" + tag);
			ctx.False(LF_SaveSections.WasLoaded(LF_TestSaveSectionFixtureA.NAME), "no section is recorded as loaded" + tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The current store through the real write and read, in the context a real save uses.
	protected void CheckCurrent(LF_TestContext ctx, notnull LF_PersistenceComponent persistence, bool binary)
	{
		string tag = " (json)";
		if (binary)
			tag = " (binary)";

		ArmFixtures();
		SaveContext save;
		if (binary)
			save = new SCR_PersistenceBinarySaveContext();
		else
			save = new JsonSaveContext(false);

		if (!RedProof())
			LF_PersistenceComponentSerializer.SerializeStore(persistence, save);
		else
			LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, save, LF_TestSaveVersion.LEGACY);

		LoadContext load = Reopen(save, binary);
		ctx.True(load != null, "the current store reopens" + tag);
		if (!load)
			return;

		LF_PersistenceComponentSerializer.DeserializeStore(persistence, load);
		ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the current store loads clean" + tag);
		ctx.True(LF_SaveSections.WasLoaded(LF_TestSaveSectionFixtureA.NAME), "the store read applied section A" + tag);
		ctx.Equal(123, LF_TestSaveSectionFixtureA.s_iRead, "section A reads back through the store" + tag);
		ctx.True(LF_TestSaveSectionFixtureB.s_fRead == 456, "section B reads back through the store" + tag);
		ctx.Equal(LF_TestSaveVersion.CURRENT, LF_SaveStamp.LoadedSaveVersion(), "the build stamp names the sectioned format" + tag);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ArmFixtures()
	{
		LF_TestSaveSectionFixtureA.Reset();
		LF_TestSaveSectionFixtureB.Reset();
		LF_TestSaveSectionFixtureA.s_bActive = true;
		LF_TestSaveSectionFixtureA.s_iValue = 123;
		LF_TestSaveSectionFixtureB.s_bActive = true;
		LF_TestSaveSectionFixtureB.s_fValue = 456;
	}

	//------------------------------------------------------------------------------------------------
	protected LoadContext Reopen(SaveContext save, bool binary)
	{
		if (binary)
		{
			SCR_PersistenceBinarySaveContext binarySave = SCR_PersistenceBinarySaveContext.Cast(save);
			m_Data = binarySave.SaveToContainer();
			if (!m_Data)
				return null;

			SCR_PersistenceBinaryLoadContext binaryLoad = new SCR_PersistenceBinaryLoadContext();
			if (!binaryLoad.LoadFromContainer(m_Data))
				return null;

			return binaryLoad;
		}

		JsonSaveContext jsonSave = JsonSaveContext.Cast(save);
		JsonLoadContext jsonLoad = new JsonLoadContext(false);
		if (!jsonLoad.LoadFromString(jsonSave.SaveToString()))
			return null;

		return jsonLoad;
	}
}
