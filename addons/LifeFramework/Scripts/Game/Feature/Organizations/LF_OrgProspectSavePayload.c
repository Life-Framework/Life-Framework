//------------------------------------------------------------------------------------------------
//! Save section "org_prospects" (#4455): who is a gang prospect and since when (unix seconds), so the
//! 48 hours run across a restart. A save from before the section has no prospects: every member it holds
//! keeps full rights.
class LF_OrgProspectSavePayload : LF_SavePayload
{
	static const string NAME = "org_prospects";

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
		array<ref LF_OrgProspectRow> rows = LF_OrgProspect.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 gang prospect(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_OrgProspectRow> rows = new array<ref LF_OrgProspectRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_OrgProspect.FEATURE, "gang prospects failed to read; the live table is kept");
			return false;
		}

		int kept = LF_OrgProspect.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("loaded %1 of %2 gang prospect(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before prospects: nobody in it is one. A boot starts with an empty table, so nothing is
	//! cleared here; an older save re-applied to a live session keeps the prospects who joined in it.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_OrgProspect.FEATURE, "save has no gang prospects; every member it holds keeps full rights");
	}
}
