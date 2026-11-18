//------------------------------------------------------------------------------------------------
//! One saved laundering batch ("launder" save section, #2894), keyed by the station's chain site id.
//! Times are seconds, never world time (it restarts at 0 on a boot): m_iWashLeft until the batch is
//! washed, m_iReadySeconds it has waited washed (the forfeit clock). m_iClean is what is still owed:
//! after a short pay it is the unpaid rest. Field order is the binary save order; append only.
class LF_LaunderStationRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sStationKey;
	string m_sOwnerKey;
	int m_iMarked;
	int m_iClean;
	int m_iWashLeft;
	int m_iReadySeconds;

	//------------------------------------------------------------------------------------------------
	static LF_LaunderStationRow Create(string stationKey, string ownerKey, int marked, int clean, int washLeft, int readySeconds)
	{
		LF_LaunderStationRow row = new LF_LaunderStationRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sStationKey = stationKey;
		row.m_sOwnerKey = ownerKey;
		row.m_iMarked = marked;
		row.m_iClean = clean;
		row.m_iWashLeft = washLeft;
		row.m_iReadySeconds = readySeconds;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_LaunderStationRow Copy()
	{
		return Create(m_sStationKey, m_sOwnerKey, m_iMarked, m_iClean, m_iWashLeft, m_iReadySeconds);
	}
}

//------------------------------------------------------------------------------------------------
//! The server's laundering stations by save key, and the bridge to the save. A station registers when
//! it exists; a saved batch that arrives before its station waits as a pending row and is applied when
//! the station registers, so load order never matters. A pending row whose station has not appeared
//! DROP_AFTER_MS after the load is dropped with a warning (the station left the map). A station that
//! is deleted keeps its batch as a pending row, so the next save still holds it.
class LF_LaunderStations
{
	//! How long after a load a saved batch waits for its station before it is dropped.
	static const int DROP_AFTER_MS = 600000; // lf-fixed: a grace for stations that register after the load, not a gameplay tunable

	protected static ref map<string, ref LF_LaunderStationRow> s_mPending;

	//------------------------------------------------------------------------------------------------
	protected static map<string, LF_LaunderStationComponent> Live()
	{
		return LF_LaunderStationComponent.Live();
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_LaunderStationRow> Pending()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref LF_LaunderStationRow>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	//! World seconds now, the clock every station reads (0 with no world).
	static float WorldNow()
	{
		if (!GetGame())
			return 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DropUnclaimed);

		LF_LaunderStationComponent.ResetLive();
		s_mPending = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Register(LF_LaunderStationComponent station)
	{
		if (!station)
			return;

		string key = station.GetStationKey();
		if (key.IsEmpty())
		{
			LF_Debug.Warn(LF_LaunderStationComponent.FEATURE, "a laundering station has no site id; it works but its batch is not saved");
			return;
		}

		LF_LaunderStationComponent known = Live().Get(key);
		if (known && known != station)
		{
			LF_Debug.Warn(LF_LaunderStationComponent.FEATURE, string.Format("laundering station key %1 is used twice; the second station's batch is not saved", key));
			return;
		}

		Live().Set(key, station);
		LF_LaunderStationRow row = Pending().Get(key);
		if (row)
		{
			station.Import(row, WorldNow());
			Pending().Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The station leaves (deleted, or re-keyed by Configure). A loaded batch stays as a pending row.
	static void Unregister(LF_LaunderStationComponent station)
	{
		if (!station)
			return;

		string key = station.GetStationKey();
		if (Live().Get(key) != station)
			return;

		Live().Remove(key);
		LF_LaunderStationRow row = station.Export(WorldNow());
		if (row)
			Pending().Set(key, row);
	}

	//------------------------------------------------------------------------------------------------
	static LF_LaunderStationComponent Find(string stationKey)
	{
		return Live().Get(stationKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The saved batch still waiting for its station, or null.
	static LF_LaunderStationRow FindPending(string stationKey)
	{
		return Pending().Get(stationKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: forget a pending row.
	static void ForgetPending(string stationKey)
	{
		Pending().Remove(stationKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every loaded station's batch at nowSeconds, plus the pending rows.
	static array<ref LF_LaunderStationRow> ExportAll(float nowSeconds)
	{
		array<ref LF_LaunderStationRow> rows = new array<ref LF_LaunderStationRow>();
		foreach (string key, LF_LaunderStationComponent station : Live())
		{
			if (!station)
				continue;

			LF_LaunderStationRow row = station.Export(nowSeconds);
			if (row)
				rows.Insert(row);
		}

		foreach (string key, LF_LaunderStationRow pending : Pending())
		{
			if (pending)
				rows.Insert(pending.Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved batches replace the live ones. A live station with no row is
	//! emptied; a row with no live station waits for it, and is dropped if it has not appeared
	//! DROP_AFTER_MS later. Idempotent: a second load with the same rows ends in the same state.
	//! \return How many rows went to a live station.
	static int ApplyAll(notnull array<ref LF_LaunderStationRow> rows, float nowSeconds)
	{
		s_mPending = new map<string, ref LF_LaunderStationRow>();
		foreach (LF_LaunderStationRow row : rows)
		{
			if (!row || row.m_sStationKey.IsEmpty())
			{
				LF_Debug.Warn(LF_LaunderStationComponent.FEATURE, "saved laundering batch with no station key skipped");
				continue;
			}

			s_mPending.Set(row.m_sStationKey, row.Copy());
		}

		int applied;
		foreach (string key, LF_LaunderStationComponent station : Live())
		{
			if (!station)
				continue;

			LF_LaunderStationRow saved = s_mPending.Get(key);
			station.Import(saved, nowSeconds);
			if (saved)
			{
				s_mPending.Remove(key);
				applied++;
			}
		}

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(DropUnclaimed);
			if (!s_mPending.IsEmpty())
				GetGame().GetCallqueue().CallLater(DropUnclaimed, DROP_AFTER_MS, false);
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every saved batch whose station has not appeared, with a warning naming owner and amounts.
	//! \return How many rows were dropped.
	static int DropUnclaimed()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DropUnclaimed);

		if (!s_mPending)
			return 0;

		int dropped;
		foreach (string key, LF_LaunderStationRow row : s_mPending)
		{
			if (!row)
				continue;

			LF_Debug.Warn(LF_LaunderStationComponent.FEATURE, string.Format("saved batch of laundering station %1 dropped: no such station after the load (owner %2, %3 marked, %4 clean owed)", key, row.m_sOwnerKey, row.m_iMarked, row.m_iClean));
			dropped++;
		}

		s_mPending.Clear();
		return dropped;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "launder" (#2894): one LF_LaunderStationRow per laundering station holding a batch.
//! A load replaces every live station's batch with the saved one. OnAbsent keeps the default no-op:
//! a boot starts with every station empty, and a re-apply to a live session keeps the live batches.
//! The per-character day totals (LF_LaunderRules) are not in it: they stay session-only.
//------------------------------------------------------------------------------------------------
class LF_LaunderSavePayload : LF_SavePayload
{
	static const string NAME = "launder";

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
		array<ref LF_LaunderStationRow> rows = LF_LaunderStations.ExportAll(LF_LaunderStations.WorldNow());
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 laundering batch row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_LaunderStationRow> rows = new array<ref LF_LaunderStationRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_LaunderStationComponent.FEATURE, "laundering batches failed to read; the live stations stay as they were");
			return false;
		}

		int applied = LF_LaunderStations.ApplyAll(rows, LF_LaunderStations.WorldNow());
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 laundering batch row(s); the rest wait for their station", applied, rows.Count()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LaunderStations's statics (LF_StaticReset finds it by reflection).
class LF_LaunderStations_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LaunderStations";
	}

	override void Apply()
	{
		LF_LaunderStations.ResetStatics();
	}
}
