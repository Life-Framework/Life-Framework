//------------------------------------------------------------------------------------------------
//! Two test-only save sections (#2841), the shape a feature's section takes. Inactive unless a test
//! turns them on, so a real save never carries them; registered always, so a save that does carry
//! them (the e2e save boot) reads back in the next boot.
//------------------------------------------------------------------------------------------------
class LF_TestSaveSectionFixtureA : LF_SavePayload
{
	static const string NAME = "testfixture_a";
	static bool s_bActive;
	static int s_iVersion = 1;
	static int s_iValue;
	static int s_iRead = -1;
	static int s_iReadVersion;
	static bool s_bAbsent;
	static bool s_bFailRead;

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_bActive = false;
		s_iVersion = 1;
		s_iValue = 0;
		s_iRead = -1;
		s_iReadVersion = 0;
		s_bAbsent = false;
		s_bFailRead = false;
	}

	override string Name()
	{
		return NAME;
	}

	override int Version()
	{
		return s_iVersion;
	}

	override bool IsActive()
	{
		return s_bActive;
	}

	override void WriteSection(notnull SaveContext context)
	{
		array<int> rows = {s_iValue, s_iValue + 1};
		context.WriteValue("rows", rows);
	}

	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		if (s_bFailRead)
			return false;

		array<int> rows = {};
		if (!context.ReadValue("rows", rows) || rows.Count() != 2)
			return false;

		s_iRead = rows.Get(0);
		s_iReadVersion = savedVersion;
		return true;
	}

	override void OnAbsent()
	{
		s_bAbsent = true;
	}
}

//------------------------------------------------------------------------------------------------
//! The second fixture, sorted after A, with a record array as its rows.
class LF_TestSaveSectionFixtureB : LF_SavePayload
{
	static const string NAME = "testfixture_b";
	static bool s_bActive;
	static float s_fValue;
	static float s_fRead = -1;
	static bool s_bAbsent;

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_bActive = false;
		s_fValue = 0;
		s_fRead = -1;
		s_bAbsent = false;
	}

	override string Name()
	{
		return NAME;
	}

	override bool IsActive()
	{
		return s_bActive;
	}

	override void WriteSection(notnull SaveContext context)
	{
		WriteFixed(context, s_fValue);
	}

	//! Writes the rows with a given value (a test building a save by hand).
	static void WriteFixed(notnull SaveContext context, float value)
	{
		array<ref LF_SurvivalStatsRecord> rows = {};
		LF_SurvivalStatsRecord row = new LF_SurvivalStatsRecord();
		row.m_sPersistentId = "fixture-b";
		row.m_fHunger = value;
		rows.Insert(row);
		context.WriteValue("rows", rows);
	}

	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_SurvivalStatsRecord> rows = {};
		if (!context.ReadValue("rows", rows) || rows.Count() != 1 || !rows.Get(0))
			return false;

		s_fRead = rows.Get(0).m_fHunger;
		return true;
	}

	override void OnAbsent()
	{
		s_bAbsent = true;
	}
}
