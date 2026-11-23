//------------------------------------------------------------------------------------------------
//! Save section "orgflags" (#4213): one LF_OrgFlagRow per organization with a flag. Sections apply
//! after every legacy payload, so the "orgs" register is loaded first. A load replaces the flag
//! table; an organization with no row takes the first free flag. OnAbsent (a save from before
//! the section) keeps the live table (empty on a boot) and gives every organization with no flag
//! the first free one, in organization id order, so a re-applied old save takes no flag away.
//------------------------------------------------------------------------------------------------
class LF_OrgFlagSavePayload : LF_SavePayload
{
	static const string NAME = "orgflags";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_OrgFlagRow> rows = LF_OrgFlags.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 organization flag row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_OrgFlagRow> rows = new array<ref LF_OrgFlagRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_OrgFlags.FEATURE, "organization flags failed to read; the live flags stay as they were");
			return false;
		}

		LF_OrgFlags.ApplyAll(rows);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnAbsent()
	{
		int given = LF_OrgFlags.EnsureAll();
		LF_Debug.Info(LF_OrgFlags.FEATURE, string.Format("save has no organization flags: %1 organization(s) took the first free flag", given));
	}
}
