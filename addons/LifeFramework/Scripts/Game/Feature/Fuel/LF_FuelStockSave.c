//------------------------------------------------------------------------------------------------
//! One fuel station in the save ("fuelstock" section, #3329): its station id (the key the pump's
//! `LF_FuelStationComponent` names and the config rows match) and the litres in its tanks. Never the
//! capacity: the config owns that, so a changed row applies after a restart. Field order is the binary
//! save order; append only.
class LF_FuelStockRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sId;
	int m_iLitres;

	//------------------------------------------------------------------------------------------------
	static LF_FuelStockRow Create(string id, int litres)
	{
		LF_FuelStockRow row = new LF_FuelStockRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sId = id;
		row.m_iLitres = litres;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "fuelstock" (#3329, framework #788): one LF_FuelStockRow per station the server has opened
//! (a pump sold from it, or a delivery landed). A load replaces every station's stock with the rows, litres
//! clamped to the station's capacity from the config, so a dry station is still dry and a fresh delivery is
//! still there. Which stations a save may name:
//! - a config with station rows (`m_aStations`) names every station there is; a row whose id has no config
//!   row is a retired station and is dropped with a log line;
//! - a config with no rows gives every id the table values, so any non-empty id is kept.
//! A save from before the section has no stock to load (`OnAbsent` does nothing): each station opens at its
//! pack starting litres on first use, the state a boot without the section had.
class LF_FuelStockSavePayload : LF_SavePayload
{
	static const string NAME = "fuelstock";
	static const string FEATURE = "Fuel";
	//! Most rows one load takes; a save is trusted but never unbounded.
	static const int MAX_ROWS = 512; // lf-fixed: a bound on a read, not a gameplay knob

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
	//! One row per station opened so far, sorted by id so two saves of the same state are the same bytes.
	static array<ref LF_FuelStockRow> ExportAll()
	{
		array<ref LF_FuelStockRow> rows = new array<ref LF_FuelStockRow>();
		array<string> ids = {};
		map<string, ref LF_FuelStock> stocks = LF_FuelStationComponent.Stocks();
		foreach (string id, LF_FuelStock stock : stocks)
		{
			if (stock && !id.IsEmpty())
				ids.Insert(id);
		}

		ids.Sort();
		foreach (string key : ids)
		{
			rows.Insert(LF_FuelStockRow.Create(key, stocks.Get(key).m_iLitres));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every station's stock with `rows`, read against `config`. A row with an empty id, a negative
	//! litre count or a second row for the same id is dropped with a log line (the first of a duplicate wins);
	//! a row with no station in a config that lists its stations is dropped as retired; litres above the
	//! capacity are cut to it. Loading the same rows twice ends in the same state.
	//! \return how many stations hold a loaded stock afterwards.
	static int ApplyAll(notnull array<ref LF_FuelStockRow> rows, notnull LF_FuelConfig config)
	{
		bool listed = config.m_aStations && config.m_aStations.Count() > 0;
		map<string, ref LF_FuelStock> loaded = new map<string, ref LF_FuelStock>();
		int seen;
		foreach (LF_FuelStockRow row : rows)
		{
			if (!row)
				continue;

			seen++;
			if (seen > MAX_ROWS)
			{
				LF_Debug.Warn(FEATURE, string.Format("saved fuel stock rows past %1 dropped", MAX_ROWS));
				break;
			}

			if (row.m_sId.IsEmpty() || row.m_iLitres < 0)
			{
				LF_Debug.Warn(FEATURE, string.Format("saved fuel stock with id '%1' and %2 litres skipped", row.m_sId, row.m_iLitres));
				continue;
			}

			if (listed && !config.FindStation(row.m_sId))
			{
				LF_Debug.Warn(FEATURE, string.Format("saved fuel stock for '%1' skipped: no such station in the config", row.m_sId));
				continue;
			}

			if (loaded.Contains(row.m_sId))
			{
				LF_Debug.Warn(FEATURE, string.Format("saved fuel stock for '%1' appears twice, the first one is kept", row.m_sId));
				continue;
			}

			LF_FuelStock stock = config.NewStationFor(row.m_sId);
			if (row.m_iLitres > stock.m_iCapacity)
				LF_Debug.Warn(FEATURE, string.Format("saved fuel stock for '%1' holds %2 litres, over its capacity %3: cut", row.m_sId, row.m_iLitres, stock.m_iCapacity));

			stock.m_iLitres = LF_Utils.ClampInt(row.m_iLitres, 0, stock.m_iCapacity);
			loaded.Set(row.m_sId, stock);
		}

		LF_FuelStationComponent.ForgetStocks();
		map<string, ref LF_FuelStock> live = LF_FuelStationComponent.Stocks();
		foreach (string id, LF_FuelStock kept : loaded)
		{
			live.Set(id, kept);
			LF_Debug.Info(FEATURE, string.Format("fuel station '%1' loaded with %2 of %3 litres", id, kept.m_iLitres, kept.m_iCapacity));
		}

		return live.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_FuelStockRow> rows = ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 fuel station row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_FuelStockRow> rows = new array<ref LF_FuelStockRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(FEATURE, "fuel stock failed to read; the live stock stays as it was");
			return false;
		}

		LF_FuelConfig config = LF_FuelConfig.Get();
		if (!config)
		{
			LF_Debug.Error(FEATURE, "fuel stock not loaded: no fuel config to read the stations against");
			return false;
		}

		int kept = ApplyAll(rows, config);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 fuel station row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the section: nothing to load. A boot has no station open, and a station opens at
	//! its pack starting litres on first use (`LF_FuelStationComponent.StockFor`), which is the state an
	//! older save implies. Stocks already open are left alone (an unrelated store load must not empty them).
	override void OnAbsent()
	{
	}
}
