//------------------------------------------------------------------------------------------------
//! One rental deposit still held when the save was written: whose it is, which take it was, and how much.
class LF_HarvestDepositRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sKey;
	string m_sToken;
	int m_iAmount;

	//------------------------------------------------------------------------------------------------
	static LF_HarvestDepositRow Create(string key, string token, int amount)
	{
		LF_HarvestDepositRow row = new LF_HarvestDepositRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_sToken = token;
		row.m_iAmount = amount;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "harvest_deposits" (#3297): the rental deposits held while a kit is out. A kit is a job vehicle and
//! never outlives the session, so a deposit in the save is one whose kit went with the restart; the load pays it back
//! into the taker's bank (sections apply after the bank payload). LF_HarvestContract skips a deposit it has already
//! taken, settled or refunded this session, so a second load pays nothing twice.
class LF_HarvestContractSavePayload : LF_SavePayload
{
	static const string NAME = "harvest_deposits";

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
		array<ref LF_HarvestDepositRow> rows = LF_HarvestContract.ExportDeposits();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 open harvest deposit(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_HarvestDepositRow> rows = new array<ref LF_HarvestDepositRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_HarvestContract.FEATURE, "open harvest deposits failed to read; none is refunded");
			return false;
		}

		int refunded = LF_HarvestContract.RefundSavedDeposits(rows);
		LF_Debug.Info("Persistence", string.Format("refunded %1 of %2 saved harvest deposit(s)", refunded, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the contract held no deposits.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_HarvestContract.FEATURE, "save has no harvest deposits; nothing to refund");
	}
}
