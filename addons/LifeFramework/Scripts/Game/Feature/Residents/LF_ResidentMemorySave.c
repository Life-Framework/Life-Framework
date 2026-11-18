// LF_ResidentMemorySave.c - resident memory survives a restart (#2861, the "residentmemory" save
// section). One row per resident and character uid that is not neutral: a score other than 0, or a
// cooldown still running at the save. Rows are keyed by resident id and uid, never an EntityID.

//------------------------------------------------------------------------------------------------
//! One saved resident memory row ("residentmemory" save section). Field order is the binary save
//! order; append only. m_aReasons and m_aLastSeconds are parallel: the unix second each reason last
//! counted for this pair.
class LF_ResidentMemorySaveRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sResident;
	string m_sUid;
	int m_iScore;
	ref array<string> m_aReasons = {};
	ref array<int> m_aLastSeconds = {};

	//------------------------------------------------------------------------------------------------
	static LF_ResidentMemorySaveRow Create(string resident, string uid, int score)
	{
		LF_ResidentMemorySaveRow row = new LF_ResidentMemorySaveRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sResident = resident;
		row.m_sUid = uid;
		row.m_iScore = score;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one reason's last award time.
	LF_ResidentMemorySaveRow With(string reason, int lastSeconds)
	{
		m_aReasons.Insert(reason);
		m_aLastSeconds.Insert(lastSeconds);
		return this;
	}
}

//------------------------------------------------------------------------------------------------
//! The save seam of LF_ResidentMemory, kept beside the payload so the rules file stays pure.
modded class LF_ResidentMemory
{
	//------------------------------------------------------------------------------------------------
	//! Every row worth saving at unix time `nowSeconds`: a neutral row (score 0, no cooldown running)
	//! is left out. A written row keeps every reason time it holds.
	static array<ref LF_ResidentMemorySaveRow> ExportRows(int nowSeconds)
	{
		array<ref LF_ResidentMemorySaveRow> rows = new array<ref LF_ResidentMemorySaveRow>();
		if (!s_mRows)
			return rows;

		int cooldown;
		LF_ResidentMemoryConfig config = LF_ResidentMemoryConfig.Get();
		if (config)
			cooldown = config.m_iCooldownSeconds;

		foreach (string key, LF_ResidentMemoryRow row : s_mRows)
		{
			if (!row)
				continue;

			bool running;
			foreach (string reason, int last : row.m_mLastSeconds)
			{
				if (nowSeconds - last < cooldown)
					running = true;
			}

			if (row.m_iScore == 0 && !running)
				continue;

			int bar = key.IndexOf("|");
			if (bar <= 0)
				continue;

			LF_ResidentMemorySaveRow saved = LF_ResidentMemorySaveRow.Create(key.Substring(0, bar), key.Substring(bar + 1, key.Length() - bar - 1), row.m_iScore);	// lf-bounded: a resident id and a uid
			foreach (string reasonId, int lastSeconds : row.m_mLastSeconds)
				saved.With(reasonId, lastSeconds);

			rows.Insert(saved);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every row with `rows` (a load; running it twice ends in the same state). A row with no
	//! resident or uid is skipped; a score off the scale clamps. A row for a resident the residents
	//! config no longer lists is kept, since a pack may bring the resident back; each such resident is
	//! logged once per load.
	//! \param[out] unknownResidents How many distinct resident ids were not in the residents config.
	//! \return How many rows were applied.
	static int ApplyRows(notnull array<ref LF_ResidentMemorySaveRow> rows, out int unknownResidents)
	{
		unknownResidents = 0;
		s_mRows = new map<string, ref LF_ResidentMemoryRow>();
		array<string> known = {};
		LF_ResidentsConfig residents = LF_ResidentsConfig.Get();
		if (residents && residents.m_aResidents)
		{
			foreach (LF_ResidentDef def : residents.m_aResidents)
			{
				if (def)
					known.Insert(def.m_sId);
			}
		}

		array<string> warned = {};
		int applied;
		foreach (LF_ResidentMemorySaveRow saved : rows)
		{
			if (!saved || saved.m_sResident.IsEmpty() || saved.m_sUid.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "memory: a saved row without a resident id or uid is skipped");
				continue;
			}

			if (residents && !known.Contains(saved.m_sResident) && !warned.Contains(saved.m_sResident))
			{
				warned.Insert(saved.m_sResident);
				LF_Debug.Warn(FEATURE, string.Format("memory: saved rows for resident '%1', who is not in the residents config, are kept", saved.m_sResident));
			}

			LF_ResidentMemoryRow row = RowFor(saved.m_sResident, saved.m_sUid, true);
			row.m_iScore = LF_Utils.ClampInt(saved.m_iScore, SCORE_MIN, SCORE_MAX);
			row.m_mLastSeconds.Clear();
			if (saved.m_aReasons && saved.m_aLastSeconds)
			{
				int pairs = LF_Utils.MinInt(saved.m_aReasons.Count(), saved.m_aLastSeconds.Count());
				for (int i = 0; i < pairs; i++)
				{
					if (!saved.m_aReasons[i].IsEmpty())
						row.m_mLastSeconds.Set(saved.m_aReasons[i], saved.m_aLastSeconds[i]);
				}
			}

			applied++;
		}

		unknownResidents = warned.Count();
		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! The admin character wipe (LF_Progression.Forget, #2771): every resident forgets `uid`.
	//! \return How many rows were dropped.
	static int Forget(string uid)
	{
		if (!s_mRows || uid.IsEmpty())
			return 0;

		string suffix = "|" + uid;
		array<string> drop = {};
		foreach (string key, LF_ResidentMemoryRow row : s_mRows)
		{
			if (key.EndsWith(suffix))
				drop.Insert(key);
		}

		foreach (string key : drop)
			s_mRows.Remove(key);

		if (!drop.IsEmpty())
			LF_Debug.Info(FEATURE, string.Format("memory: %1 resident(s) forgot %2 (character wipe)", drop.Count(), uid));

		return drop.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "residentmemory" (#2861): the LF_ResidentMemorySaveRow list. A save without it (older
//! than this section) leaves the live memory as it is: a boot starts empty, and a re-apply to a live
//! session has nothing to replace it with (the OnAbsent default).
class LF_ResidentMemorySavePayload : LF_SavePayload
{
	static const string NAME = "residentmemory";

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
		array<ref LF_ResidentMemorySaveRow> rows = LF_ResidentMemory.ExportRows(System.GetUnixTime());
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 resident memory row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_ResidentMemorySaveRow> rows = new array<ref LF_ResidentMemorySaveRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_ResidentMemory.FEATURE, "resident memory failed to read, the memory stays as it was");
			return false;
		}

		int unknown;
		int applied = LF_ResidentMemory.ApplyRows(rows, unknown);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 resident memory row(s), %3 resident(s) not in config", applied, rows.Count(), unknown));
		return true;
	}
}
