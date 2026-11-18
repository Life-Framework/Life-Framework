// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_SaveSections,LF_Test_E2E_SaveSections`: RED-OK persist/save-sections, 71 assertions (with the sections left out of the store every section read fails). Green with them (--only).
//! Save sections (#2841): each payload in its own named object with its own version, after the
//! positional part of the store. Proved in both contexts the game uses: JSON (the tests) and the
//! engine's binary save context (SCR_PersistenceBinarySaveContext, object seeking and skippable
//! objects on), which is what a real save goes through.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveSections : LF_Test
{
	protected ref BinaryDataContainer m_Data;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/save-sections";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckRegistry(ctx);
		RunBoth(ctx, false);
		RunBoth(ctx, true);
		LF_TestSaveSectionFixtureA.Reset();
		LF_TestSaveSectionFixtureB.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBoth(LF_TestContext ctx, bool binary)
	{
		string tag = " (json)";
		if (binary)
			tag = " (binary)";

		CheckRoundTrip(ctx, binary, tag);
		CheckUnreadable(ctx, binary, tag);
		CheckFailedSectionDoesNotStopTheRest(ctx, binary, tag);
		CheckAbsent(ctx, binary, tag);
		CheckSectionVersion(ctx, binary, tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRegistry(LF_TestContext ctx)
	{
		ctx.NotNull(LF_SaveSections.Get(LF_TestSaveSectionFixtureA.NAME), "fixture section A is found by reflection");
		ctx.NotNull(LF_SaveSections.Get(LF_TestSaveSectionFixtureB.NAME), "fixture section B is found by reflection");
		ctx.True(LF_SaveSections.IsValidName("lockers_v2"), "lowercase, digits and '_' make a legal section name");
		ctx.False(LF_SaveSections.IsValidName("Bad-Name"), "capitals and '-' are not legal");
		int a = -1;
		int b = -1;
		array<ref LF_SavePayload> all = LF_SaveSections.All();
		for (int i = 0; i < all.Count(); i++)
		{
			if (all.Get(i).Name() == LF_TestSaveSectionFixtureA.NAME)
				a = i;
			if (all.Get(i).Name() == LF_TestSaveSectionFixtureB.NAME)
				b = i;
		}

		ctx.True(a >= 0 && a < b, "sections sort by (Order, Name): A before B");
	}

	//------------------------------------------------------------------------------------------------
	//! The store's shape: a version and a positional array first, then the sections.
	protected void CheckRoundTrip(LF_TestContext ctx, bool binary, string tag)
	{
		Arm(11, 22);
		SaveContext save = NewSave(binary);
		save.WriteValue("version", LF_TestSaveVersion.CURRENT);
		array<ref LF_SurvivalStatsRecord> prefixRows = {};
		LF_SurvivalStatsRecord prefixRow = new LF_SurvivalStatsRecord();
		prefixRow.m_sPersistentId = "prefix";
		prefixRows.Insert(prefixRow);
		save.Write(prefixRows);
		if (!RedProof())
			LF_SaveSections.WriteAll(save);

		LoadContext load = Reopen(save, binary);
		ctx.True(load != null, "the save reopens" + tag);
		if (!load)
			return;

		int version;
		load.ReadValue("version", version);
		ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the version reads first" + tag);
		// Read keys by the variable name (JSON), and binary checks it too: the same name as the write.
		prefixRows = {};
		ctx.True(load.Read(prefixRows) && prefixRows.Count() == 1, "the positional part reads before the sections" + tag);
		int failed;
		int unread;
		LF_SaveSections.ReadAll(load, failed, unread);
		ctx.Equal(0, failed, "no section failed" + tag);
		ctx.Equal(0, unread, "every section is readable" + tag);
		ctx.Equal(11, LF_TestSaveSectionFixtureA.s_iRead, "section A reads back" + tag);
		ctx.True(LF_TestSaveSectionFixtureB.s_fRead == 22, "section B reads back" + tag);
		ctx.True(LF_SaveSections.WasLoaded(LF_TestSaveSectionFixtureA.NAME), "the load records section A" + tag);
		ctx.False(LF_TestSaveSectionFixtureA.s_bAbsent, "a section in the save is not absent" + tag);
	}

	//------------------------------------------------------------------------------------------------
	//! A section this build does not know (a newer build, a reverted PR) and a section written at a
	//! newer version are skipped whole; the section after them still reads.
	protected void CheckUnreadable(LF_TestContext ctx, bool binary, string tag)
	{
		Arm(0, 33);
		SaveContext save = NewSave(binary);
		array<string> names = {"aaa_unknown", LF_TestSaveSectionFixtureA.NAME, LF_TestSaveSectionFixtureB.NAME};
		save.WriteValue(LF_SaveSections.MANIFEST, names);
		save.StartObject(LF_SaveSections.OBJECT_PREFIX + "aaa_unknown");
		save.WriteValue(LF_SaveSections.VERSION_KEY, 4);
		array<ref LF_SurvivalStatsRecord> unknownRows = {};
		for (int i = 0; i < 3; i++)
		{
			LF_SurvivalStatsRecord unknownRow = new LF_SurvivalStatsRecord();
			unknownRow.m_sPersistentId = "unknown-" + i;
			unknownRows.Insert(unknownRow);
		}
		save.WriteValue("rows", unknownRows);
		save.EndObject();
		save.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_TestSaveSectionFixtureA.NAME);
		save.WriteValue(LF_SaveSections.VERSION_KEY, 2);
		save.WriteValue("rows", "a shape this build cannot read");
		save.EndObject();
		save.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_TestSaveSectionFixtureB.NAME);
		save.WriteValue(LF_SaveSections.VERSION_KEY, 1);
		if (!RedProof())
			LF_TestSaveSectionFixtureB.WriteFixed(save, 33);
		save.EndObject();

		LoadContext load = Reopen(save, binary);
		ctx.True(load != null, "the save with unreadable sections reopens" + tag);
		if (!load)
			return;

		int failed;
		int unread;
		LF_SaveSections.ReadAll(load, failed, unread);
		ctx.Equal(2, unread, "the unknown section and the newer section are not read" + tag);
		ctx.Equal(-1, LF_TestSaveSectionFixtureA.s_iRead, "a newer section version leaves the live state alone" + tag);
		ctx.True(LF_TestSaveSectionFixtureB.s_fRead == 33, "the section after the skipped ones still reads" + tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFailedSectionDoesNotStopTheRest(LF_TestContext ctx, bool binary, string tag)
	{
		Arm(44, 55);
		LF_TestSaveSectionFixtureA.s_bFailRead = true;
		SaveContext save = NewSave(binary);
		if (!RedProof())
			LF_SaveSections.WriteAll(save);

		LoadContext load = Reopen(save, binary);
		if (!load)
		{
			ctx.Fail("the save reopens" + tag);
			return;
		}

		int failed;
		int unread;
		LF_SaveSections.ReadAll(load, failed, unread);
		ctx.Equal(1, failed, "the failing section counts as one failed read" + tag);
		ctx.True(LF_TestSaveSectionFixtureB.s_fRead == 55, "the section after a failed one still reads" + tag);
		ctx.False(LF_SaveSections.WasLoaded(LF_TestSaveSectionFixtureA.NAME), "a failed section is not recorded as loaded" + tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAbsent(LF_TestContext ctx, bool binary, string tag)
	{
		Arm(66, 77);
		LF_TestSaveSectionFixtureB.s_bActive = false;
		SaveContext save = NewSave(binary);
		array<string> omit = {LF_TestSaveSectionFixtureA.NAME};
		if (!RedProof())
			LF_SaveSections.WriteAll(save, omit);

		LoadContext load = Reopen(save, binary);
		if (!load)
		{
			ctx.Fail("the save reopens" + tag);
			return;
		}

		int failed;
		int unread;
		LF_SaveSections.ReadAll(load, failed, unread);
		ctx.Equal(0, failed + unread, "a save with no fixture sections reads clean" + tag);
		ctx.True(LF_TestSaveSectionFixtureA.s_bAbsent, "an omitted section gets OnAbsent" + tag);
		ctx.True(LF_TestSaveSectionFixtureB.s_bAbsent, "an inactive section is not written and gets OnAbsent" + tag);
		ctx.Equal(-1, LF_TestSaveSectionFixtureA.s_iRead, "an absent section reads nothing" + tag);
	}

	//------------------------------------------------------------------------------------------------
	//! A section raises its own version; the reader gets the saved one.
	protected void CheckSectionVersion(LF_TestContext ctx, bool binary, string tag)
	{
		Arm(88, 99);
		LF_TestSaveSectionFixtureA.s_iVersion = 3;
		SaveContext save = NewSave(binary);
		if (!RedProof())
			LF_SaveSections.WriteAll(save);

		LoadContext load = Reopen(save, binary);
		if (!load)
		{
			ctx.Fail("the save reopens" + tag);
			return;
		}

		int failed;
		int unread;
		LF_SaveSections.ReadAll(load, failed, unread);
		ctx.Equal(3, LF_TestSaveSectionFixtureA.s_iReadVersion, "the reader gets the section's saved version" + tag);
		ctx.Equal(88, LF_TestSaveSectionFixtureA.s_iRead, "a section at its own raised version reads" + tag);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Arm(int a, float b)
	{
		LF_TestSaveSectionFixtureA.Reset();
		LF_TestSaveSectionFixtureB.Reset();
		LF_TestSaveSectionFixtureA.s_bActive = true;
		LF_TestSaveSectionFixtureA.s_iValue = a;
		LF_TestSaveSectionFixtureB.s_bActive = true;
		LF_TestSaveSectionFixtureB.s_fValue = b;
	}

	//------------------------------------------------------------------------------------------------
	protected static SaveContext NewSave(bool binary)
	{
		if (binary)
			return new SCR_PersistenceBinarySaveContext();

		return new JsonSaveContext(false);
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
