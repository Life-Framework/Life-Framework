//------------------------------------------------------------------------------------------------
//! One saved market price ("market" save section, #2846): the good's prefab, the price the book
//! stores (before recovery) and the unix second it last moved. Floor, ceiling and curve stay config
//! and are never saved. Field order is the binary save order; append only.
class LF_MarketPriceRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sGood;
	int m_iCurrent;
	int m_iLastSaleUnix;
	//! Reserved for per-town markets; empty is the one shared market. A row with a scope is not read yet.
	string m_sScope;

	//------------------------------------------------------------------------------------------------
	static LF_MarketPriceRow Create(string good, int current, int lastSaleUnix)
	{
		LF_MarketPriceRow row = new LF_MarketPriceRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sGood = good;
		row.m_iCurrent = current;
		row.m_iLastSaleUnix = lastSaleUnix;
		row.m_sScope = string.Empty;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "market" (#2846): one LF_MarketPriceRow per good LF_MarketBook holds. A load replaces
//! the book with the saved rows; recovery for the time the server was down follows from the saved
//! stamp on the next quote. OnAbsent keeps the default no-op: a boot starts with an empty book (every
//! good at its base) and a re-apply to a live session keeps the live prices.
//------------------------------------------------------------------------------------------------
class LF_MarketSavePayload : LF_SavePayload
{
	static const string NAME = "market";

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
	//! \return One row per good in the live book.
	static array<ref LF_MarketPriceRow> ExportAll()
	{
		array<ref LF_MarketPriceRow> rows = new array<ref LF_MarketPriceRow>();
		map<ResourceName, ref LF_MarketPrice> book = LF_MarketBook.Entries();
		if (!book)
			return rows;

		foreach (ResourceName good, LF_MarketPrice price : book)
		{
			if (price)
				rows.Insert(LF_MarketPriceRow.Create(good, price.m_iPrice, price.m_iLastSaleUnix));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the live book with `rows`. A good the config no longer lists is dropped and a price
	//! outside the good's current floor..ceiling is clamped, each with a log line. With no config at
	//! all the rows are kept as saved, so a broken `.conf` never loses the prices on the next save.
	//! \return How many rows the book now holds.
	static int ApplyAll(notnull array<ref LF_MarketPriceRow> rows)
	{
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (!config)
			LF_Debug.Warn(LF_MarketBook.FEATURE, "market config did not load; saved prices are kept unchecked");

		map<ResourceName, ref LF_MarketPrice> book = LF_MarketBook.Entries();
		book.Clear();
		foreach (LF_MarketPriceRow row : rows)
		{
			if (!row || row.m_sGood.IsEmpty())
			{
				LF_Debug.Warn(LF_MarketBook.FEATURE, "saved market row with no good skipped");
				continue;
			}

			if (!row.m_sScope.IsEmpty())
			{
				LF_Debug.Warn(LF_MarketBook.FEATURE, string.Format("saved price of %1 has scope '%2', which this build does not read; skipped", row.m_sGood, row.m_sScope));
				continue;
			}

			int price = row.m_iCurrent;
			if (config)
			{
				LF_MarketGoodDef def = config.FindGood(row.m_sGood);
				if (!def)
				{
					LF_Debug.Info(LF_MarketBook.FEATURE, string.Format("saved price of %1 dropped: the market config no longer lists it", row.m_sGood));
					continue;
				}

				price = LF_Utils.ClampInt(row.m_iCurrent, def.m_iFloor, def.m_iCeiling);
				if (price != row.m_iCurrent)
					LF_Debug.Info(LF_MarketBook.FEATURE, string.Format("saved price of %1 clamped %2 -> %3 (floor %4, ceiling %5)", row.m_sGood, row.m_iCurrent, price, def.m_iFloor, def.m_iCeiling));
			}

			book.Set(row.m_sGood, LF_MarketPrice.Make(price, row.m_iLastSaleUnix));
		}

		return book.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_MarketPriceRow> rows = ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 market price row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_MarketPriceRow> rows = new array<ref LF_MarketPriceRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_MarketBook.FEATURE, "market prices failed to read; the live prices stay as they were");
			return false;
		}

		int kept = ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 market price row(s)", kept, rows.Count()));
		return true;
	}
}
