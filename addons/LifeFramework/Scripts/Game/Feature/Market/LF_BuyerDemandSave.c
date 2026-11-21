//------------------------------------------------------------------------------------------------
//! One saved buyer purse ("buyerdemand" save section, #3599): the buyer plus good key
//! (LF_BuyerDemandList.RowId), the units left and the unix second the refill clock counts from. Cap,
//! refill and price stay config and are never saved. Field order is the binary save order; append only.
class LF_BuyerPurseRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sKey;
	int m_iRemaining;
	int m_iStamp;

	//------------------------------------------------------------------------------------------------
	static LF_BuyerPurseRow Create(string key, int remaining, int stamp)
	{
		LF_BuyerPurseRow row = new LF_BuyerPurseRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_iRemaining = remaining;
		row.m_iStamp = stamp;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "buyerdemand" (#3599): one LF_BuyerPurseRow per purse LF_BuyerDemandBook holds. A load
//! replaces the book with the saved rows; the refill for the time the server was down follows from the
//! saved stamp on the next sale. OnAbsent keeps the default no-op: a boot starts with no purses (every
//! buyer full) and a re-apply to a live session keeps the live purses.
class LF_BuyerDemandSavePayload : LF_SavePayload
{
	static const string NAME = "buyerdemand";

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
	//! \return One row per purse in the live book.
	static array<ref LF_BuyerPurseRow> ExportAll()
	{
		array<ref LF_BuyerPurseRow> rows = new array<ref LF_BuyerPurseRow>();
		foreach (string key, LF_BuyerPurse purse : LF_BuyerDemandBook.Entries())
		{
			if (purse)
				rows.Insert(LF_BuyerPurseRow.Create(key, purse.m_iRemaining, purse.m_iStamp));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The config row a saved key names, or null (no config, or the row is gone).
	protected static LF_BuyerDemandRow RowForKey(string key)
	{
		LF_BuyerDemandConfig config = LF_BuyerDemandConfig.Get();
		if (!config || !config.m_aRows)
			return null;

		foreach (LF_BuyerDemandRow row : config.m_aRows)
		{
			if (row && LF_BuyerDemandBook.KeyOf(row.m_sBuyer, row.m_sGood) == key)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the live book with `rows`. The units of a purse whose buyer row the config still has are held
	//! to the row's cap (a lowered cap). A purse with no row (a buyer or good the config dropped, or a config
	//! that did not load) is kept as saved and inert, so a broken or changed `.conf` never loses the purses
	//! on the next save.
	//! \return How many purses the book now holds.
	static int ApplyAll(notnull array<ref LF_BuyerPurseRow> rows)
	{
		map<string, ref LF_BuyerPurse> book = LF_BuyerDemandBook.Entries();
		book.Clear();
		foreach (LF_BuyerPurseRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty())
			{
				LF_Debug.Warn(LF_BuyerDemandBook.FEATURE, "saved buyer purse with no key skipped");
				continue;
			}

			LF_BuyerPurse purse = new LF_BuyerPurse();
			purse.m_iRemaining = LF_Utils.MaxInt(row.m_iRemaining, 0);
			purse.m_iStamp = row.m_iStamp;
			LF_BuyerDemandRow demand = RowForKey(row.m_sKey);
			if (demand)
				purse = LF_BuyerPurse.Make(demand, row.m_iRemaining, row.m_iStamp);
			else
				LF_Debug.Info(LF_BuyerDemandBook.FEATURE, string.Format("saved purse %1 kept inert: the buyer demand config has no row for it", row.m_sKey));

			book.Set(row.m_sKey, purse);
		}

		return book.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_BuyerPurseRow> rows = ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 buyer purse row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_BuyerPurseRow> rows = new array<ref LF_BuyerPurseRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_BuyerDemandBook.FEATURE, "buyer purses failed to read; the live purses stay as they were");
			return false;
		}

		int kept = ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 buyer purse row(s)", kept, rows.Count()));
		return true;
	}
}
