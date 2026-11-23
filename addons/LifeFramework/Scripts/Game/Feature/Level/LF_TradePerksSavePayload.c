//------------------------------------------------------------------------------------------------
//! Save section "tradeperks" (WK-W8, #4186): one LF_TradePerkRow per perk a character picked in a
//! trade, keyed by the character's record key. Sections apply after every legacy payload, so the
//! "characters" payload has already set the final record keys.
//------------------------------------------------------------------------------------------------
class LF_TradePerksSavePayload : LF_SavePayload
{
	static const string NAME = "tradeperks";

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
		array<ref LF_TradePerkRow> rows = LF_TradePerks.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 trade perk row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_TradePerkRow> rows = new array<ref LF_TradePerkRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_TradePerks.FEATURE, "trade perks failed to read, perks stay as they were");
			return false;
		}

		// A trade is a job id: a job renamed in the alias table moves its perks with its XP.
		foreach (LF_TradePerkRow row : rows)
		{
			if (row)
				row.m_sTrade = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, row.m_sTrade);
		}

		int kept = LF_TradePerks.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 trade perk row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before perks (0.2.1 and older): nobody has picked one, which is the state a boot
	//! starts in, so nothing is cleared (as the barber and hive sections do).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_TradePerks.FEATURE, "save has no trade perks; every character starts with none");
	}
}
