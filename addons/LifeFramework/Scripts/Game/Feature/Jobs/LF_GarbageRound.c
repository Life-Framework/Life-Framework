//------------------------------------------------------------------------------------------------
//! Garbage round numbers (#3325): Configs/Jobs/LF_GarbageRound.conf with each pack overlay of the same
//! class layered over it. A round is `m_iStops` bins emptied in order, then tipped at the dump: pay is
//! stops x rate + a round bonus (LF_RouteRules, #3324). Steady, low, predictable pay: no buy-in, no risk.
[BaseContainerProps(configRoot: true)]
class LF_GarbageRoundConfig
{
	static const ResourceName CONFIG_PATH = "{5D2C8E1A7B94F306}Configs/Jobs/LF_GarbageRound.conf";

	static const int DEFAULT_STOPS = 5;	// lf-fixed: fallback only, mirrors LF_GarbageRound.conf
	static const int DEFAULT_RATE = 15;	// lf-fixed: fallback only, mirrors LF_GarbageRound.conf
	static const int DEFAULT_BONUS = 25;	// lf-fixed: fallback only, mirrors LF_GarbageRound.conf
	static const float DEFAULT_TIP_RADIUS = 30;	// lf-fixed: fallback only, mirrors LF_GarbageRound.conf
	static const int DEFAULT_MIN_SECONDS = 900;	// lf-fixed: fallback only, mirrors LF_GarbageRound.conf

	[Attribute("5", UIWidgets.Auto, "Garbage round: how many bins a round has (the nearest to the depot, in order)", params: "1 40")]
	int m_iStops;

	[Attribute("15", UIWidgets.Auto, "Garbage round: cash paid for each bin emptied, at the tip", params: "0 100000")]
	int m_iRatePerStop;

	[Attribute("25", UIWidgets.Auto, "Garbage round: cash added when every bin was emptied and the load is tipped", params: "0 100000")]
	int m_iRoundBonus;

	[Attribute("30", UIWidgets.Auto, "Garbage round: how close (metres) the truck must stand to the dump to tip", params: "1 500")]
	float m_fTipRadius;

	[Attribute("900", UIWidgets.Auto, "Garbage round: the least seconds a round takes, start to tip. Keeps a round of bins by the depot from paying faster than the job's hourly band", params: "0 36000")]
	int m_iMinRoundSeconds;

	[Attribute("{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et", UIWidgets.ResourcePickerThumbnail, "Garbage round: the company truck handed out at the depot", "et")]
	ResourceName m_Truck;

	protected static ref LF_GarbageRoundConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_GarbageRoundConfig CreateDefault()
	{
		LF_GarbageRoundConfig config = new LF_GarbageRoundConfig();
		config.m_iStops = DEFAULT_STOPS;
		config.m_iRatePerStop = DEFAULT_RATE;
		config.m_iRoundBonus = DEFAULT_BONUS;
		config.m_fTipRadius = DEFAULT_TIP_RADIUS;
		config.m_iMinRoundSeconds = DEFAULT_MIN_SECONDS;
		config.m_Truck = "{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et";
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config; the shipped numbers (logged) when the file does not resolve.
	static LF_GarbageRoundConfig Get()
	{
		if (s_bLoadAttempted && s_Config)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_GarbageRoundConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error("Jobs", string.Format("garbage round config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = CreateDefault();
		}
		else
		{
			s_Config.ApplyLayers();
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the files again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_GarbageRoundConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over this instance, in load order (#3147).
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_GarbageRoundConfig, "garbageRound", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("garbageRound: pack overlay %1 changed %2 fields", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: only the fields it sets directly. \return Fields changed.
	int ApplyContainer(BaseContainer container)
	{
		if (!container)
			return 0;

		int changed;
		if (container.IsVariableSetDirectly("m_iStops") && container.Get("m_iStops", m_iStops))
			changed++;
		if (container.IsVariableSetDirectly("m_iRatePerStop") && container.Get("m_iRatePerStop", m_iRatePerStop))
			changed++;
		if (container.IsVariableSetDirectly("m_iRoundBonus") && container.Get("m_iRoundBonus", m_iRoundBonus))
			changed++;
		if (container.IsVariableSetDirectly("m_fTipRadius") && container.Get("m_fTipRadius", m_fTipRadius))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinRoundSeconds") && container.Get("m_iMinRoundSeconds", m_iMinRoundSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_Truck") && container.Get("m_Truck", m_Truck))
			changed++;
		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! What a garbage-round step did; the toast key of each refusal is `LF_GarbageRound.RefusalKey`.
enum LF_EGarbageResult
{
	OK = 0,
	NO_ROUND = 1,		// the player has no round open
	NOT_ON_ROUTE = 2,	// the bin is not one of this round's bins
	OUT_OF_ORDER = 3,	// a bin on the route, but not the next one
	NOT_ENOUGH = 4,		// tip asked before every bin is emptied
	TRUCK_FAR = 5,		// the truck is not at the dump
	NO_BINS = 6,		// the world has no bin to build a route from
	NO_TRUCK = 7,		// the company truck could not be handed out
	BAD_CONFIG = 8,
	TOO_FAST = 9,		// tip asked before the round's minimum time has passed
	UNFIT = 10		// the worker is dead, downed or cuffed (no toast: they cannot act)
}

//------------------------------------------------------------------------------------------------
//! One player's open round: the route rules, the bins in order, and the truck. Server only.
class LF_GarbageRun
{
	ref LF_RouteRules m_Rules;
	ref array<EntityID> m_aBins = {};
	EntityID m_Truck = EntityID.INVALID;
	int m_iBags;
	float m_fStartedAt;
}

//------------------------------------------------------------------------------------------------
//! The garbage round (#3325): sign up at the depot and take the company truck, "Empty bin" at each bin
//! of the route in order (the bin's contents are deleted server-side, a bag goes on the truck), "Tip the
//! load" at the dump ends the round, takes the truck back and pays once through the job cash path. The route is the bins
//! nearest the depot, found by component through LF_BinComponent, never by name. Server state only: a
//! round is not saved, a restart ends it (the truck is gone too).
class LF_GarbageRound
{
	static const string FEATURE = "Jobs";

	//! LF_Places kinds of the depot and the dump.
	static const string PLACE_DEPOT = "garbage_depot";
	static const string PLACE_DUMP = "garbage_dump";

	protected static ref map<string, ref LF_GarbageRun> s_mRuns;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_GarbageRun> Runs()
	{
		if (!s_mRuns)
			s_mRuns = new map<string, ref LF_GarbageRun>();

		return s_mRuns;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasRound(string uid)
	{
		return !uid.IsEmpty() && Runs().Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player's open round, or null.
	static LF_GarbageRun GetRound(string uid)
	{
		LF_GarbageRun run;
		if (uid.IsEmpty() || !Runs().Find(uid, run))
			return null;

		return run;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The toast key that says why a step was refused; "" for OK.
	static string RefusalKey(LF_EGarbageResult result)
	{
		if (result == LF_EGarbageResult.NO_ROUND)
			return "#LF-Garbage_NoRound";
		if (result == LF_EGarbageResult.NOT_ON_ROUTE)
			return "#LF-Garbage_NotOnRoute";
		if (result == LF_EGarbageResult.OUT_OF_ORDER)
			return "#LF-Garbage_OutOfOrder";
		if (result == LF_EGarbageResult.NOT_ENOUGH)
			return "#LF-Garbage_NotEnough";
		if (result == LF_EGarbageResult.TRUCK_FAR)
			return "#LF-Garbage_TruckFar";
		if (result == LF_EGarbageResult.NO_BINS)
			return "#LF-Garbage_NoBins";
		if (result == LF_EGarbageResult.NO_TRUCK)
			return "#LF-Garbage_NoTruck";
		if (result == LF_EGarbageResult.BAD_CONFIG)
			return "#LF-Garbage_NoBins";
		if (result == LF_EGarbageResult.TOO_FAST)
			return "#LF-Garbage_TooFast";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens a round for the player at `depot`: the bins nearest the depot become the route
	//! (nearest first) and the company truck is handed out beside it. A round already open is closed
	//! first (no pay, its truck removed), so a dead round never locks the player out.
	static LF_EGarbageResult Start(IEntity user, string uid, IEntity depot)
	{
		if (!Replication.IsServer() || !user || !depot)
			return LF_EGarbageResult.NO_ROUND;

		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "garbage round start refused: player has no account id");
			return LF_EGarbageResult.NO_ROUND;
		}

		LF_GarbageRoundConfig config = LF_GarbageRoundConfig.Get();
		array<IEntity> bins = {};
		LF_BinComponent.FindAllBins(bins);
		if (bins.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "garbage round start refused: the world has no bin");
			return LF_EGarbageResult.NO_BINS;
		}

		LF_RouteRules rules = LF_RouteRules.Create(Math.Min(config.m_iStops, bins.Count()), config.m_iRatePerStop, config.m_iRoundBonus);
		if (!rules)
			return LF_EGarbageResult.BAD_CONFIG;

		if (!CanWork(user, uid, "start"))
			return LF_EGarbageResult.UNFIT;

		Close(uid);

		LF_GarbageRun run = new LF_GarbageRun();
		run.m_Rules = rules;
		OrderByDistance(bins, depot.GetOrigin());
		for (int i = 0; i < rules.StopCount(); i++)
			run.m_aBins.Insert(bins[i].GetID());

		vector at = depot.GetOrigin() + depot.GetTransformAxis(2) * 6 + depot.GetTransformAxis(0) * 3;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity truck = LF_Utils.SpawnEntityPrefab(config.m_Truck, at, depot.GetAngles());
		if (!truck)
		{
			LF_Debug.Error(FEATURE, string.Format("garbage round: the company truck did not spawn: %1", config.m_Truck));
			return LF_EGarbageResult.NO_TRUCK;
		}

		run.m_Truck = truck.GetID();
		run.m_fStartedAt = LF_JobGuards.Now();
		Runs().Set(uid, run);
		LF_Debug.Info(FEATURE, string.Format("garbage round started by %1: %2 bins", uid, run.m_aBins.Count()));
		return LF_EGarbageResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player empties `binEntity`. It must be the round's next bin; the bin's contents are
	//! deleted and a bag goes on the truck. A refusal changes nothing.
	static LF_EGarbageResult EmptyBin(IEntity user, string uid, IEntity binEntity)
	{
		if (!Replication.IsServer() || !user || !binEntity)
			return LF_EGarbageResult.NO_ROUND;

		LF_GarbageRun run = GetRound(uid);
		if (!run)
			return LF_EGarbageResult.NO_ROUND;

		if (!CanWork(user, uid, "bin"))
			return LF_EGarbageResult.UNFIT;

		int index = run.m_aBins.Find(binEntity.GetID());
		if (index < 0)
			return LF_EGarbageResult.NOT_ON_ROUTE;

		if (!run.m_Rules.CompleteStop(index))
			return LF_EGarbageResult.OUT_OF_ORDER;

		LF_BinComponent bin = LF_Component<LF_BinComponent>.Find(binEntity);
		if (bin)
			bin.Empty();

		run.m_iBags++;
		LF_Debug.Info(FEATURE, string.Format("garbage round: bin %1 of %2 emptied", run.m_Rules.StopsDone(), run.m_Rules.StopCount()));
		return LF_EGarbageResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player tips the load at `dump`: every bin must be emptied and the truck must stand
	//! within the tip radius. Pays once and closes the round.
	//! \param[out] paid The cash paid, 0 on a refusal.
	static LF_EGarbageResult Tip(IEntity user, string uid, IEntity dump, out int paid)
	{
		paid = 0;
		if (!Replication.IsServer() || !user || !dump)
			return LF_EGarbageResult.NO_ROUND;

		LF_GarbageRun run = GetRound(uid);
		if (!run)
			return LF_EGarbageResult.NO_ROUND;

		if (!CanWork(user, uid, "tip"))
			return LF_EGarbageResult.UNFIT;

		if (!run.m_Rules.IsComplete())
			return LF_EGarbageResult.NOT_ENOUGH;

		if (LF_JobGuards.Now() - run.m_fStartedAt < LF_GarbageRoundConfig.Get().m_iMinRoundSeconds)
			return LF_EGarbageResult.TOO_FAST;

		IEntity truck = GetGame().GetWorld().FindEntityByID(run.m_Truck);
		if (!truck || vector.DistanceXZ(truck.GetOrigin(), dump.GetOrigin()) > LF_GarbageRoundConfig.Get().m_fTipRadius)
			return LF_EGarbageResult.TRUCK_FAR;

		paid = run.m_Rules.PayAtEnd();
		Runs().Remove(uid);
		ReturnTruck(truck);
		LF_CivilianJob.HandCash(user, paid, "garbage-round");
		LF_Debug.Info(FEATURE, string.Format("garbage round tipped by %1: paid %2", uid, paid));
		return LF_EGarbageResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: closes the player's round without pay and removes its truck.
	static void Close(string uid)
	{
		LF_GarbageRun run = GetRound(uid);
		if (!run)
			return;

		Runs().Remove(uid);
		ReturnTruck(GetGame().GetWorld().FindEntityByID(run.m_Truck));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the company truck back (deleted). An occupied truck is left where it stands, logged:
	//! deleting a vehicle with its children would delete the people in its seats.
	protected static void ReturnTruck(IEntity truck)
	{
		if (!truck)
			return;

		if (LF_StolenVehicles.HasOccupant(truck))
		{
			LF_Debug.Warn(FEATURE, string.Format("garbage round: company truck %1 has someone aboard, left in place", truck.GetID()));
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(truck);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a dead, downed or cuffed worker takes no round step (logged, nothing changes).
	protected static bool CanWork(IEntity user, string uid, string step)
	{
		if (LF_Utils.IsAliveState(user) && !LF_Downed.IsDowned(user) && !LF_Restraint.IsRestrained(user))
			return true;

		LF_Debug.Warn(FEATURE, string.Format("garbage round %1 refused for %2: dead, downed or cuffed", step, uid));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts `bins` nearest first to `origin` on the ground plane (insertion sort, the list is short).
	protected static void OrderByDistance(notnull array<IEntity> bins, vector origin)
	{
		for (int i = 1; i < bins.Count(); i++)
		{
			IEntity entry = bins[i];
			float dist = vector.DistanceXZ(entry.GetOrigin(), origin);
			int j = i - 1;
			while (j >= 0 && vector.DistanceXZ(bins[j].GetOrigin(), origin) > dist)
			{
				bins[j + 1] = bins[j];
				j--;
			}

			bins[j + 1] = entry;
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRuns = null;
		LF_GarbageRoundConfig.Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_GarbageRound's statics (LF_StaticReset finds it by reflection).
class LF_GarbageRound_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_GarbageRound";
	}

	override void Apply()
	{
		LF_GarbageRound.ResetStatics();
	}
}
