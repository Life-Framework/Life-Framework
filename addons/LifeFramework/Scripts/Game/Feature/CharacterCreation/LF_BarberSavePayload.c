//------------------------------------------------------------------------------------------------
//! Save section "barber" (#3234): one LF_BarberLookRow per character that paid for a look, keyed by
//! the character's record key. Sections apply after every legacy payload, so the "characters"
//! payload has already set the final record keys; the spawn hook puts the look back on the body.
//------------------------------------------------------------------------------------------------
class LF_BarberSavePayload : LF_SavePayload
{
	static const string NAME = "barber";

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
		array<ref LF_BarberLookRow> rows = LF_Barber.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 barber look row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_BarberLookRow> rows = new array<ref LF_BarberLookRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_Barber.FEATURE, "barber looks failed to read, looks stay as they were");
			return false;
		}

		int kept = LF_Barber.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 barber look row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the barber: nobody has paid for a look, every character keeps its prefab head.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_Barber.FEATURE, "save has no barber looks; every character keeps its own head");
	}
}
