//------------------------------------------------------------------------------------------------
//! Save section "hives" (#3447): one LF_HiveRow per beehive, live or waiting for its entity. A load
//! replaces every hive's combs and clock (LF_Hives.ApplyAll). A save from before the section has no
//! hives, which is the state a boot starts in, so OnAbsent keeps the default no-op.
class LF_HiveSavePayload : LF_SavePayload
{
	static const string NAME = "hives";

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
		array<ref LF_HiveRow> rows = LF_Hives.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 beehive row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_HiveRow> rows = new array<ref LF_HiveRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_Hives.FEATURE, "beehives failed to read; the live hives stay as they were");
			return false;
		}

		int kept = LF_Hives.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 beehive row(s)", kept, rows.Count()));
		return true;
	}
}
