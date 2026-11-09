//------------------------------------------------------------------------------------------------
//! Civilian delivery work. Courier, fuel delivery, and taxi are taken at a
//! workplace. A run pays cash once. Abandoning it pays nothing. Job XP lives
//! here; LF_PlayerJobComponent.AddExperience copies it onto the player.
//!
//! Level 1 may take courier work. Level 2 opens fuel delivery. Level 3 opens
//! taxi. Experience per run and per level come from LF_JobsConfig (flagship: 100 XP
//! a run, 500 XP to level 2, each later level twice the one before: 5 runs to fuel
//! delivery, 15 to the taxi). Progression is hidden: no level text reaches the player.
//------------------------------------------------------------------------------------------------
class LF_JobWorker : Managed
{
	string m_sPlayerUid;
	LF_EJobType m_eJob;
	bool m_bSignedUp;
	bool m_bActive;
	int m_iPaidTotal;
	float m_fExperience;
	int m_iLevel;
	LF_EJobType m_eActiveJob;
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
			worker.m_eJob = LF_EJobType.UNEMPLOYED;
			worker.m_eActiveJob = LF_EJobType.UNEMPLOYED;
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
	//! Persistence seam: sets the saved career. The sign-up has its own payload (ApplySignUps) and an
	//! active run is session state; both are left alone, so a re-apply to a live session does not end a run.
	static void RestoreCareer(string playerUid, int level, float experience, int paidTotal)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "career restore refused: empty player id");
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
	//! \return The delivery job the player is signed up for, or UNEMPLOYED (never creates a career).
	static LF_EJobType GetSignedUpJob(string playerUid)
	{
		if (!s_mWorkers || playerUid.IsEmpty())
			return LF_EJobType.UNEMPLOYED;

		LF_JobWorker worker = s_mWorkers.Get(playerUid);
		if (!worker || !worker.m_bSignedUp)
			return LF_EJobType.UNEMPLOYED;

		return worker.m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one record per signed-up player. The job is stored by enum name, so a
	//! reordered LF_EJobType does not move a saved sign-up to another job. An active run is not
	//! saved; after a restart the player is still signed up and starts a new run.
	static array<ref LF_JobSignUpRecord> ExportSignUps()
	{
		array<ref LF_JobSignUpRecord> records = new array<ref LF_JobSignUpRecord>();
		if (!s_mWorkers)
			return records;

		foreach (string playerUid, LF_JobWorker worker : s_mWorkers)
		{
			if (!worker || !worker.m_bSignedUp || !IsDeliveryJob(worker.m_eJob))
				continue;

			records.Insert(LF_JobSignUpRecord.Create(playerUid, typename.EnumToString(LF_EJobType, worker.m_eJob)));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: signs the saved players up again and puts the job on their live bodies.
	//! Runs after the progression payload, so the career level that gates the job is restored
	//! first. Applying the same records twice ends in the same state; an active run is left alone.
	static void ApplySignUps(notnull array<ref LF_JobSignUpRecord> records)
	{
		int restored = 0;
		int bodies = 0;
		foreach (LF_JobSignUpRecord record : records)
		{
			if (!record)
				continue;

			if (!RestoreSignUp(record.m_sPlayerUid, record.m_sJob))
				continue;

			restored++;
			if (ApplySignUpToLiveBody(record.m_sPlayerUid))
				bodies++;
		}

		LF_Debug.Info("Jobs", string.Format("restored %1 of %2 job sign-up(s), %3 live body(ies)", restored, records.Count(), bodies));
	}

	//------------------------------------------------------------------------------------------------
	//! Signs the player up for a saved job by name. A name that is no longer a delivery job, or a
	//! job above the player's career level, is logged and leaves the player unemployed.
	//! \return true when the player is signed up for the job afterwards.
	static bool RestoreSignUp(string playerUid, string jobName)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn("Jobs", string.Format("saved sign-up '%1' has no player id, skipped", jobName));
			return false;
		}

		LF_EJobType job = typename.StringToEnum(LF_EJobType, jobName);
		if (typename.EnumToString(LF_EJobType, job) != jobName || !IsDeliveryJob(job))
		{
			LF_Debug.Warn("Jobs", string.Format("saved sign-up '%1' for %2 is not a delivery job, stays unemployed", jobName, playerUid));
			return false;
		}

		if (!CanSignUp(playerUid, job))
		{
			LF_Debug.Warn("Jobs", string.Format("saved sign-up %1 for %2 is above career level %3, stays unemployed", jobName, playerUid, GetLevel(playerUid)));
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		worker.m_eJob = job;
		worker.m_bSignedUp = true;
		LF_Debug.Info("Jobs", string.Format("sign-up restored %1 -> %2", playerUid, jobName));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the signed-up job on a body the player controls (a new spawn, or a live body after a
	//! load). The job component reads the player id from its controlled body, so a spawn calls
	//! this after the hand-over. Setting the same job again is a no-op.
	//! \return true when the body carries the signed-up job afterwards.
	static bool ApplySignUpToCharacter(string playerUid, IEntity character)
	{
		LF_EJobType job = GetSignedUpJob(playerUid);
		if (job == LF_EJobType.UNEMPLOYED || !character)
			return false;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(character);
		if (!jobs)
		{
			LF_Debug.Error("Jobs", string.Format("sign-up %1 not put on %2's body: no LF_PlayerJobComponent", typename.EnumToString(LF_EJobType, job), playerUid));
			return false;
		}

		if (!jobs.SetJob(job))
		{
			LF_Debug.Warn("Jobs", string.Format("sign-up %1 not put on %2's body: the job component refused it", typename.EnumToString(LF_EJobType, job), playerUid));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplySignUpToLiveBody(string playerUid)
	{
		if (!GetGame())
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) != playerUid)
				continue;

			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (body)
				return ApplySignUpToCharacter(playerUid, body);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDeliveryJob(LF_EJobType job)
	{
		if (job == LF_EJobType.PARCEL)
			return true;
		if (job == LF_EJobType.FUEL_DELIVERY)
			return true;
		if (job == LF_EJobType.TAXI)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static int RequiredLevel(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return 2;
		if (job == LF_EJobType.TAXI)
			return 3;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	static int PayFor(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return PAY_FUEL;
		if (job == LF_EJobType.TAXI)
			return PAY_TAXI;

		return PAY_PARCEL;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName PrefabFor(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return FUEL_PREFAB;
		if (job == LF_EJobType.TAXI)
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
	static LF_EJobType GetJob(string playerUid)
	{
		if (playerUid.IsEmpty())
			return LF_EJobType.UNEMPLOYED;

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
	static LF_EJobType GetActiveJob(string playerUid)
	{
		if (playerUid.IsEmpty())
			return LF_EJobType.UNEMPLOYED;

		return Worker(playerUid).m_eActiveJob;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasCargo(LF_EJobType job)
	{
		if (job == LF_EJobType.PARCEL)
			return true;
		if (job == LF_EJobType.FUEL_DELIVERY)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string NameKey(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return "#EL-Job_Name_Fuel";
		if (job == LF_EJobType.TAXI)
			return "#EL-Job_Name_Taxi";

		return "#EL-Job_Name_Courier";
	}

	//------------------------------------------------------------------------------------------------
	//! The sign-up toast body: the next step, at the same counter.
	static string SignedUpKey(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return "#LF-Job_SignedUp_Fuel";
		if (job == LF_EJobType.TAXI)
			return "#LF-Job_SignedUp_Taxi";

		return "#LF-Job_SignedUp_Courier";
	}

	//------------------------------------------------------------------------------------------------
	//! The toast when a run starts: where the cargo goes. Empty for a job that does not need one.
	static string RunStartedKey(LF_EJobType job)
	{
		if (job == LF_EJobType.PARCEL)
			return "#LF-Job_RunStarted_Courier";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether "Complete the run" and "Abandon" are offered. The server reads its run map. The
	//! runs live on the server only, so a dedicated client decides from what it can see: the job
	//! replicated on its LF_PlayerJobComponent and the cargo in its own inventory (ClientRunOpen).
	//! Reading the server map on the client greyed both actions for every hosted player.
	static bool RunOfferOpen(IEntity user)
	{
		if (!user)
			return false;

		if (Replication.IsServer())
		{
			string uid = LF_Utils.GetPlayerUid(user);
			return !uid.IsEmpty() && HasActiveRun(uid);
		}

		LF_EJobType bodyJob = LF_EJobType.UNEMPLOYED;
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs)
			bodyJob = jobs.GetJob();

		bool carrying = false;
		if (HasCargo(bodyJob))
		{
			InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(user.FindComponent(InventoryStorageManagerComponent));
			if (storage)
				carrying = !LF_InventoryUtils.FindItemsByPrefab(storage, PrefabFor(bodyJob)).IsEmpty();
		}

		return ClientRunOpen(bodyJob, carrying);
	}

	//------------------------------------------------------------------------------------------------
	//! Client rule for an active run: a delivery job on the body, and its cargo in hand when the
	//! job has one (a taxi fare has none; the server refuses a fare that is not running).
	static bool ClientRunOpen(LF_EJobType bodyJob, bool carryingCargo)
	{
		if (!IsDeliveryJob(bodyJob))
			return false;

		if (!HasCargo(bodyJob))
			return true;

		return carryingCargo;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanSignUp(string playerUid, LF_EJobType job)
	{
		if (playerUid.IsEmpty() || !IsDeliveryJob(job))
			return false;

		return GetLevel(playerUid) >= RequiredLevel(job);
	}

	//------------------------------------------------------------------------------------------------
	static bool SignUp(string playerUid, LF_EJobType job)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "signup refused: empty player id");
			return false;
		}

		if (!IsDeliveryJob(job))
		{
			LF_Debug.Error("Jobs", string.Format("signup refused: %1 is not a delivery job", typename.EnumToString(LF_EJobType, job)));
			return false;
		}

		if (!CanSignUp(playerUid, job))
		{
			LF_Debug.Log("Jobs", string.Format("signup rejected: %1 level %2 below %3 for %4", playerUid, GetLevel(playerUid), RequiredLevel(job), typename.EnumToString(LF_EJobType, job)));
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		worker.m_eJob = job;
		worker.m_bSignedUp = true;
		LF_Debug.Log("Jobs", string.Format("signed up %1 -> %2 (level %3)", playerUid, typename.EnumToString(LF_EJobType, job), worker.m_iLevel));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool StartRun(string playerUid, LF_EJobType job)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "start refused: empty player id");
			return false;
		}

		if (!IsDeliveryJob(job))
		{
			LF_Debug.Error("Jobs", "start refused: not a delivery job");
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bSignedUp || worker.m_eJob != job)
		{
			LF_Debug.Log("Jobs", string.Format("start rejected: %1 is not signed up for %2", playerUid, typename.EnumToString(LF_EJobType, job)));
			return false;
		}

		if (worker.m_bActive)
		{
			LF_Debug.Log("Jobs", string.Format("start rejected: %1 already has a run", playerUid));
			return false;
		}

		if (!PrefabLoads(job))
			return false;

		worker.m_bActive = true;
		worker.m_eActiveJob = job;
		LF_Debug.Log("Jobs", string.Format("run started %1 job %2", playerUid, typename.EnumToString(LF_EJobType, job)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the active run once and grants job XP. A second call pays nothing.
	static int CompleteRun(string playerUid)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "complete refused: empty player id");
			return 0;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bActive)
		{
			LF_Debug.Log("Jobs", string.Format("complete rejected: %1 has no active run", playerUid));
			return 0;
		}

		int amount = PayFor(worker.m_eActiveJob);
		LF_EJobType finished = worker.m_eActiveJob;
		worker.m_bActive = false;
		worker.m_eActiveJob = LF_EJobType.UNEMPLOYED;
		worker.m_iPaidTotal = worker.m_iPaidTotal + amount;

		AddExperience(playerUid, XpPerRun());
		LF_Debug.Log("Jobs", string.Format("run paid %1 to %2", amount, playerUid));
		LF_EventBus.Publish(LF_OnJobCompleted.Create(playerUid, finished, amount));
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the active run with no pay.
	static bool AbandonRun(string playerUid)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "abandon refused: empty player id");
			return false;
		}

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bActive)
		{
			LF_Debug.Log("Jobs", string.Format("abandon rejected: %1 has no active run", playerUid));
			return false;
		}

		worker.m_bActive = false;
		worker.m_eActiveJob = LF_EJobType.UNEMPLOYED;
		LF_Debug.Log("Jobs", string.Format("run abandoned by %1, no pay", playerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the awarded cash on the player. A short pocket is logged. The run
	//! is already marked paid, so a full inventory does not pay a second time.
	static void HandCash(IEntity user, int amount)
	{
		if (!user || amount <= 0)
			return;

		int given = LF_MoneyUtils.GiveCash(user, amount, "job-pay");
		if (given != amount)
			LF_Debug.Warn("Jobs", string.Format("run payout short: paid %1 of %2 (player %3)", given, amount, user));
		else
			LF_Debug.Log("Jobs", string.Format("run cash +%1", given));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one parcel or fuel can on the player. Taxi uses the rank vehicle.
	//! A full inventory leaves no run and no leftover item.
	static bool GiveCargo(IEntity user, LF_EJobType job)
	{
		if (!HasCargo(job))
			return true;

		if (!user)
		{
			LF_Debug.Error("Jobs", "cargo give refused: no player");
			return false;
		}

		ResourceName prefab = PrefabFor(job);
		int added = LF_InventoryUtils.AddAmount(user, prefab, 1, false);
		if (added != 1)
		{
			LF_Debug.Error("Jobs", "cargo give failed: " + prefab);
			return false;
		}

		LF_Debug.Log("Jobs", string.Format("cargo given %1", prefab));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the parcel or fuel can. The fuel can is consumed here.
	static bool TakeCargo(IEntity user, LF_EJobType job)
	{
		if (!HasCargo(job))
			return true;

		if (!user)
		{
			LF_Debug.Error("Jobs", "cargo take refused: no player");
			return false;
		}

		ResourceName prefab = PrefabFor(job);
		int removed = LF_InventoryUtils.RemoveAmount(user, prefab, 1);
		if (removed != 1)
		{
			LF_Debug.Error("Jobs", "cargo missing at dropoff: " + prefab);
			return false;
		}

		if (job == LF_EJobType.FUEL_DELIVERY)
			LF_Debug.Log("Jobs", string.Format("fuel can consumed for %1", LF_Utils.GetPlayerUID(user)));
		else
			LF_Debug.Log("Jobs", string.Format("parcel delivered for %1", LF_Utils.GetPlayerUID(user)));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Job XP for the civilian career. LF_PlayerJobComponent.AddExperience
	//! forwards here. Each level needs XpForLevel(level); the remainder stays.
	static void AddExperience(string playerUid, float amount)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "experience refused: empty player id");
			return;
		}

		if (amount <= 0)
		{
			LF_Debug.Error("Jobs", "experience refused: non-positive amount");
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

		LF_Debug.Log("Jobs", string.Format("job xp +%1 -> level %2 xp %3 (%4)", amount, worker.m_iLevel, worker.m_fExperience, playerUid));
		if (worker.m_iLevel > levelBefore)
			LF_Debug.Log("Jobs", string.Format("job level -> %1 (%2)", worker.m_iLevel, playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! The delivery job that a rise from levelBefore to levelAfter opens, or UNEMPLOYED.
	//! The clerk mentions new work; the level itself is never shown.
	static LF_EJobType NewlyOpened(int levelBefore, int levelAfter)
	{
		if (levelAfter <= levelBefore)
			return LF_EJobType.UNEMPLOYED;

		if (levelBefore < RequiredLevel(LF_EJobType.TAXI) && levelAfter >= RequiredLevel(LF_EJobType.TAXI))
			return LF_EJobType.TAXI;

		if (levelBefore < RequiredLevel(LF_EJobType.FUEL_DELIVERY) && levelAfter >= RequiredLevel(LF_EJobType.FUEL_DELIVERY))
			return LF_EJobType.FUEL_DELIVERY;

		return LF_EJobType.UNEMPLOYED;
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
	protected static bool PrefabLoads(LF_EJobType job)
	{
		ResourceName prefab = PrefabFor(job);
		Resource loaded = Resource.Load(prefab);
		if (!loaded.IsValid())
		{
			LF_Debug.Error("Jobs", "run prefab missing: " + prefab);
			return false;
		}

		return true;
	}
}
