//------------------------------------------------------------------------------------------------
//! Save section "licencebans" (#3430): one LF_LicenceBanRow per licence ban still in force. A load
//! replaces the live bans, lifts the ones that ended while the server was down, and takes each banned
//! licence from the saved record again. OnAbsent keeps the default no-op: a save from before the
//! section has no bans.
//------------------------------------------------------------------------------------------------
class LF_LicenceBansSavePayload : LF_SavePayload
{
	static const string NAME = "licencebans";

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
		array<ref LF_LicenceBanRow> rows = LF_LicenceBans.ExportAll(LF_LicenceBans.Now());
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 licence ban row(s), game clock %2", rows.Count(), LF_LicenceDesk.ClockText()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_LicenceBanRow> rows = new array<ref LF_LicenceBanRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_LicenceBans.FEATURE, "licence bans failed to read; the live bans stay as they were");
			return false;
		}

		int kept = LF_LicenceBans.ApplyAll(rows, LF_LicenceBans.Now());
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 licence ban row(s); the rest had ended; game clock %3", kept, rows.Count(), LF_LicenceDesk.ClockText()));
		return true;
	}
}
