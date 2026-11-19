//------------------------------------------------------------------------------------------------
//! One felled tree in the save ("lumber" section, #3262): its LF_TreeFelling key and the seconds left
//! until it grows back. Never an absolute time: the world clock restarts with the server, so a restart
//! gives the stand the time it had left. Field order is the binary save order; append only.
class LF_TreeRegrowRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sKey;
	int m_iSecondsLeft;

	//------------------------------------------------------------------------------------------------
	static LF_TreeRegrowRow Create(string key, int secondsLeft)
	{
		LF_TreeRegrowRow row = new LF_TreeRegrowRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_iSecondsLeft = secondsLeft;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "lumber" (#3262): one LF_TreeRegrowRow per tree the felled-tree ledger holds. A load
//! replaces the ledger with the rows whose time is not up, hides each tree again on the server and
//! tells the clients (LF_TreeRegrow.HideKey), so a restart does not grow a stand that was just cut.
//! A save from before the section has no felled trees, which is the state a boot starts in, so
//! OnAbsent keeps the default no-op.
class LF_LumberSavePayload : LF_SavePayload
{
	static const string NAME = "lumber";

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
	//! One row per tree still down at `nowS`, each with its seconds left. A tree whose time is up is not written.
	static array<ref LF_TreeRegrowRow> ExportAll(int nowS)
	{
		array<ref LF_TreeRegrowRow> rows = new array<ref LF_TreeRegrowRow>();
		LF_TreeFellingLedger ledger = LF_FelledTrees.Ledger();
		foreach (string key : ledger.Keys())
		{
			int left = ledger.SecondsLeft(key, nowS);
			if (left > 0)
				rows.Insert(LF_TreeRegrowRow.Create(key, left));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the ledger with `rows` on the clock `nowS`. A row with no usable key or no time left is
	//! dropped with a log line; a tree down now that the rows do not name stands again; every kept tree is
	//! hidden. Loading the same rows twice ends in the same state.
	//! \return how many trees are down afterwards.
	static int ApplyAll(notnull array<ref LF_TreeRegrowRow> rows, int nowS)
	{
		map<string, int> keep = new map<string, int>();
		foreach (LF_TreeRegrowRow row : rows)
		{
			if (!row)
				continue;

			int dx;
			int dz;
			string prefab;
			if (!LF_FelledTreesReplication.ParseKey(row.m_sKey, dx, dz, prefab))
			{
				LF_Debug.Warn(LF_TreeRegrow.FEATURE, "saved felled tree with a bad key skipped: " + row.m_sKey);
				continue;
			}

			if (row.m_iSecondsLeft <= 0)
			{
				LF_Debug.Info(LF_TreeRegrow.FEATURE, "saved felled tree already grown back, dropped: " + row.m_sKey);
				continue;
			}

			int previous;
			if (keep.Find(row.m_sKey, previous))
				keep.Set(row.m_sKey, LF_Utils.MaxInt(previous, row.m_iSecondsLeft));
			else if (keep.Count() < LF_FelledTreesReplication.MAX_KEYS)
				keep.Set(row.m_sKey, row.m_iSecondsLeft);
			else
				LF_Debug.Warn(LF_TreeRegrow.FEATURE, "saved felled tree dropped, the table is full: " + row.m_sKey);
		}

		LF_TreeFellingLedger ledger = LF_FelledTrees.Ledger();
		foreach (string live : ledger.Keys())
		{
			if (keep.Contains(live))
				continue;

			LF_TreeRegrow.ShowKey(live);
			LF_FelledTreesReplication.Retract(live);
		}

		ledger.Clear();
		foreach (string key, int left : keep)
		{
			ledger.Fell(key, nowS, left);
			LF_TreeRegrow.HideKey(key);
		}

		if (ledger.Count() > 0)
			LF_TreeRegrow.EnsureTicking();

		return ledger.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_TreeRegrowRow> rows = ExportAll(LF_FelledTrees.NowS());
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 felled tree row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_TreeRegrowRow> rows = new array<ref LF_TreeRegrowRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_TreeRegrow.FEATURE, "felled trees failed to read; the live trees stay as they were");
			return false;
		}

		int kept = ApplyAll(rows, LF_FelledTrees.NowS());
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 felled tree row(s)", kept, rows.Count()));
		return true;
	}
}
