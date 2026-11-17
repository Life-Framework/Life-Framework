//------------------------------------------------------------------------------------------------
//! Civilian delivery work. Courier, fuel delivery, and taxi are taken at a
//! workplace. A run pays cash once. Abandoning it pays nothing. The career is the
//! DELIVERY job of LF_JobLevels (#749); LF_PlayerJobComponent.PullCareer copies its
//! level onto the player for the client-side offer.
//!
//! Level 1 may take courier work. Level 2 opens fuel delivery. Level 3 opens
//! taxi. XP per run is the jobs config's delivery_run award and the levels are the
//! DELIVERY track's thresholds (shipped: 100 XP a run, 500 XP to level 2, 1500 to
//! level 3: 5 runs to fuel delivery, 15 to the taxi). Progression is hidden: no level
//! text reaches the player.
//------------------------------------------------------------------------------------------------
class LF_JobWorker : Managed
{
	string m_sPlayerUid;
	LF_EJobType m_eJob;
	bool m_bSignedUp;
	bool m_bActive;
	int m_iPaidTotal;
	LF_EJobType m_eActiveJob;
	//! Where and when the active run started (set by LF_CivilianJob.BeginRun). A run with no start
	//! on record pays nothing.
	vector m_vRunStart;
	float m_fRunStartedAt;
	bool m_bRunTimed;
	//! Cash held against the cargo of the active run: returned on delivery, kept on abandon.
	int m_iDeposit;
}

//------------------------------------------------------------------------------------------------
//! How BeginRun ended.
enum LF_ERunStart
{
	STARTED,
	REFUSED,
	NO_DEPOSIT,
	NO_CARGO
}

//------------------------------------------------------------------------------------------------
class LF_CivilianJob
{
	//! The system id the Jobs switch answers to (#1074).
	static const string SYSTEM = "Jobs";

	static const int PAY_PARCEL = 40;
	static const int PAY_FUEL = 60;
	static const int PAY_TAXI = 80;

	//! The reason keys the drop-off refusal shows (Language/everonlife_localization.st).
	static const string KEY_TOO_CLOSE = "#LF-Job_TooClose";
	static const string KEY_TOO_FAST = "#LF-Job_TooFast";
	static const string KEY_NO_PASSENGER = "#LF-Job_NoPassenger";
	static const string KEY_NOT_DROP = "#LF-Job_NotADrop";
	static const string KEY_NO_RUN = "#EL-Job_NoActiveRun";
	static const string KEY_NEED_DEPOSIT = "#LF-Job_NeedDeposit";

	//! The ledger reasons of a deposit taken, given back and kept.
	static const string REASON_DEPOSIT = "job-deposit";
	static const string REASON_DEPOSIT_BACK = "job-deposit-back";

	//! The prefab slot (#1128): The courier parcel.
	static const string SLOT_PARCEL = "courier_parcel";

	//! The prefab (slot "courier_parcel"), or empty when no usable prefab resolves (logged).
	static ResourceName ParcelPrefab()
	{
		return LF_PrefabSlots.Get(SLOT_PARCEL);
	}
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
	//! Persistence seam: sets the saved career pay total. The career's XP is the DELIVERY job of
	//! LF_JobLevels (its own payload, or seeded from the progression record of an older save). The
	//! sign-up has its own payload (ApplySignUps) and an active run is session state; both are left
	//! alone, so a re-apply to a live session does not end a run.
	static void RestoreCareer(string playerUid, int paidTotal)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "career restore refused: empty player id");
			return;
		}

		LF_JobWorker worker = Worker(playerUid);
		worker.m_iPaidTotal = Math.Max(0, paidTotal);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the player's career and its DELIVERY XP (test cleanup, admin wipe).
	static void ForgetCareer(string playerUid)
	{
		if (s_mWorkers)
			s_mWorkers.Remove(playerUid);

		LF_JobLevels.ForgetJob(playerUid, LF_JobLevels.JOB_DELIVERY);
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

		return ParcelPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player's DELIVERY level (LF_JobLevels); 1 for an empty id.
	static int GetLevel(string playerUid)
	{
		return LF_JobLevels.LevelOf(playerUid, LF_JobLevels.JOB_DELIVERY);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player's total DELIVERY XP.
	static float GetExperience(string playerUid)
	{
		return LF_JobLevels.XpOf(playerUid, LF_JobLevels.JOB_DELIVERY);
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
		LF_Debug.Log("Jobs", string.Format("signed up %1 -> %2 (level %3)", playerUid, typename.EnumToString(LF_EJobType, job), GetLevel(playerUid)));
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
		worker.m_bRunTimed = false;
		worker.m_iDeposit = 0;
		LF_Debug.Log("Jobs", string.Format("run started %1 job %2", playerUid, typename.EnumToString(LF_EJobType, job)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Records where and when the active run started; the drop-off is measured against it.
	static void RecordRunStart(string playerUid, vector position, float now)
	{
		if (playerUid.IsEmpty())
			return;

		LF_JobWorker worker = Worker(playerUid);
		worker.m_vRunStart = position;
		worker.m_fRunStartedAt = now;
		worker.m_bRunTimed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The cash held against the active run's cargo.
	static int GetDeposit(string playerUid)
	{
		if (playerUid.IsEmpty())
			return 0;

		return Worker(playerUid).m_iDeposit;
	}

	//------------------------------------------------------------------------------------------------
	//! Records the cash held against the active run's cargo (the caller has taken it from the player).
	static void RecordDeposit(string playerUid, int amount)
	{
		if (playerUid.IsEmpty())
			return;

		Worker(playerUid).m_iDeposit = Math.Max(0, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the deposit off the run's record so it is handed back once.
	//! \return The amount to give back; 0 when none is held or it was already returned.
	static int ReleaseDeposit(string playerUid)
	{
		if (playerUid.IsEmpty())
			return 0;

		LF_JobWorker worker = Worker(playerUid);
		int amount = worker.m_iDeposit;
		worker.m_iDeposit = 0;
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The nearest a drop-off may be to where a run started, in metres (jobs config).
	static float RunMinDistance()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return LF_JobsConfig.DEFAULT_RUN_MIN_DISTANCE;

		return config.m_fRunMinDistance;
	}

	//------------------------------------------------------------------------------------------------
	protected static float RunMinTimeFraction()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return LF_JobsConfig.DEFAULT_RUN_MIN_TIME_FRACTION;

		return config.m_fRunMinTimeFraction;
	}

	//------------------------------------------------------------------------------------------------
	protected static float RunRoadSpeed()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return LF_JobsConfig.DEFAULT_RUN_ROAD_SPEED;

		return config.m_fRunRoadSpeed;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the least time a run of this straight-line distance may take, a share of the expected
	//! driving time (distance over the road speed).
	//! \return Seconds; 0 when the road speed is not positive.
	static float MinRunSeconds(float distance, float roadSpeed, float fraction)
	{
		if (roadSpeed <= 0)
			return 0;

		return distance / roadSpeed * fraction;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the deposit for cargo from its shop sell price. A negative price (the shop list is not
	//! configured) uses the fallback base.
	static int DepositAmount(int sellPrice, int percent, int fallback)
	{
		if (percent <= 0)
			return 0;

		int basis = sellPrice;
		if (basis < 0)
			basis = fallback;

		float scaled = basis;
		scaled = scaled * percent / 100;
		return Math.Round(scaled);
	}

	//------------------------------------------------------------------------------------------------
	//! The deposit a run of this job takes at the start: the cargo's shop sell price scaled by the
	//! jobs config. A job with no cargo (the taxi) takes none.
	static int DepositFor(LF_EJobType job)
	{
		if (!HasCargo(job))
			return 0;

		int percent = LF_JobsConfig.DEFAULT_CARGO_DEPOSIT_PERCENT;
		int fallback = LF_JobsConfig.DEFAULT_CARGO_DEPOSIT_FALLBACK;
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (config)
		{
			percent = config.m_iCargoDepositPercent;
			fallback = config.m_iCargoDepositFallback;
		}

		int sellPrice = -1;
		LF_ShopManagerComponent shop = LF_ShopManagerComponent.GetInstance();
		if (shop)
			sellPrice = shop.GetSellPrice(PrefabFor(job));

		return DepositAmount(sellPrice, percent, fallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the active run may be paid at a drop at `dropPos` at world time `now`: the drop is at
	//! least the configured distance from the run's start, the run took at least the configured share
	//! of the expected driving time at road speed, and a taxi fare has a seated passenger.
	//! \param[in] passengers Other players seated in the driver's vehicle (only a taxi fare reads it).
	//! \return "" when the run pays, else the reason key to show. A run with no start on record is refused.
	static string DropoffRefusal(string playerUid, vector dropPos, float now, int passengers)
	{
		if (playerUid.IsEmpty())
			return KEY_NO_RUN;

		LF_JobWorker worker = Worker(playerUid);
		if (!worker.m_bActive)
			return KEY_NO_RUN;

		if (!worker.m_bRunTimed)
		{
			LF_Debug.Error("Jobs", string.Format("drop-off refused: %1's run has no recorded start", playerUid));
			return KEY_NO_RUN;
		}

		float distance = vector.Distance(worker.m_vRunStart, dropPos);
		if (distance < RunMinDistance())
		{
			LF_Debug.Log("Jobs", string.Format("drop-off refused for %1: %2 m from the start, %3 m needed", playerUid, distance, RunMinDistance()));
			return KEY_TOO_CLOSE;
		}

		float elapsed = now - worker.m_fRunStartedAt;
		float needed = MinRunSeconds(distance, RunRoadSpeed(), RunMinTimeFraction());
		if (elapsed < needed)
		{
			LF_Debug.Log("Jobs", string.Format("drop-off refused for %1: %2 s on the run, %3 s needed", playerUid, elapsed, needed));
			return KEY_TOO_FAST;
		}

		if (worker.m_eActiveJob == LF_EJobType.TAXI && passengers < 1)
		{
			LF_Debug.Log("Jobs", string.Format("drop-off refused for %1: the fare has no passenger", playerUid));
			return KEY_NO_PASSENGER;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Other players seated in the vehicle the driver sits in; 0 on foot.
	static int CountPassengers(IEntity driver)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(driver);
		if (!character || !character.IsInVehicle())
			return 0;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return 0;

		BaseCompartmentSlot own = access.GetCompartment();
		if (!own)
			return 0;

		IEntity vehicle = own.GetVehicle();
		if (!vehicle)
			return 0;

		BaseCompartmentManagerComponent seats = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!seats || !GetGame().GetPlayerManager())
			return 0;

		int driverId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(driver);
		array<BaseCompartmentSlot> slots = {};
		seats.GetCompartments(slots);
		int passengers;
		foreach (BaseCompartmentSlot slot : slots)
		{
			IEntity occupant = slot.GetOccupant();
			if (!occupant || occupant == driver)
				continue;

			int occupantId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(occupant);
			if (occupantId > 0 && occupantId != driverId)
				passengers++;
		}

		return passengers;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the entity is a placed drop-off (an LF_JobSiteComponent of site DROP).
	static bool IsDropSite(IEntity site)
	{
		if (!site)
			return false;

		LF_JobSiteComponent marker = LF_Component<LF_JobSiteComponent>.Find(site);
		return marker && marker.GetSite() == LF_EJobSite.DROP;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts a run for a player. Signs the start (position, time) on the run, takes the cargo
	//! deposit from the player's cash and hands over the cargo. Nothing is kept when a step fails: a
	//! player short of the deposit gets no run and no cargo.
	//! \param[out] deposit The deposit this job asks (for the toast).
	static LF_ERunStart BeginRun(IEntity user, string uid, LF_EJobType job, float now, out int deposit)
	{
		deposit = 0;
		if (uid.IsEmpty() || !user)
		{
			LF_Debug.Error("Jobs", "start refused: no player or no account id");
			return LF_ERunStart.REFUSED;
		}

		if (!StartRun(uid, job))
			return LF_ERunStart.REFUSED;

		deposit = DepositFor(job);
		if (deposit > 0)
		{
			if (LF_MoneyUtils.GetCash(user) < deposit)
			{
				LF_Debug.Log("Jobs", string.Format("start refused for %1: the %2 deposit is not in cash", uid, deposit));
				CancelRun(uid);
				return LF_ERunStart.NO_DEPOSIT;
			}

			int taken = LF_MoneyUtils.TakeCash(user, deposit, REASON_DEPOSIT);
			if (taken != deposit)
			{
				LF_Debug.Warn("Jobs", string.Format("start refused for %1: took %2 of the %3 deposit, the rest is given back", uid, taken, deposit));
				if (taken > 0)
					LF_MoneyUtils.GiveCash(user, taken, REASON_DEPOSIT_BACK);

				CancelRun(uid);
				return LF_ERunStart.NO_DEPOSIT;
			}

			RecordDeposit(uid, deposit);
			LF_Debug.Log("Jobs", string.Format("deposit %1 taken from %2", deposit, uid));
		}

		RecordRunStart(uid, user.GetOrigin(), now);
		if (!GiveCargo(user, job))
		{
			GiveDepositBack(user, uid);
			CancelRun(uid);
			return LF_ERunStart.NO_CARGO;
		}

		return LF_ERunStart.STARTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the drop-off of the active run at a placed drop site. Refused (0) unless the site is a
	//! drop and DropoffRefusal passes; then takes the cargo, pays the run, returns the deposit and
	//! hands over the cash.
	//! \param[out] refusal The reason key when the drop is refused for a rule, else "".
	//! \return The pay of the run, or 0 when nothing was paid.
	static int Dropoff(IEntity user, string uid, IEntity site, float now, out string refusal)
	{
		refusal = string.Empty;
		if (uid.IsEmpty() || !user)
		{
			LF_Debug.Error("Jobs", "drop-off refused: no player or no account id");
			refusal = KEY_NO_RUN;
			return 0;
		}

		if (!HasActiveRun(uid))
		{
			refusal = KEY_NO_RUN;
			return 0;
		}

		if (!IsDropSite(site))
		{
			LF_Debug.Error("Jobs", "drop-off refused: the entity is not a placed drop-off (needs an LF_JobSiteComponent of site DROP)");
			refusal = KEY_NOT_DROP;
			return 0;
		}

		LF_EJobType finished = GetActiveJob(uid);
		int passengers;
		if (finished == LF_EJobType.TAXI)
			passengers = CountPassengers(user);

		refusal = DropoffRefusal(uid, site.GetOrigin(), now, passengers);
		if (!refusal.IsEmpty())
			return 0;

		if (!TakeCargo(user, finished))
			return 0;

		int paid = CompleteRun(uid);
		if (paid <= 0)
			return 0;

		GiveDepositBack(user, uid);
		HandCash(user, paid);
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the run's deposit back to the player once.
	protected static void GiveDepositBack(IEntity user, string playerUid)
	{
		int amount = ReleaseDeposit(playerUid);
		if (amount <= 0)
			return;

		int given = LF_MoneyUtils.GiveCash(user, amount, REASON_DEPOSIT_BACK);
		if (given != amount)
			LF_Debug.Warn("Jobs", string.Format("deposit back short: gave %1 of %2 (player %3)", given, amount, playerUid));
		else
			LF_Debug.Log("Jobs", string.Format("deposit %1 returned to %2", amount, playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a run that never got going (a failed start): no pay, no forfeit, the caller has already
	//! handed any deposit back.
	protected static void CancelRun(string playerUid)
	{
		LF_JobWorker worker = Worker(playerUid);
		worker.m_bActive = false;
		worker.m_eActiveJob = LF_EJobType.UNEMPLOYED;
		worker.m_bRunTimed = false;
		worker.m_iDeposit = 0;
		LF_JobMapMarks.OnStopDone(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the active run once and grants job XP. A second call pays nothing. This is the payout
	//! only: the drop-off rules (distance, time, passenger) are Dropoff's, and the deposit is the
	//! caller's to return (ReleaseDeposit).
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
		worker.m_bRunTimed = false;
		worker.m_iPaidTotal = worker.m_iPaidTotal + amount;

		LF_JobLevels.Award(playerUid, LF_JobLevels.ACTION_DELIVERY_RUN);
		LF_Debug.Log("Jobs", string.Format("run paid %1 to %2", amount, playerUid));
		LF_EventBus.Publish(LF_OnJobCompleted.Create(playerUid, finished, amount));
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the active run with no pay. The cargo deposit is kept (forfeited): the player keeps the
	//! cargo, and the deposit is what it is worth.
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

		int forfeited = worker.m_iDeposit;
		worker.m_bActive = false;
		worker.m_eActiveJob = LF_EJobType.UNEMPLOYED;
		worker.m_bRunTimed = false;
		worker.m_iDeposit = 0;
		LF_Debug.Log("Jobs", string.Format("run abandoned by %1, no pay, deposit %2 kept", playerUid, forfeited));
		LF_JobMapMarks.OnStopDone(playerUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the awarded cash on the player. A short pocket is logged. The run
	//! is already marked paid, so a full inventory does not pay a second time.
	static void HandCash(IEntity user, int amount)
	{
		if (!user || amount <= 0)
			return;

		if (LF_Systems.Refuses(SYSTEM, "run payout"))
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
	//! Job XP for the civilian career (the DELIVERY job of LF_JobLevels).
	//! LF_PlayerJobComponent.AddExperience forwards here.
	static void AddExperience(string playerUid, float amount)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "experience refused: empty player id");
			return;
		}

		int whole = Math.Round(amount);
		if (whole <= 0)
		{
			LF_Debug.Error("Jobs", "experience refused: non-positive amount");
			return;
		}

		LF_JobLevels.AddXp(playerUid, LF_JobLevels.JOB_DELIVERY, whole, "delivery");
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
	//! Job experience one completed run grants (the jobs config's delivery_run award; 0 when none).
	static int XpPerRun()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return 0;

		LF_JobXpAwardDef award = config.FindAward(LF_JobLevels.ACTION_DELIVERY_RUN);
		if (!award)
			return 0;

		return award.m_iXp;
	}

	//------------------------------------------------------------------------------------------------
	//! Completed runs a new worker needs to reach level (pacing, tests, docs); -1 when the DELIVERY
	//! track stops below that level or a run earns nothing.
	static int RunsToReach(int level)
	{
		LF_JobTrackDef track = LF_JobLevels.Track(LF_JobLevels.JOB_DELIVERY);
		int perRun = XpPerRun();
		if (!track || perRun <= 0)
			return -1;

		int needed = track.XpForLevel(level);
		if (needed < 0)
			return -1;

		float neededXp = needed;
		return Math.Ceil(neededXp / perRun);
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
