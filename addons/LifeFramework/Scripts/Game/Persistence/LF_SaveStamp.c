//------------------------------------------------------------------------------------------------
//! The build stamp of a save (#980): the game build that wrote it and our save version. Persisted as
//! one row in the "buildstamp" payload (save version 32), the last payload, so it is read after
//! everything else in the same load. Plain fields only; positional, never reordered.
class LF_SaveStampRecord
{
	int m_iVersion = 1;
	//! GetGame().GetBuildVersion() of the server that wrote the save, such as "1.9.0.37".
	string m_sGameBuild;
	//! The save version the store was written at (the payload list length).
	int m_iSaveVersion;
}

//------------------------------------------------------------------------------------------------
//! Which game build wrote the loaded save, and whether that is this server's build (#980).
//! A Steam update can change how the engine reads a save (1.7.0.41 dropped every save), so a load
//! from another build is logged with both builds; the host preflight (`tools\cli host preflight`)
//! backs the save folder up and proves the load before players join. The engine's own save point
//! meta-info.json carries the build too (m_sGameVersion); that is what the tool reads before a boot.
//! Server only. Statics reset at the start of every store read, so a re-applied load is idempotent.
class LF_SaveStamp
{
	//! The build the e2e "old build" proof writes in place of the real one.
	static const string FAKE_OLD_BUILD = "1.8.0.13";
	//! Shown for a save with no stamp (written before #980).
	static const string UNKNOWN_BUILD = "unknown";

	protected static string s_sBuildOverride;
	protected static bool s_bLoaded;
	protected static string s_sLoadedBuild;
	protected static int s_iLoadedSaveVersion;

	//! The first store read of this world (the engine's boot load), kept apart from later test or
	//! re-apply reads so the e2e reload boot can check what the restart itself loaded.
	protected static bool s_bBootCaptured;
	protected static string s_sBootBuild;
	protected static int s_iBootSaveVersion;

	//------------------------------------------------------------------------------------------------
	//! World teardown (LF_StaticReset): the next world's first load is its boot load again.
	static void ResetStatics()
	{
		ResetForLoad();
		s_bBootCaptured = false;
		s_sBootBuild = string.Empty;
		s_iBootSaveVersion = 0;
		s_sBuildOverride = string.Empty;
		LF_PersistenceComponentSerializer.ClearLoadDamaged();
	}

	//------------------------------------------------------------------------------------------------
	//! Called at the end of every store read; only the first one of a world is kept.
	static void CaptureBoot()
	{
		if (s_bBootCaptured)
			return;

		s_bBootCaptured = true;
		s_sBootBuild = LoadedGameBuild();
		s_iBootSaveVersion = s_iLoadedSaveVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn The game build stamped in the first save this world loaded, UNKNOWN_BUILD without one.
	static string BootLoadedBuild()
	{
		if (!s_bBootCaptured)
			return UNKNOWN_BUILD;

		return s_sBootBuild;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn The save version stamped in the first save this world loaded, 0 without one.
	static int BootSaveVersion()
	{
		return s_iBootSaveVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The game build this server runs (a test or the e2e old-build proof can override it).
	static string CurrentGameBuild()
	{
		if (s_sBuildOverride != string.Empty)
			return s_sBuildOverride;

#ifdef LF_E2E_OLD_BUILD
		// tools\cli wt e2e <slug> --old-build: the save boot stamps an older build, as a server
		// before a Steam update would; the reload boot (without this define) runs the real one.
		return FAKE_OLD_BUILD;
#else
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return UNKNOWN_BUILD;

		string build = game.GetBuildVersion();
		if (build == string.Empty)
			return UNKNOWN_BUILD;

		return build;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Test hook: stamp and compare against this build instead of the real one. Empty clears it.
	static void SetBuildOverride(string build)
	{
		s_sBuildOverride = build;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] saveVersion The version the store is being written at.
	//! \return The one stamp row a save carries.
	static array<ref LF_SaveStampRecord> Export(int saveVersion)
	{
		LF_SaveStampRecord row = new LF_SaveStampRecord();
		row.m_sGameBuild = CurrentGameBuild();
		row.m_iSaveVersion = saveVersion;
		array<ref LF_SaveStampRecord> rows = {};
		rows.Insert(row);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the last load's stamp. Called at the start of every store read.
	static void ResetForLoad()
	{
		s_bLoaded = false;
		s_sLoadedBuild = string.Empty;
		s_iLoadedSaveVersion = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the stamp read from a save and logs the builds. Replaces any earlier stamp.
	//! \param[in] rows The "buildstamp" payload; a missing or empty one is treated as unknown.
	static void Apply(array<ref LF_SaveStampRecord> rows)
	{
		ResetForLoad();
		LF_SaveStampRecord row;
		if (rows && rows.Count() > 0)
			row = rows.Get(0);

		if (!row)
		{
			LF_Debug.Warn("Persistence", "save build stamp is empty: the build that wrote this save is unknown");
			return;
		}

		s_bLoaded = true;
		s_sLoadedBuild = row.m_sGameBuild;
		if (s_sLoadedBuild == string.Empty)
			s_sLoadedBuild = UNKNOWN_BUILD;

		s_iLoadedSaveVersion = row.m_iSaveVersion;
		if (IsFromOtherBuild())
			LF_Debug.Warn("Persistence", string.Format("save was written by game build %1 (save version %2); this server runs %3. The host preflight backs the save up before such a boot (docs/ops/game-updates.md)", s_sLoadedBuild, s_iLoadedSaveVersion, CurrentGameBuild()));
		else
			LF_Debug.Info("Persistence", string.Format("save was written by this game build %1 (save version %2)", s_sLoadedBuild, s_iLoadedSaveVersion));
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the last store read carried a stamp.
	static bool WasLoaded()
	{
		return s_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The game build that wrote the loaded save, or UNKNOWN_BUILD.
	static string LoadedGameBuild()
	{
		if (!s_bLoaded)
			return UNKNOWN_BUILD;

		return s_sLoadedBuild;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The save version the loaded stamp records, 0 without a stamp.
	static int LoadedSaveVersion()
	{
		return s_iLoadedSaveVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the loaded save carried a stamp from a different game build.
	static bool IsFromOtherBuild()
	{
		return s_bLoaded && s_sLoadedBuild != CurrentGameBuild();
	}
}
