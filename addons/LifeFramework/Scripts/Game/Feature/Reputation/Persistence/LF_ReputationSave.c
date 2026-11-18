//------------------------------------------------------------------------------------------------
//! The "reputation" payload's seam on LF_Reputation (#2698). A modded class so it reads the store
//! directly; LF_Reputation.c itself stays free of persistence.
//!
//! EXPORT. One row per (character, faction) with points other than 0. A 0 row is the default and is
//! not written.
//! LOAD. Replaces the store (idempotent: a second load gives the same store). A row whose faction
//! the config no longer lists is skipped with a warning; a row with no character key or faction is
//! skipped. Points clamp to the faction's current first and last threshold, so a retuned ladder
//! reaches old saves. With no reputation config loaded (a broken path or overlay) every row is kept as
//! saved, unclamped, so the next save does not drop all standing. The session cooldown and daily-cap
//! clocks are left alone. The row's m_iVersion is 1 for this shape; a new shape is a new payload.
//------------------------------------------------------------------------------------------------
modded class LF_Reputation
{
	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every non-zero standing as a row.
	static array<ref LF_ReputationRow> ExportAll()
	{
		array<ref LF_ReputationRow> rows = new array<ref LF_ReputationRow>();
		if (!s_mPoints)
			return rows;

		foreach (string uid, map<string, int> held : s_mPoints)
		{
			if (!held)
				continue;

			foreach (string faction, int points : held)
			{
				if (points != 0)
					rows.Insert(LF_ReputationRow.Create(uid, faction, points));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the store with the saved rows.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_ReputationRow> rows)
	{
		s_mPoints = new map<string, ref map<string, int>>();
		if (!rows)
			return 0;

		bool noConfig = !LF_ReputationConfig.Get();
		if (noConfig)
			LF_Debug.Error(FEATURE, string.Format("load kept %1 standing row(s) unclamped: the reputation config did not load", rows.Count()));

		int applied = 0;
		foreach (LF_ReputationRow row : rows)
		{
			if (!row || row.m_sCharacterKey.IsEmpty() || row.m_sFaction.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a standing row with no character or no faction");
				continue;
			}

			if (noConfig)
			{
				SetPoints(row.m_sCharacterKey, row.m_sFaction, row.m_iPoints);
				applied++;
				continue;
			}

			LF_ReputationFactionDef def = FactionDef(row.m_sFaction);
			if (!def)
			{
				LF_Debug.Warn(FEATURE, string.Format("load skipped standing %1 for %2: faction '%3' is not in the reputation config", row.m_iPoints, row.m_sCharacterKey, row.m_sFaction));
				continue;
			}

			int points = def.Clamp(row.m_iPoints);
			if (points != row.m_iPoints)
				LF_Debug.Info(FEATURE, string.Format("load clamped %1 %2 from %3 to %4 (thresholds changed)", row.m_sCharacterKey, row.m_sFaction, row.m_iPoints, points));

			if (points == 0)
				continue;

			SetPoints(row.m_sCharacterKey, row.m_sFaction, points);
			applied++;
		}

		return applied;
	}
}
