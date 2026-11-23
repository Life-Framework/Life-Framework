//------------------------------------------------------------------------------------------------
//! Save section "dutybonus" (#4319): one LF_DutyBonusRow per officer or medic with an unpaid outcome bonus
//! or an open hour window (LF_DutyBonus). Before this section the ledger lived in memory and a restart
//! mid-shift lost the bonus; the hour's count is saved too, so a restart does not reopen the cap.
//------------------------------------------------------------------------------------------------
class LF_DutyBonusSavePayload : LF_SavePayload
{
	static const string NAME = "dutybonus";

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
		array<ref LF_DutyBonusRow> rows = LF_DutyBonus.ExportRows();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 duty bonus row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_DutyBonusRow> rows = new array<ref LF_DutyBonusRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_DutyBonus.FEATURE, "duty bonuses failed to read, the bonuses stay as they were");
			return false;
		}

		int kept = LF_DutyBonus.ApplyRows(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 duty bonus row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the section: no bonus was saved, so the session's ledger stays as it is (empty on
	//! a boot).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_DutyBonus.FEATURE, "save has no duty bonuses; the ledger stays as the session holds it (empty on a boot)");
	}
}
