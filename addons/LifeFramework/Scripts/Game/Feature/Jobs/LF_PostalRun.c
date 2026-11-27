//------------------------------------------------------------------------------------------------
//! Postal van run numbers: Configs/Jobs/LF_PostalRun.conf. The pay is the jobs config's run pay row
//! "POSTAL" (LF_CivilianJob.PayFor's table) and the XP its "postal_run" award, so the money and level
//! numbers stay in the one jobs table; this file holds the run's own rules.
[BaseContainerProps(configRoot: true)]
class LF_PostalRunConfig
{
	static const ResourceName CONFIG_PATH = "{3F7A1C5E9B2D4086}Configs/Jobs/LF_PostalRun.conf";
	//! The name pack overlay log lines use; there is no owner section.
	static const string OVERLAY_NAME = "postalrun";
	static const float MIN_DISTANCE_LOW = 50;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float MIN_DISTANCE_HIGH = 20000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const ResourceName DEFAULT_VAN ="{E1A1203BE1A00001}Prefabs/Vehicles/Wheeled/S1203/EL_S1203_transport_beige.et";

	[Attribute("4", UIWidgets.Auto, "Postal run: mail sacks loaded at the home office and unloaded at the far one, one action each", params: "1 20")]
	int m_iSacks;

	[Attribute("800", UIWidgets.Auto, "Postal run: the least distance (metres) from the home office to the far one; the nearest office past it is the run's destination", params: "50 20000")]
	float m_fMinDistance;

	[Attribute("18", UIWidgets.Auto, "Postal run: how close (metres) the van must stand to an office to load, unload or hand in", params: "3 100")]
	float m_fVanReach;

	[Attribute("420", UIWidgets.Auto, "Postal run: the least seconds from taking the van to handing in; a faster hand-in pays nothing", params: "0 36000")]
	int m_iMinRunSeconds;

	[Attribute("2700", UIWidgets.Auto, "Postal run: a run still open after this many seconds is called off (no pay, the van goes back)", params: "60 36000")]
	int m_iMaxRunSeconds;

	[Attribute("350", UIWidgets.Auto, "Postal run: the driver farther than this (metres) from the van, or off the server, counts as away", params: "20 5000")]
	float m_fAbandonMetres;

	[Attribute("180", UIWidgets.Auto, "Postal run: seconds away from the van before the run is called off as abandoned", params: "10 3600")]
	int m_iAbandonSeconds;

	[Attribute("60", UIWidgets.Auto, "Postal run: seconds after a run ends before the same player can take another van", params: "0 3600")]
	int m_iStartCooldownSeconds;

	[Attribute("1", UIWidgets.CheckBox, "Postal run: the driver must hold a driving licence")]
	bool m_bNeedsDrivingLicence;

	[Attribute("{E1A1203BE1A00001}Prefabs/Vehicles/Wheeled/S1203/EL_S1203_transport_beige.et", UIWidgets.ResourcePickerThumbnail, "Postal run: the van lent when the office names none (LF_PostalPointComponent.m_Van)", "et")]
	ResourceName m_Van;

	protected static ref LF_PostalRunConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_PostalRunConfig CreateDefault()
	{
		LF_PostalRunConfig config = new LF_PostalRunConfig();
		config.m_iSacks = 4;
		config.m_fMinDistance = 800;
		config.m_fVanReach = 18;
		config.m_iMinRunSeconds = 420;
		config.m_iMaxRunSeconds = 2700;
		config.m_fAbandonMetres = 350;
		config.m_iAbandonSeconds = 180;
		config.m_iStartCooldownSeconds = 60;
		config.m_bNeedsDrivingLicence = true;
		config.m_Van = DEFAULT_VAN;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config; the shipped numbers (logged) when the file does not resolve.
	static LF_PostalRunConfig Get()
	{
		if (s_bLoadAttempted && s_Config)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_PostalRunConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error("Jobs", string.Format("postal run config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = CreateDefault();
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Lays each pack overlay of this class over the core values, in load order (LF_ConfigOverlay).
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_PostalRunConfig, OVERLAY_NAME, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i]);
			LF_Debug.Info("Jobs", string.Format("postal run: pack overlay %1 changed %2 fields (far office at least %3 m)", names[i], changed, m_fMinDistance));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the fields one pack overlay writes directly. Only the office distance is a pack's to
	//! set; a value outside the schema range is refused and logged, the earlier value stays.
	//! \return Fields changed.
	int ApplyPackContainer(notnull BaseContainer root, string source)
	{
		float value;
		if (!root.IsVariableSetDirectly("m_fMinDistance") || !root.Get("m_fMinDistance", value))
			return 0;

		if (value < MIN_DISTANCE_LOW || value > MIN_DISTANCE_HIGH)
		{
			LF_Debug.Error("Jobs", string.Format("postal run: %1 sets m_fMinDistance %2, outside %3 to %4; keeping %5", source, value, MIN_DISTANCE_LOW, MIN_DISTANCE_HIGH, m_fMinDistance));
			return 0;
		}

		m_fMinDistance = value;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_PostalRunConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}
}

//------------------------------------------------------------------------------------------------
//! What a postal run step did; the toast key of each refusal is `LF_PostalRun.RefusalKey`.
enum LF_EPostalResult
{
	OK = 0,
	NO_RUN = 1,		// the player has no run open
	ALREADY = 2,	// a run is open: one van per player
	COOLDOWN = 3,	// a run ended a moment ago
	NO_LICENCE = 4,	// no driving licence
	NO_OFFICE = 5,	// no other post office far enough away
	NO_SPOT = 6,	// no clear spot off the road for the van
	NO_VAN = 7,		// the van did not spawn
	WRONG_OFFICE = 8,	// this office is not the one this step needs
	VAN_FAR = 9,	// the van is not at this office
	NOT_LOADED = 10,	// a sack is still to load or unload
	ALL_DONE = 11,	// every sack of this step is already moved
	TOO_FAST = 12,	// handed in faster than the run's least time
	UNFIT = 13		// dead, downed or cuffed
}

//------------------------------------------------------------------------------------------------
//! One player's open postal run. Server only, never saved: a restart ends it and the van is gone too.
class LF_PostalRunState
{
	string m_sUid;
	int m_iPlayerId;
	EntityID m_Origin = EntityID.INVALID;
	EntityID m_Destination = EntityID.INVALID;
	EntityID m_Van = EntityID.INVALID;
	EntityID m_Key = EntityID.INVALID;
	int m_iLoaded;
	int m_iUnloaded;
	float m_fStartedAt;
	//! Whether the driver is away from the van, and since when (LF_JobGuards.Now).
	bool m_bAway;
	float m_fAwaySince;
	//! The driver has been told the run is about to be called off.
	bool m_bWarned;
}

//------------------------------------------------------------------------------------------------
//! The postal van run: a beginner delivery job of the DELIVERY career. At a post office
//! (LF_PostalPointComponent, registered in LF_Places as "PostalPoint") the player takes the run: a
//! company van is lent on a clear spot off the road beside the office, locked, with its key in the
//! player's pocket (the ignition needs it, so only the key holder drives). The player loads the mail
//! sacks at the home office, drives to the nearest other office at least m_fMinDistance away (a map
//! mark and waypoint), unloads there, drives back and hands in: paid once through the job cash path,
//! DELIVERY XP, the van and key taken back. A destroyed van, a driver away from the van for
//! m_iAbandonSeconds, or a run past m_iMaxRunSeconds is called off with no pay. The van is never an
//! owned car (no record, out of the world save) and is deleted when the run ends however it ends.
class LF_PostalRun
{
	static const string FEATURE = "Jobs";
	static const string PLACE_KIND = "PostalPoint";
	static const string JOB_ROW = "POSTAL";
	static const string XP_ACTION = "postal_run";
	static const string CASH_REASON = "postal-run";
	static const string TITLE_KEY = "#LF-Postal_Title";

	//! How often (ms) the watch checks the open runs for a lost or abandoned van; runs only while one is open.
	protected static const int WATCH_MS = 5000; // lf-fixed: an internal poll interval, not a game number
	//! Spot search around the office: rings and steps (metres), the van's clear box half size.
	protected static const float SPOT_RING_FIRST = 7; // lf-fixed: geometry, a van's length from the office wall
	protected static const float SPOT_RING_STEP = 3; // lf-fixed: geometry
	protected static const int SPOT_RINGS = 6; // lf-fixed: geometry, out to 22 m
	protected static const int SPOT_ANGLES = 12; // lf-fixed: geometry, every 30 degrees
	protected static const float SPOT_MAX_STEP = 0.6; // lf-fixed: geometry, the most the ground may rise across the van

	protected static ref map<string, ref LF_PostalRunState> s_mRuns;
	protected static ref map<string, float> s_mEndedAt;
	protected static bool s_bWatchOn;
	//! Tests: an office the spot search treats as having no clear spot.
	protected static bool s_bTestNoSpot;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PostalRunState> Runs()
	{
		if (!s_mRuns)
			s_mRuns = new map<string, ref LF_PostalRunState>();

		return s_mRuns;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasRun(string uid)
	{
		return !uid.IsEmpty() && Runs().Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	static LF_PostalRunState GetRun(string uid)
	{
		LF_PostalRunState run;
		if (uid.IsEmpty() || !Runs().Find(uid, run))
			return null;

		return run;
	}

	//------------------------------------------------------------------------------------------------
	static int RunCount()
	{
		return Runs().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true for a van lent by an open run (or one waiting for its seats to empty after it).
	static bool IsRunVan(IEntity vehicle)
	{
		if (!vehicle || !s_mRuns)
			return false;

		foreach (string uid, LF_PostalRunState run : s_mRuns)
		{
			if (run && run.m_Van == vehicle.GetID())
				return true;
		}

		return LF_GarbageRound.IsPendingRemoval(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! What a finished run pays: the jobs config's "POSTAL" row, or LF_CivilianJob.PAY_POSTAL.
	static int Pay()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return LF_CivilianJob.PAY_POSTAL;

		return config.RunPayFor(JOB_ROW, LF_CivilianJob.PAY_POSTAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The night rate's extra on `basePay` for a run handed in now: the POSTAL row's night rate, the
	//! same window and rule as the other delivery runs (LF_CivilianJob.NightPay).
	static int NightPay(int basePay)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return 0;

		float rate = config.NightRateFor(JOB_ROW);
		if (rate <= 1)
			return 0;

		float hour = LF_CivilianJob.ClockHour();
		if (hour < 0)
			return 0;

		int startHour, endHour;
		config.NightWindow(startHour, endHour);
		if (!LF_CivilianJob.IsNightHour(hour, startHour, endHour))
			return 0;

		return LF_CivilianJob.NightExtra(basePay, rate);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The toast key that says why a step was refused; "" for OK.
	static string RefusalKey(LF_EPostalResult result)
	{
		switch (result)
		{
			case LF_EPostalResult.NO_RUN: return "#LF-Postal_NoRun";
			case LF_EPostalResult.ALREADY: return "#LF-Postal_Already";
			case LF_EPostalResult.COOLDOWN: return "#LF-Postal_Cooldown";
			case LF_EPostalResult.NO_LICENCE: return "#LF-Postal_NoLicence";
			case LF_EPostalResult.NO_OFFICE: return "#LF-Postal_NoOffice";
			case LF_EPostalResult.NO_SPOT: return "#LF-Postal_NoSpot";
			case LF_EPostalResult.NO_VAN: return "#LF-Postal_NoSpot";
			case LF_EPostalResult.WRONG_OFFICE: return "#LF-Postal_WrongOffice";
			case LF_EPostalResult.VAN_FAR: return "#LF-Postal_VanFar";
			case LF_EPostalResult.NOT_LOADED: return "#LF-Postal_NotLoaded";
			case LF_EPostalResult.ALL_DONE: return "#LF-Postal_AllDone";
			case LF_EPostalResult.TOO_FAST: return "#LF-Postal_TooFast";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The least minutes a run takes before a hand-in pays, rounded up (for the stand and the refusal).
	static int MinMinutes()
	{
		return (LF_PostalRunConfig.Get().m_iMinRunSeconds + 59) / 60;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the destination for a run from `origin`: the nearest of `offices` at least `minDistance`
	//! away on the ground plane. \return Its index, -1 when none is far enough.
	static int PickDestination(vector origin, notnull array<vector> offices, float minDistance)
	{
		int best = -1;
		float bestDistance;
		foreach (int i, vector office : offices)
		{
			float distance = vector.DistanceXZ(origin, office);
			if (distance < minDistance)
				continue;

			if (best < 0 || distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player takes the run at `office`: a van is lent beside it with its key, the sacks
	//! wait to be loaded. One run per player; a refusal changes nothing.
	static LF_EPostalResult Start(IEntity user, string uid, IEntity office)
	{
		if (!Replication.IsServer() || !user || !office)
			return LF_EPostalResult.NO_RUN;

		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "postal run start refused: player has no account id");
			return LF_EPostalResult.NO_RUN;
		}

		if (!CanWork(user, uid, "start"))
			return LF_EPostalResult.UNFIT;

		if (HasRun(uid))
			return LF_EPostalResult.ALREADY;

		LF_PostalRunConfig config = LF_PostalRunConfig.Get();
		float endedAt;
		if (s_mEndedAt && s_mEndedAt.Find(uid, endedAt) && LF_JobGuards.Now() - endedAt < config.m_iStartCooldownSeconds)
			return LF_EPostalResult.COOLDOWN;

		if (config.m_bNeedsDrivingLicence && !LF_LicenceDesk.HoldsLicence(user, uid, LF_ELicenseType.DRIVING_LICENCE))
		{
			LF_Debug.Info(FEATURE, string.Format("postal run start refused for %1: no driving licence", uid));
			return LF_EPostalResult.NO_LICENCE;
		}

		IEntity destination = FindDestination(office, config.m_fMinDistance);
		if (!destination)
		{
			LF_Debug.Warn(FEATURE, string.Format("postal run start refused: no other post office %1 m or more from %2", config.m_fMinDistance, office.GetOrigin()));
			return LF_EPostalResult.NO_OFFICE;
		}

		vector spot, angles;
		if (!FindVanSpot(office, spot, angles))
		{
			LF_Debug.Warn(FEATURE, string.Format("postal run start refused: no clear spot off the road for the van at %1", office.GetOrigin()));
			return LF_EPostalResult.NO_SPOT;
		}

		ResourceName vanPrefab = VanFor(office, config);
		IEntity van = LF_Utils.SpawnEntityPrefab(vanPrefab, spot, angles);
		if (!van)
		{
			LF_Debug.Error(FEATURE, string.Format("postal run: the van did not spawn: %1", vanPrefab));
			return LF_EPostalResult.NO_VAN;
		}

		// A lent van is never a car anyone keeps: out of the engine's world save, no ownership record.
		LF_VehicleWorldSave.Hold(van);

		LF_PostalRunState run = new LF_PostalRunState();
		run.m_sUid = uid;
		run.m_iPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		run.m_Origin = office.GetID();
		run.m_Destination = destination.GetID();
		run.m_Van = van.GetID();
		run.m_fStartedAt = LF_JobGuards.Now();
		HandKey(user, van, run);
		Runs().Set(uid, run);
		WatchOn();

		LF_JobMapMarks.OnStop(uid, "postal-load", office.GetOrigin(), "#LF-JobMap_PostLoad");
		LF_Debug.Info(FEATURE, string.Format("postal run started by %1: van %2 at %3, destination %4 m away", uid, vanPrefab, spot, Math.Round(vector.DistanceXZ(office.GetOrigin(), destination.GetOrigin()))));
		return LF_EPostalResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one mail sack at `office`: loaded into the van at the home office, unloaded from it at
	//! the destination. The van must stand within reach. The last unload sends the player home.
	static LF_EPostalResult Sack(IEntity user, string uid, IEntity office)
	{
		if (!Replication.IsServer() || !user || !office)
			return LF_EPostalResult.NO_RUN;

		LF_PostalRunState run = GetRun(uid);
		if (!run)
			return LF_EPostalResult.NO_RUN;

		if (!CanWork(user, uid, "sack"))
			return LF_EPostalResult.UNFIT;

		LF_PostalRunConfig config = LF_PostalRunConfig.Get();
		bool atHome = office.GetID() == run.m_Origin;
		bool atDestination = office.GetID() == run.m_Destination;
		if (!atHome && !atDestination)
			return LF_EPostalResult.WRONG_OFFICE;

		if (!VanAt(run, office, config))
			return LF_EPostalResult.VAN_FAR;

		if (atHome)
		{
			if (run.m_iLoaded >= config.m_iSacks)
			{
				if (run.m_iUnloaded < config.m_iSacks)
					return LF_EPostalResult.WRONG_OFFICE;

				return LF_EPostalResult.ALL_DONE;
			}

			run.m_iLoaded++;
			LF_Debug.Info(FEATURE, string.Format("postal run: %1 loaded sack %2 of %3", uid, run.m_iLoaded, config.m_iSacks));
			if (run.m_iLoaded == config.m_iSacks)
				MarkNext(run);

			return LF_EPostalResult.OK;
		}

		if (run.m_iLoaded < config.m_iSacks)
			return LF_EPostalResult.NOT_LOADED;

		if (run.m_iUnloaded >= config.m_iSacks)
			return LF_EPostalResult.ALL_DONE;

		run.m_iUnloaded++;
		LF_Debug.Info(FEATURE, string.Format("postal run: %1 unloaded sack %2 of %3", uid, run.m_iUnloaded, config.m_iSacks));
		if (run.m_iUnloaded == config.m_iSacks)
			MarkNext(run);

		return LF_EPostalResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player hands the run in at the home office: every sack unloaded at the far office,
	//! the van back within reach, the run's least time passed. Pays once, closes the run, takes the van.
	//! \param[out] paid The cash paid, 0 on a refusal.
	static LF_EPostalResult HandIn(IEntity user, string uid, IEntity office, out int paid)
	{
		paid = 0;
		if (!Replication.IsServer() || !user || !office)
			return LF_EPostalResult.NO_RUN;

		LF_PostalRunState run = GetRun(uid);
		if (!run)
			return LF_EPostalResult.NO_RUN;

		if (!CanWork(user, uid, "hand-in"))
			return LF_EPostalResult.UNFIT;

		LF_PostalRunConfig config = LF_PostalRunConfig.Get();
		if (office.GetID() != run.m_Origin)
			return LF_EPostalResult.WRONG_OFFICE;

		if (run.m_iUnloaded < config.m_iSacks)
			return LF_EPostalResult.NOT_LOADED;

		if (!VanAt(run, office, config))
			return LF_EPostalResult.VAN_FAR;

		if (LF_JobGuards.Now() - run.m_fStartedAt < config.m_iMinRunSeconds)
		{
			LF_Debug.Warn(FEATURE, string.Format("postal run hand-in refused for %1: %2 s, the least is %3 s", uid, Math.Round(LF_JobGuards.Now() - run.m_fStartedAt), config.m_iMinRunSeconds));
			return LF_EPostalResult.TOO_FAST;
		}

		int basePay = Pay();
		paid = basePay + NightPay(basePay);
		Close(uid, "handed in");
		LF_CivilianJob.HandCash(user, paid, CASH_REASON);
		LF_JobLevels.Award(uid, XP_ACTION);
		LF_Debug.Info(FEATURE, string.Format("postal run handed in by %1: paid %2", uid, paid));
		return LF_EPostalResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player gives the run up: no pay, the van and its key go back.
	static bool GiveUp(string uid)
	{
		if (!HasRun(uid))
			return false;

		Close(uid, "given up");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: closes the run (no pay here), takes the key and the van back and clears the map mark.
	//! A van with someone aboard is deleted once its seats are empty (LF_GarbageRound.ReturnTruck).
	static void Close(string uid, string why)
	{
		LF_PostalRunState run = GetRun(uid);
		if (!run)
			return;

		Runs().Remove(uid);
		if (!s_mEndedAt)
			s_mEndedAt = new map<string, float>();

		s_mEndedAt.Set(uid, LF_JobGuards.Now());
		BaseWorld world = GetGame().GetWorld();
		IEntity key = world.FindEntityByID(run.m_Key);
		if (key)
			SCR_EntityHelper.DeleteEntityAndChildren(key);

		LF_GarbageRound.ReturnTruck(world.FindEntityByID(run.m_Van));
		LF_JobMapMarks.OnStopDone(uid);
		LF_Debug.Info(FEATURE, string.Format("postal run of %1 closed: %2", uid, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every WATCH_MS, the open runs: a destroyed or missing van, a driver away too long or a
	//! run past its time ends the run with no pay. Stops itself when no run is open.
	static void Watch()
	{
		if (!s_mRuns)
			return;

		LF_PostalRunConfig config = LF_PostalRunConfig.Get();
		float now = LF_JobGuards.Now();
		array<string> lost = {};
		array<string> why = {};
		foreach (string uid, LF_PostalRunState run : s_mRuns)
		{
			string verdict = Judge(run, config, now);
			if (verdict.IsEmpty())
				continue;

			lost.Insert(uid);
			why.Insert(verdict);
		}

		foreach (int i, string uid : lost)
		{
			LF_PostalRunState run = GetRun(uid);
			IEntity driver;
			if (run)
				driver = PlayerOf(run);

			Close(uid, why[i]);
			if (driver)
				LF_Utils.NotifyEntity(driver, LostKey(why[i]), TITLE_KEY, 6.0);
		}

		if (s_mRuns.IsEmpty())
			WatchOff();
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key for a run that ended on its own (`Judge`'s verdict).
	static string LostKey(string why)
	{
		if (why == "abandoned")
			return "#LF-Postal_Lost_abandoned";
		if (why == "late")
			return "#LF-Postal_Lost_late";

		return "#LF-Postal_Lost_van";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the driver, away for `awayFor` seconds of the `abandonAfter` allowed, is told now:
	//! once two thirds of the time are gone.
	static bool WarnDue(float awayFor, int abandonAfter)
	{
		return awayFor >= abandonAfter * 2 / 3;
	}

	//------------------------------------------------------------------------------------------------
	//! One run's verdict: "" while it is fine, else why it ends ("van", "abandoned", "late").
	//! Updates the run's away clock.
	static string Judge(notnull LF_PostalRunState run, notnull LF_PostalRunConfig config, float now)
	{
		IEntity van = GetGame().GetWorld().FindEntityByID(run.m_Van);
		if (!van || IsDestroyed(van))
			return "van";

		if (now - run.m_fStartedAt > config.m_iMaxRunSeconds)
			return "late";

		IEntity driver = PlayerOf(run);
		bool away = !driver || vector.Distance(driver.GetOrigin(), van.GetOrigin()) > config.m_fAbandonMetres;
		if (!away)
		{
			run.m_bAway = false;
			run.m_bWarned = false;
			return string.Empty;
		}

		if (!run.m_bAway)
		{
			run.m_bAway = true;
			run.m_fAwaySince = now;
			LF_Debug.Info(FEATURE, string.Format("postal run of %1: driver away from the van", run.m_sUid));
		}

		if (now - run.m_fAwaySince >= config.m_iAbandonSeconds)
			return "abandoned";

		// A warning first, while there is still time to walk back: a fading toast at the end is too late.
		if (!run.m_bWarned && driver && WarnDue(now - run.m_fAwaySince, config.m_iAbandonSeconds))
		{
			run.m_bWarned = true;
			int left = Math.Round(config.m_iAbandonSeconds - (now - run.m_fAwaySince));
			LF_Utils.NotifyEntity(driver, "#LF-Postal_Warn", TITLE_KEY, 8.0, left.ToString());
			LF_Debug.Info(FEATURE, string.Format("postal run of %1: driver warned, %2 s left", run.m_sUid, left));
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDestroyed(notnull IEntity van)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(van.FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! The run's driver, or null when they are off the server or between lives.
	protected static IEntity PlayerOf(notnull LF_PostalRunState run)
	{
		if (!GetGame().GetPlayerManager())
			return null;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(run.m_iPlayerId);
		if (!character || LF_Utils.GetPlayerUid(character) != run.m_sUid)
			return null;

		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! The van stands within reach of `office`.
	protected static bool VanAt(notnull LF_PostalRunState run, notnull IEntity office, notnull LF_PostalRunConfig config)
	{
		IEntity van = GetGame().GetWorld().FindEntityByID(run.m_Van);
		return van && !IsDestroyed(van) && vector.DistanceXZ(van.GetOrigin(), office.GetOrigin()) <= config.m_fVanReach;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the run's next office on the map: the far office once loaded, home once unloaded.
	protected static void MarkNext(notnull LF_PostalRunState run)
	{
		EntityID next = run.m_Destination;
		string stop = "postal-unload";
		string label = "#LF-JobMap_PostUnload";
		if (run.m_iUnloaded >= LF_PostalRunConfig.Get().m_iSacks)
		{
			next = run.m_Origin;
			stop = "postal-return";
			label = "#LF-JobMap_PostReturn";
		}

		IEntity office = GetGame().GetWorld().FindEntityByID(next);
		if (office)
			LF_JobMapMarks.OnStop(run.m_sUid, stop, office.GetOrigin(), label);
	}

	//------------------------------------------------------------------------------------------------
	//! The van the office lends: its own (a pack livery), else the config's.
	protected static ResourceName VanFor(notnull IEntity office, notnull LF_PostalRunConfig config)
	{
		LF_PostalPointComponent point = LF_Component<LF_PostalPointComponent>.Find(office);
		if (point && !point.GetVan().IsEmpty())
			return point.GetVan();

		return config.m_Van;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest other post office at least `minDistance` from `office` (LF_Places, by component).
	protected static IEntity FindDestination(notnull IEntity office, float minDistance)
	{
		array<IEntity> offices = {};
		LF_Places.FindAll(PLACE_KIND, offices);
		array<IEntity> others = {};
		array<vector> positions = {};
		foreach (IEntity other : offices)
		{
			if (!other || other == office)
				continue;

			others.Insert(other);
			positions.Insert(other.GetOrigin());
		}

		int pick = PickDestination(office.GetOrigin(), positions, minDistance);
		if (pick < 0)
			return null;

		return others[pick];
	}

	//------------------------------------------------------------------------------------------------
	//! The lent van's key in the player's pocket, the van locked; the ignition needs the key (the van
	//! prefab's LF_VehicleLockComponent), so only the key holder drives it. A van with no lock is lent
	//! open (logged): a pack that wants it keyed gives its van the lock.
	protected static void HandKey(notnull IEntity user, notnull IEntity van, notnull LF_PostalRunState run)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(van);
		if (!vehicleLock)
		{
			LF_Debug.Warn(FEATURE, string.Format("postal van %1 has no LF_VehicleLockComponent: lent with no key", LF_Utils.GetPrefabName(van)));
			return;
		}

		string id = vehicleLock.GetVehicleIdentifier();
		if (id.IsEmpty())
		{
			id = PersistenceIdUtils.Generate();
			vehicleLock.SetVehicleIdentifier(id);
		}

		IEntity key = LF_VehicleOwnership.SpawnKey(id, user.GetOrigin());
		if (!key)
			return;

		LF_VehicleOwnership.GiveKey(user, key);
		vehicleLock.SetLocked(true);
		run.m_Key = key.GetID();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a clear spot for the van near `office`: off the road, on even ground, nothing in a
	//! van-sized box. Searches rings around the office, the side away from its front first.
	static bool FindVanSpot(notnull IEntity office, out vector spot, out vector angles)
	{
		spot = vector.Zero;
		angles = vector.Zero;
		if (s_bTestNoSpot)
			return false;

		BaseWorld world = GetGame().GetWorld();
		vector origin = office.GetOrigin();
		vector back = office.GetTransformAxis(2);
		back[1] = 0;
		if (back.LengthSq() < 0.01)
			back = "0 0 1";

		back.Normalize();
		float baseYaw = back.ToYaw();
		for (int ring = 0; ring < SPOT_RINGS; ring++)
		{
			float radius = SPOT_RING_FIRST + ring * SPOT_RING_STEP;
			for (int step = 0; step < SPOT_ANGLES; step++)
			{
				// 0, +30, -30, +60, -60 ... from straight behind the office.
				int half = (step + 1) / 2;
				float turn = half * 360 / SPOT_ANGLES;
				if (step % 2 == 1)
					turn = -turn;

				float yaw = baseYaw + turn;
				vector dir = Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
				vector at = origin + dir * radius;
				at[1] = world.GetSurfaceY(at[0], at[2]);
				if (LF_PlacementZones.IsOnRoad(at))
					continue;

				// The van parks across the line from the office: its side to the office.
				float vanYaw = yaw + 90;
				if (!GroundEven(world, at, vanYaw) || !BoxClear(world, at, vanYaw, office))
					continue;

				spot = at;
				angles = Vector(0, vanYaw, 0);
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GroundEven(notnull BaseWorld world, vector at, float yaw)
	{
		vector fwd = Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
		vector side = Vector(fwd[2], 0, -fwd[0]);
		float low = at[1];
		float high = at[1];
		array<vector> corners = {at + fwd * 2.6 + side * 1.1, at + fwd * 2.6 - side * 1.1, at - fwd * 2.6 + side * 1.1, at - fwd * 2.6 - side * 1.1};
		foreach (vector corner : corners)
		{
			float y = world.GetSurfaceY(corner[0], corner[2]);
			low = Math.Min(low, y);
			high = Math.Max(high, y);
		}

		return high - low <= SPOT_MAX_STEP;
	}

	//------------------------------------------------------------------------------------------------
	//! Nothing solid in a van-sized box (5.6 x 2.4 x 2.2 m) standing on the spot, the office aside.
	protected static bool BoxClear(notnull BaseWorld world, vector at, float yaw, IEntity office)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
		mat[3] = at;
		TraceOBB box = new TraceOBB();
		box.Start = at + Vector(0, 0.35, 0);
		box.Mat[0] = mat[0];
		box.Mat[1] = mat[1];
		box.Mat[2] = mat[2];
		box.Mins = Vector(-1.2, 0, -2.8);
		box.Maxs = Vector(1.2, 2.2, 2.8);
		box.Flags = TraceFlags.ENTS;
		box.LayerMask = EPhysicsLayerPresets.Projectile;
		array<IEntity> exclude = {};
		if (office)
			exclude.Insert(office);

		box.ExcludeArray = exclude;
		return world.TracePosition(box, null) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a dead, downed or cuffed worker takes no run step (logged, nothing changes).
	protected static bool CanWork(IEntity user, string uid, string step)
	{
		if (LF_Utils.IsAliveState(user) && !LF_Downed.IsDowned(user) && !LF_Restraint.IsRestrained(user))
			return true;

		LF_Debug.Warn(FEATURE, string.Format("postal run %1 refused for %2: dead, downed or cuffed", step, uid));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WatchOn()
	{
		if (s_bWatchOn || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bWatchOn = true;
		GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WatchOff()
	{
		if (s_bWatchOn && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Watch);

		s_bWatchOn = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: the driver went away from the van `seconds` ago.
	static void AwayForTest(string uid, float seconds)
	{
		LF_PostalRunState run = GetRun(uid);
		if (run)
		{
			run.m_bAway = true;
			run.m_fAwaySince = LF_JobGuards.Now() - seconds;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: forget the cooldown of `uid`; and make the spot search find nothing (or something again).
	static void ClearCooldownForTest(string uid)
	{
		if (s_mEndedAt)
			s_mEndedAt.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		WatchOff();
		s_mRuns = null;
		s_mEndedAt = null;
		s_bTestNoSpot = false;
		LF_PostalRunConfig.Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PostalRun's statics (LF_StaticReset finds it by reflection).
class LF_PostalRun_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PostalRun";
	}

	override void Apply()
	{
		LF_PostalRun.ResetStatics();
	}
}
