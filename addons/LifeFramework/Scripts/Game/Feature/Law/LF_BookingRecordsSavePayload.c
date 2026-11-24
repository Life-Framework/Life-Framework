//------------------------------------------------------------------------------------------------
//! Save section "booking_records" (#4711 follow-up): one LF_BookingRecordRow per settled booking, the
//! newest LF_BookingRecords.KEEP per account, keyed by the account's player uid. Load replaces the store.
class LF_BookingRecordsSavePayload : LF_SavePayload
{
	static const string NAME = "booking_records";

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
		array<ref LF_BookingRecordRow> rows = LF_BookingRecords.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 booking record(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_BookingRecordRow> rows = new array<ref LF_BookingRecordRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_BookingRecords.FEATURE, "booking records failed to read; the records stay as they were");
			return false;
		}

		int kept = LF_BookingRecords.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 booking record(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the records: no booking has its details; the conviction count stays. Nothing is
	//! cleared: a boot starts with an empty store, and a live session keeps what it wrote.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_BookingRecords.FEATURE, "save has no booking records; My file lists no past booking yet");
	}
}
