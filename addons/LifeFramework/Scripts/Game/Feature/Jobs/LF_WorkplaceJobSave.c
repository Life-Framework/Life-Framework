//------------------------------------------------------------------------------------------------
//! One saved gathering job (#4959): the player's record key and the job by enum name, so a reordered
//! LF_EJobType does not move a saved miner to another job.
class LF_WorkplaceJobRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sPlayerUid;
	string m_sJob;

	//------------------------------------------------------------------------------------------------
	static LF_WorkplaceJobRow Create(string playerUid, string job)
	{
		LF_WorkplaceJobRow row = new LF_WorkplaceJobRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sPlayerUid = playerUid;
		row.m_sJob = job;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The gathering job (MINER, LUMBERJACK, FARMER) each player last took at a workplace desk (#4959).
//! Server only, never replicated. The job itself lives on the body (LF_PlayerJobComponent), which a
//! save does not keep; before this a restart brought a miner back with the older delivery sign-up
//! (LF_CivilianJob) or unemployed. The desk notes the job on take and clears it on quit; a delivery
//! sign-up clears it (the newer job wins). A restored body and a new spawn get it after the sign-up.
class LF_WorkplaceJobs
{
	static const string FEATURE = "Jobs";

	protected static ref map<string, string> s_mJobs;

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Jobs()
	{
		if (!s_mJobs)
			s_mJobs = new map<string, string>();

		return s_mJobs;
	}

	//------------------------------------------------------------------------------------------------
	//! Records the gathering job the player holds now; UNEMPLOYED or a job that is not a gathering job
	//! clears the record.
	static void Note(string playerUid, LF_EJobType job)
	{
		if (playerUid.IsEmpty())
			return;

		if (LF_WorkplaceJob.DeskJob(typename.EnumToString(LF_EJobType, job)) == LF_EJobType.UNEMPLOYED)
		{
			Forget(playerUid);
			return;
		}

		Jobs().Set(playerUid, typename.EnumToString(LF_EJobType, job));
	}

	//------------------------------------------------------------------------------------------------
	static void Forget(string playerUid)
	{
		if (s_mJobs && s_mJobs.Contains(playerUid))
		{
			s_mJobs.Remove(playerUid);
			LF_Debug.Log(FEATURE, string.Format("workplace job of %1 cleared", playerUid));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The gathering job the player last took at a desk, or UNEMPLOYED.
	static LF_EJobType Get(string playerUid)
	{
		if (!s_mJobs || playerUid.IsEmpty() || !s_mJobs.Contains(playerUid))
			return LF_EJobType.UNEMPLOYED;

		return LF_WorkplaceJob.DeskJob(s_mJobs.Get(playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the recorded gathering job on a body the player controls (a restored body, a new spawn).
	//! Runs after LF_CivilianJob.ApplySignUpToCharacter, so the desk job, the newer one, wins.
	//! Idempotent: the same job again is a no-op.
	//! \return true when the body carries the recorded job afterwards.
	static bool ApplyToCharacter(string playerUid, IEntity character)
	{
		LF_EJobType job = Get(playerUid);
		if (job == LF_EJobType.UNEMPLOYED || !character)
			return false;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(character);
		if (!jobs)
		{
			LF_Debug.Error(FEATURE, string.Format("workplace job %1 not put on %2's body: no LF_PlayerJobComponent", typename.EnumToString(LF_EJobType, job), playerUid));
			return false;
		}

		if (!jobs.SetJob(job, false))
		{
			LF_Debug.Warn(FEATURE, string.Format("workplace job %1 not put on %2's body: the job component refused it", typename.EnumToString(LF_EJobType, job), playerUid));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("workplace job %1 back on %2's body", typename.EnumToString(LF_EJobType, job), playerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_WorkplaceJobRow> ExportAll()
	{
		array<ref LF_WorkplaceJobRow> rows = new array<ref LF_WorkplaceJobRow>();
		if (!s_mJobs)
			return rows;

		foreach (string playerUid, string job : s_mJobs)
		{
			rows.Insert(LF_WorkplaceJobRow.Create(playerUid, job));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the records with the saved rows (a second load ends in the same state) and puts each
	//! job on a body its player already controls (a re-apply to a live session). A row whose job is no
	//! longer a gathering job is skipped with a warning.
	//! \return The number of rows kept.
	static int ApplyAll(notnull array<ref LF_WorkplaceJobRow> rows)
	{
		Jobs().Clear();
		int kept = 0;
		foreach (LF_WorkplaceJobRow row : rows)
		{
			if (!row || row.m_sPlayerUid.IsEmpty())
				continue;

			if (LF_WorkplaceJob.DeskJob(row.m_sJob) == LF_EJobType.UNEMPLOYED)
			{
				LF_Debug.Warn(FEATURE, string.Format("saved workplace job '%1' for %2 is not a gathering job, skipped", row.m_sJob, row.m_sPlayerUid));
				continue;
			}

			Jobs().Set(row.m_sPlayerUid, row.m_sJob);
			kept++;
			ApplyToLiveBody(row.m_sPlayerUid);
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyToLiveBody(string playerUid)
	{
		if (!GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) != playerUid)
				continue;

			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (body)
				ApplyToCharacter(playerUid, body);

			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mJobs = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WorkplaceJobs's statics (LF_StaticReset finds it by reflection).
class LF_WorkplaceJobs_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_WorkplaceJobs";
	}

	override void Apply()
	{
		LF_WorkplaceJobs.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "workplacejobs" (#4959): one LF_WorkplaceJobRow per player holding a gathering job
//! taken at a desk. A load replaces the records. OnAbsent keeps the default no-op: a save from before
//! the section has no desk jobs, so those players come back as before (their delivery sign-up, or
//! unemployed) and the desk takes them on again.
class LF_WorkplaceJobsSavePayload : LF_SavePayload
{
	static const string NAME = "workplacejobs";

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
		array<ref LF_WorkplaceJobRow> rows = LF_WorkplaceJobs.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 workplace job row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_WorkplaceJobRow> rows = new array<ref LF_WorkplaceJobRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_WorkplaceJobs.FEATURE, "workplace jobs failed to read; the live records stay as they were");
			return false;
		}

		int kept = LF_WorkplaceJobs.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 workplace job row(s)", kept, rows.Count()));
		return true;
	}
}
