//------------------------------------------------------------------------------------------------
//! Save section "shiftboard" (WK-W22, #4712): the week the shift boards belong to, then one
//! LF_ShiftBoardRow per worker, trade and site with the XP earned there that week. A load in a later
//! week empties the boards on the first award or read (LF_ShiftBoard.Roll).
//------------------------------------------------------------------------------------------------
class LF_ShiftBoardSavePayload : LF_SavePayload
{
	static const string NAME = "shiftboard";

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
		int week = LF_ShiftBoard.CurrentWeek();
		context.WriteValue("week", week);
		array<ref LF_ShiftBoardRow> rows = LF_ShiftBoard.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 shift board row(s) of week %2", rows.Count(), week));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		int week = -1;
		array<ref LF_ShiftBoardRow> rows = new array<ref LF_ShiftBoardRow>();
		if (!context.ReadValue("week", week) || !context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_ShiftBoard.FEATURE, "shift boards failed to read, they stay as they were");
			return false;
		}

		// A trade is a job id: a job renamed in the alias table moves its board XP with it.
		foreach (LF_ShiftBoardRow row : rows)
		{
			if (row)
				row.m_sJob = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, row.m_sJob);
		}

		int kept = LF_ShiftBoard.ApplyAll(rows, week);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 shift board row(s) of week %3", kept, rows.Count(), week));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the shift boards: every board starts the week empty, which is the state a
	//! boot starts in, so nothing is cleared (as the tradeperks, barber and hive sections do).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_ShiftBoard.FEATURE, "save has no shift boards; every board starts empty");
	}
}
