//------------------------------------------------------------------------------------------------
//! Civilian delivery work. Courier, fuel delivery, and taxi are taken at a
//! workplace. A run pays cash once. Abandoning it pays nothing. Job XP lives
//! here; EL_PlayerJobComponent.AddExperience copies it onto the player.
//!
//! Level 1 may take courier work. Level 2 opens fuel delivery. Level 3 opens
//! taxi. Experience per run and per level come from LF_JobsConfig (flagship: 100 XP
//! a run, 500 XP to level 2, each later level twice the one before: 5 runs to fuel
//! delivery, 15 to the taxi). Progression is hidden: no level text reaches the player.
//------------------------------------------------------------------------------------------------
class LF_JobWorker : Managed
{
	string m_sPlayerUid;
	EL_EJobType m_eJob;
	bool m_bSignedUp;
	bool m_bActive;
	int m_iPaidTotal;
	float m_fExperience;
	int m_iLevel;
	EL_EJobType m_eActiveJob;
}

//------------------------------------------------------------------------------------------------
class LF_CivilianJob
{
	static const int PAY_PARCEL = 40;
	static const int PAY_FUEL = 60;
	static const int PAY_TAXI = 80;
	// Defaults when Configs/Jobs/LF_Jobs.conf does not resolve. Same values as the file.
	static const int DEFAULT_XP_PER_RUN = 100;
	static const int DEFAULT_XP_FIRST_LEVEL = 500;
	static const float DEFAULT_XP_LEVEL_GROWTH = 2;

	static const ResourceName PARCEL_PREFAB = "{9CA41609F6C806AE}Prefabs/Items/Roleplay/LF_Parcel.et";
	static const ResourceName FUEL_PREFAB = "{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et";
	static const ResourceName TAXI_PREFAB = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	static const ResourceName DEPOT_PREFAB = "{28F9F4DC40089B51}Prefabs/Props/Services/Post/CounterPostOffice_01.et";

	protected static ref map<string, ref LF_JobWorker> s_mWorkers;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_JobWorker> Workers()
	{
		if (!s_mWorkers)
			s_mWorkers = new map<string, ref LF_JobWorker>();

		return s_mWorkers;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobWorker Worker(string playerUid)
	{
		LF_JobWorker worker = Workers().Get(playerUid);
		if (!worker)
		{
			worker = new LF_JobWorker();
			worker.m_sPlayerUid = playerUid;
			worker.m_eJob = EL_EJobType.UNEMPLOYED;
			worker.m_eActiveJob = EL_EJobType.UNEMPLOYED;
			worker.m_iLevel = 1;
			worker.m_fExperience = 0;
			Workers().Set(playerUid, worker);
		}

		return worker;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: player ids that have a career, without creating one.
	static void CollectCareerUids(notnull array<string> uids)
	{
		if (!s_mWorkers)
			return;

		foreach (string playerUid, LF_JobWorker worker : s_mWorkers)
		{
			if (worker)
				uids.Insert(playerUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the player has a career record (never creates one).
	static bool HasCareer(string playerUid)
	{
		if (!s_mWorkers || playerUid.IsEmpty())
			return false;

		return s_mWorkers.Contains(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: sets the saved career. The sign-up and any active run are session state
	//! and are left alone, so a re-apply to a live session does not end a run.
	static void RestoreCareer(string playerUid, int level, float experience, int paidTotal)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "career restore refused: empty player id");
			return;
		}

		LF_JobWorker worker = Worker(playerUid);
		worker.m_iLevel = Math.Max(1, level);
		worker.m_fExperience = Math.Max(0, experience);
		worker.m_iPaidTotal = Math.Max(0, paidTotal);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the player's career (test cleanup, admin wipe).
	static void ForgetCareer(string playerUid)
	{
		if (s_mWorkers)
			s_mWorkers.Remove(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDeliveryJob(EL_EJobType job)
	{
		if (job == EL_EJobType.PARCEL)
			return true;
		if (job == EL_EJobType.FUEL_DELIVERY)
			return true;
		if (job == EL_EJobType.TAXI)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static int RequiredLevel(EL_EJobType job)
	{
		if (job == EL_EJobType.FUEL_DELIVERY)
			return 2;
		if (job == EL_EJobType.TAXI)
			return 3;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	static int PayFor(EL_EJobType job)
	{
		if (job == EL_EJobType.FUEL_DELIVERY)
			return PAY_FUEL;
		if (job == EL_EJobType.TAXI)
			return PAY_TAXI;

		return PAY_PARCEL;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName PrefabFor(EL_EJobType job)
	{
		if (job == EL_EJobType.FUEL_DELIVERY)
			return FUEL_PREFAB;
		if (job == EL_EJobType.TAXI)
			return TAXI_PREFAB;

		return PARCEL_PREFAB;
	}

	//------------------------------------------------------------------------------------------------
	static int GetLevel(string playerUid)
	{
		if (playerUid.IsEmpty())
			return 1;

		return Worker(playerUid).m_iLevel;
	}

	//------------------------------------------------------------------------------------------------
	static float GetExperience(string playerUid)
	{
		if (playerUid.IsEmpty())
			return 0;

		return Worker(playerUid).m_fExperience;
	}

	//------------------------------------------------------------------------------------------------
	static int GetPaidTotal(string playerUid)
	{
		if (playerUid.IsEmpty())
			return 0;

		return Worker(playerUid).m_iPaidTotal;
	}

	//------------------------------------------------------------------------------------------------
	static EL_EJobType GetJob(string playerUid)
	{
		if (playerUid.IsEmpty())
			return EL_EJobType.UNEMPLOYED;

		return Worker(playerUid).m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasActiveRun(string playerUid)
	{
		if (playerUid.IsEmpty())
			return false;

		return Worker(playerUid).m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	static EL_EJobType GetActiveJob(string playerUid)
	{
		if (playerUid.IsEmpty())
			return EL_EJobType.UNEMPLOYED;

		return Worker(playerUid).m_eActiveJob;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasCargo(EL_EJobType job)
	{
		if (job == EL_EJobType.PARCEL)
			return true;
		if (job == EL_EJobType.FUEL_DELIVERY)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string NameKey(EL_EJobType job)
	{
		if (job == EL_EJobType.FUEL_DELIVERY)
			return "#EL-Job_Name_Fuel";
		if (job == EL_EJobType.TAXI)
			return "#EL-Job_Name_Taxi";

		return "#EL-Job_Name_Courier";
	}

	//------------------------------------------------------------------------------------------------
	static bool CanSignUp(string playerUid, EL_EJobType job)
	{
		if (playerUid.IsEmpty() || !IsDeliveryJob(job))
			return false;

		return GetLevel(playerUid) >= RequiredLevel(job);
	}

	//------------------------------------------------------------------------------------------------
	static bool SignUp(string playerUid, EL_EJobType job)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "signup refused: empty player id");
			return false;
		}

		if (!IsDeliveryJob(job))
		{
			EL_Debug.Error("Jobs", string.Format("signup refused: %1 is not a delivery job", typename.EnumToString(EL_EJobType, job)));
			return false;
		}

		if (!CanSignUp(playerUid, job))
		{
			EL_Debug.Log("Jobs", string.Format("signup rejected: %1 level %2 below %3 for %4", playerUid, GetLevel(playerUid), RequiredLevel(job), typename.EnumToString(EL_EJobType, job)));
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		worker.m_eJob = job;
		worker.m_bSignedUp = true;
		EL_Debug.Log("Jobs", string.Format("signed up %1 -> %2 (level %3)", playerUid, typename.EnumToString(EL_EJobType, job), worker.m_iLevel));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool StartRun(string playerUid, EL_EJobType job)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "start refused: empty player id");
			return false;
		}

		if (!IsDeliveryJob(job))
		{
			EL_Debug.Error("Jobs", "start refused: not a delivery job");
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bSignedUp || worker.m_eJob != job)
		{
			EL_Debug.Log("Jobs", string.Format("start rejected: %1 is not signed up for %2", playerUid, typename.EnumToString(EL_EJobType, job)));
			return false;
		}

		if (worker.m_bActive)
		{
			EL_Debug.Log("Jobs", string.Format("start rejected: %1 already has a run", playerUid));
			return false;
		}

		if (!PrefabLoads(job))
			return false;

		worker.m_bActive = true;
		worker.m_eActiveJob = job;
		EL_Debug.Log("Jobs", string.Format("run started %1 job %2", playerUid, typename.EnumToString(EL_EJobType, job)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the active run once and grants job XP. A second call pays nothing.
	static int CompleteRun(string playerUid)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "complete refused: empty player id");
			return 0;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bActive)
		{
			EL_Debug.Log("Jobs", string.Format("complete rejected: %1 has no active run", playerUid));
			return 0;
		}

		int amount = PayFor(worker.m_eActiveJob);
		EL_EJobType finished = worker.m_eActiveJob;
		worker.m_bActive = false;
		worker.m_eActiveJob = EL_EJobType.UNEMPLOYED;
		worker.m_iPaidTotal = worker.m_iPaidTotal + amount;

		AddExperience(playerUid, XpPerRun());
		EL_Debug.Log("Jobs", string.Format("run paid %1 to %2", amount, playerUid));
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the active run with no pay.
	static bool AbandonRun(string playerUid)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "abandon refused: empty player id");
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bActive)
		{
			EL_Debug.Log("Jobs", string.Format("abandon rejected: %1 has no active run", playerUid));
			return false;
		}

		worker.m_bActive = false;
		worker.m_eActiveJob = EL_EJobType.UNEMPLOYED;
		EL_Debug.Log("Jobs", string.Format("run abandoned by %1, no pay", playerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the awarded cash on the player. A short pocket is logged. The run
	//! is already marked paid, so a full inventory does not pay a second time.
	static void HandCash(IEntity user, int amount)
	{
		if (!user || amount <= 0)
			return;

		int given = EL_MoneyUtils.GiveCash(user, amount);
		if (given != amount)
			EL_Debug.Warn("Jobs", string.Format("run payout short: paid %1 of %2 (player %3)", given, amount, user));
		else
			EL_Debug.Log("Jobs", string.Format("run cash +%1", given));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one parcel or fuel can on the player. Taxi uses the rank vehicle.
	//! A full inventory leaves no run and no leftover item.
	static bool GiveCargo(IEntity user, EL_EJobType job)
	{
		if (!HasCargo(job))
			return true;

		if (!user)
		{
			EL_Debug.Error("Jobs", "cargo give refused: no player");
			return false;
		}

		ResourceName prefab = PrefabFor(job);
		int added = EL_InventoryUtils.AddAmount(user, prefab, 1, false);
		if (added != 1)
		{
			EL_Debug.Error("Jobs", "cargo give failed: " + prefab);
			return false;
		}

		EL_Debug.Log("Jobs", string.Format("cargo given %1", prefab));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the parcel or fuel can. The fuel can is consumed here.
	static bool TakeCargo(IEntity user, EL_EJobType job)
	{
		if (!HasCargo(job))
			return true;

		if (!user)
		{
			EL_Debug.Error("Jobs", "cargo take refused: no player");
			return false;
		}

		ResourceName prefab = PrefabFor(job);
		int removed = EL_InventoryUtils.RemoveAmount(user, prefab, 1);
		if (removed != 1)
		{
			EL_Debug.Error("Jobs", "cargo missing at dropoff: " + prefab);
			return false;
		}

		if (job == EL_EJobType.FUEL_DELIVERY)
			EL_Debug.Log("Jobs", string.Format("fuel can consumed for %1", EL_Utils.GetPlayerUID(user)));
		else
			EL_Debug.Log("Jobs", string.Format("parcel delivered for %1", EL_Utils.GetPlayerUID(user)));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Job XP for the civilian career. EL_PlayerJobComponent.AddExperience
	//! forwards here. Each level needs XpForLevel(level); the remainder stays.
	static void AddExperience(string playerUid, float amount)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "experience refused: empty player id");
			return;
		}

		if (amount <= 0)
		{
			EL_Debug.Error("Jobs", "experience refused: non-positive amount");
			return;
		}

		LF_JobWorker worker = Worker(playerUid);
		int levelBefore = worker.m_iLevel;
		worker.m_fExperience = worker.m_fExperience + amount;

		float step = XpForLevel(worker.m_iLevel);
		while (worker.m_fExperience >= step)
		{
			worker.m_fExperience = worker.m_fExperience - step;
			worker.m_iLevel = worker.m_iLevel + 1;
			step = XpForLevel(worker.m_iLevel);
		}

		EL_Debug.Log("Jobs", string.Format("job xp +%1 -> level %2 xp %3 (%4)", amount, worker.m_iLevel, worker.m_fExperience, playerUid));
		if (worker.m_iLevel > levelBefore)
			EL_Debug.Log("Jobs", string.Format("job level -> %1 (%2)", worker.m_iLevel, playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! The delivery job that a rise from levelBefore to levelAfter opens, or UNEMPLOYED.
	//! The clerk mentions new work; the level itself is never shown.
	static EL_EJobType NewlyOpened(int levelBefore, int levelAfter)
	{
		if (levelAfter <= levelBefore)
			return EL_EJobType.UNEMPLOYED;

		if (levelBefore < RequiredLevel(EL_EJobType.TAXI) && levelAfter >= RequiredLevel(EL_EJobType.TAXI))
			return EL_EJobType.TAXI;

		if (levelBefore < RequiredLevel(EL_EJobType.FUEL_DELIVERY) && levelAfter >= RequiredLevel(EL_EJobType.FUEL_DELIVERY))
			return EL_EJobType.FUEL_DELIVERY;

		return EL_EJobType.UNEMPLOYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Job experience one completed run grants (config, at least 1).
	static int XpPerRun()
	{
		int xp = DEFAULT_XP_PER_RUN;
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (config)
			xp = config.m_iJobXpPerRun;

		return Math.Max(1, xp);
	}

	//------------------------------------------------------------------------------------------------
	//! Experience needed to go from level to level + 1: the first level's cost times the
	//! growth factor once per level above 1. Never below 1, so the level loop always ends.
	static float XpForLevel(int level)
	{
		float first = DEFAULT_XP_FIRST_LEVEL;
		float growth = DEFAULT_XP_LEVEL_GROWTH;
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (config)
		{
			first = config.m_iJobXpFirstLevel;
			growth = config.m_fJobXpLevelGrowth;
		}

		growth = Math.Max(1, growth);
		float cost = Math.Max(1, first);
		for (int i = 1; i < level; i++)
			cost = cost * growth;

		return cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Completed runs a new worker needs to reach level (pacing, tests, docs).
	static int RunsToReach(int level)
	{
		float needed = 0;
		for (int i = 1; i < level; i++)
			needed = needed + XpForLevel(i);

		return Math.Ceil(needed / XpPerRun());
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PrefabLoads(EL_EJobType job)
	{
		ResourceName prefab = PrefabFor(job);
		Resource loaded = Resource.Load(prefab);
		if (!loaded.IsValid())
		{
			EL_Debug.Error("Jobs", "run prefab missing: " + prefab);
			return false;
		}

		return true;
	}
}
