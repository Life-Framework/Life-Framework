//------------------------------------------------------------------------------------------------
//! A parked-car marker (#3365): drop the prefab where a car should stand. At world start it puts out one
//! locked civilian car from Configs/Vehicles/LF_ParkedCars.conf (the pack's overlay chooses the period
//! cars). When that car has been gone from the marker for `m_iRespawnMinutes` and no player is within
//! `m_fPlayerClearMetres`, the marker puts out a fresh one. The old car is nobody's problem any more: it
//! is just a car in the world, so the police, the tow and the chop shop deal with it as with any other.
//!
//! Locked means the normal lock (LF_VehicleLockComponent) with no key anywhere, so opening it is a
//! lockpick or a hotwire, which the existing crime loop already handles. Nothing is saved: a world reset
//! rebuilds every marker's car. Server only. Cap: one car per marker, kept as an EntityID.
//------------------------------------------------------------------------------------------------
enum LF_EParkedCarCheck
{
	//! The car is at its marker: the clock stops.
	HOME,
	//! The car just went missing: the clock starts now.
	STARTED,
	//! Gone, but not for long enough yet.
	WAITING,
	//! Gone long enough, but a player is close: nobody sees a car appear in front of them.
	BLOCKED,
	//! Put a fresh car out.
	RESPAWN
}

//------------------------------------------------------------------------------------------------
//! The pure rule. No world access.
class LF_ParkedCarRules
{
	//------------------------------------------------------------------------------------------------
	//! \param away True when the marker's car is deleted or farther from the marker than its home radius.
	//! \param goneSince Unix second the car was first seen gone; 0 when it was home at the last sweep.
	static LF_EParkedCarCheck Check(bool away, int goneSince, int now, int waitSeconds, bool playerNear)
	{
		if (!away)
			return LF_EParkedCarCheck.HOME;

		if (goneSince <= 0)
			return LF_EParkedCarCheck.STARTED;

		if (now - goneSince < waitSeconds)
			return LF_EParkedCarCheck.WAITING;

		if (playerNear)
			return LF_EParkedCarCheck.BLOCKED;

		return LF_EParkedCarCheck.RESPAWN;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Vehicles", description: "A parked locked car that comes back after it is taken.")]
class LF_ParkedCarMarkerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ParkedCarMarkerComponent : ScriptComponent
{
	[Attribute("30", UIWidgets.Auto, desc: "Minutes the car must be gone from this marker before a fresh one is put out.", params: "1 1440")]
	protected int m_iRespawnMinutes;

	[Attribute("100", UIWidgets.Auto, desc: "No fresh car is put out while a player is this close to the marker, in metres.", params: "0 1000")]
	protected float m_fPlayerClearMetres;

	[Attribute("8", UIWidgets.Auto, desc: "The car counts as gone once it is farther than this from the marker, in metres.", params: "1 100")]
	protected float m_fHomeMetres;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "A police-car marker (#3875): put out always this car instead of one from the parked car list, and keep it a service vehicle that no civilian can pick, wire, key, chop or store. Empty: the normal list.", params: "et")]
	protected ResourceName m_sServiceCar;

	//! The seat traces start this far above the car's bottom (a car spawned with its wheels in the kerb still finds the kerb top)
	//! and reach this far under it (a marker up to 2.5 m over the street still lands the car on it).
	static const float SETTLE_LIFT = 1.2;
	static const float SETTLE_REACH = 2.5; // lf-fixed: a geometry bound of the seat trace, not a tunable

	protected EntityID m_CarId = EntityID.INVALID;
	protected int m_iGoneSince;
	protected int m_iSpawned;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame() || !GetGame().InPlayMode() || !Replication.IsServer())
			return;

		LF_ParkedCars.Register(owner.GetID());
		LF_ParkedCars.Start();
		GetGame().GetCallqueue().CallLater(FirstSpawn, 1000);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(FirstSpawn);

		LF_ParkedCars.Unregister(owner.GetID());
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void FirstSpawn()
	{
		if (!GetCar())
			SpawnCar("world start");
	}

	//------------------------------------------------------------------------------------------------
	//! \return The marker's current car, or null when it was never put out or has been deleted.
	IEntity GetCar()
	{
		if (m_CarId == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_CarId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many cars this marker has put out so far.
	int SpawnCount()
	{
		return m_iSpawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a car from the config out at the marker, locked, and makes it this marker's car.
	//! \return The car, or null (logged) when the config is empty, the prefab does not resolve or the spawn failed.
	IEntity SpawnCar(string why)
	{
		IEntity owner = GetOwner();
		if (!owner || !Replication.IsServer() || !LF_Systems.IsEnabled("Vehicles"))
			return null;

		LF_ParkedCarConfig config = LF_ParkedCarConfig.Get();
		if (!config)
			return null;

		vector transform[4];
		owner.GetWorldTransform(transform);
		vector angles = Math3D.MatrixToAngles(transform);
		vector origin = owner.GetOrigin();
		origin[1] = origin[1] + 0.3;

		// A car that does not spawn (the Z7211 on a server without the Rally content, #3676) is remembered for the
		// session and the next car in the list is tried, so the marker still puts something out.
		IEntity car;
		ResourceName prefab;
		while (!car)
		{
			if (!m_sServiceCar.IsEmpty())
			{
				prefab = m_sServiceCar;
				if (LF_ParkedCars.FailedCars().Find(LF_ParkedCarList.CarId(prefab)) >= 0)
					return null;
			}
			else
			{
				prefab = config.PickPrefabAvoiding(LF_ParkedCars.FailedCars());
			}

			if (prefab.IsEmpty())
			{
				LF_ParkedCars.ReportNoCar(config.m_aCars && !config.m_aCars.IsEmpty());
				return null;
			}

			car = LF_Utils.SpawnEntityPrefab(prefab, origin, angles);
			if (!car)
				LF_ParkedCars.NoteFailed(prefab);
		}

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		if (vehicleLock)
			{
			vehicleLock.SetLocked(true);
			vehicleLock.SetServiceVehicle(!m_sServiceCar.IsEmpty());
		}
		else
			LF_Debug.Warn(LF_ParkedCarConfig.FEATURE, string.Format("%1 has no LF_VehicleLockComponent: the parked car stands unlocked", prefab));

		// The marker's height came from the place's survey, not from the street under it (placement-system.md): rest the car's
		// wheels on the surface under its footprint (pavement, kerb, forecourt), traced from SETTLE_LIFT above its bottom.
		LF_PlaceSnapResult seat = LF_PlaceSnap.Settle(car, SETTLE_LIFT, SETTLE_REACH);
		if (seat)
			LF_Debug.Info(LF_ParkedCarConfig.FEATURE, string.Format("parked car seated on %1 (gap %2 m before, %3 m after)", seat.m_sOn, seat.m_fGapBefore, seat.m_fGap));

		m_CarId = car.GetID();
		m_iGoneSince = 0;
		m_iSpawned++;
		LF_Debug.Info(LF_ParkedCarConfig.FEATURE, string.Format("parked car out (%1): %2 at %3", why, prefab, car.GetOrigin()));
		return car;
	}

	//------------------------------------------------------------------------------------------------
	//! One sweep for this marker.
	//! \param now Unix seconds.
	//! \return True when a fresh car was put out.
	bool Sweep(int now)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		IEntity car = GetCar();
		bool away = true;
		if (car)
			away = vector.Distance(car.GetOrigin(), owner.GetOrigin()) > m_fHomeMetres;

		bool playerNear = false;
		if (away && m_iGoneSince > 0)
			playerNear = LF_ParkedCars.PlayerNear(owner.GetOrigin(), m_fPlayerClearMetres);

		LF_EParkedCarCheck check = LF_ParkedCarRules.Check(away, m_iGoneSince, now, m_iRespawnMinutes * 60, playerNear);
		if (check == LF_EParkedCarCheck.HOME)
		{
			m_iGoneSince = 0;
			return false;
		}

		if (check == LF_EParkedCarCheck.STARTED)
		{
			m_iGoneSince = now;
			LF_Debug.Info(LF_ParkedCarConfig.FEATURE, string.Format("parked car gone from its marker at %1; fresh one in %2 min", owner.GetOrigin(), m_iRespawnMinutes));
			return false;
		}

		if (check != LF_EParkedCarCheck.RESPAWN)
			return false;

		return SpawnCar("respawn") != null;
	}
}

//------------------------------------------------------------------------------------------------
//! Which markers exist and the one timer that sweeps them. Server session state, never saved.
class LF_ParkedCars
{
	static const int SWEEP_MS = 30000; // lf-fixed: the sweep cadence; the wait itself is the marker's attribute

	protected static ref array<EntityID> s_aMarkers;
	//! Test seam: positions that count as a player standing there.
	protected static ref array<vector> s_aStandIns;
	protected static bool s_bScheduled;
	//! Car ids (LF_ParkedCarList.CarId) whose prefab did not spawn this session; never tried again.
	protected static ref array<string> s_aFailedCars;
	protected static bool s_bNoCarReported;

	//------------------------------------------------------------------------------------------------
	//! \return The cars that failed to spawn this session (never null).
	static array<string> FailedCars()
	{
		if (!s_aFailedCars)
			s_aFailedCars = new array<string>();

		return s_aFailedCars;
	}

	//------------------------------------------------------------------------------------------------
	//! Remembers a prefab that did not spawn; logs once per prefab. The spawn itself already logged why.
	static void NoteFailed(ResourceName prefab)
	{
		string id = LF_ParkedCarList.CarId(prefab);
		if (FailedCars().Find(id) >= 0)
			return;

		FailedCars().Insert(id);
		LF_Debug.Error(LF_ParkedCarConfig.FEATURE, string.Format("parked car %1 did not spawn: skipped for this session, the marker tries the next car in the list", prefab));
	}

	//------------------------------------------------------------------------------------------------
	//! A marker found no car to try. Logged once per session so a respawn sweep does not repeat it.
	//! \param listHasCars False when the config list itself is empty.
	static void ReportNoCar(bool listHasCars)
	{
		if (s_bNoCarReported)
			return;

		s_bNoCarReported = true;
		if (listHasCars)
			LF_Debug.Error(LF_ParkedCarConfig.FEATURE, "marker has no car to put out: every car in the parked car list failed to spawn");
		else
			LF_Debug.Error(LF_ParkedCarConfig.FEATURE, "marker has no car to put out: the parked car list is empty");
	}

	//------------------------------------------------------------------------------------------------
	protected static array<EntityID> Markers()
	{
		if (!s_aMarkers)
			s_aMarkers = new array<EntityID>();

		return s_aMarkers;
	}

	//------------------------------------------------------------------------------------------------
	static void Register(EntityID marker)
	{
		if (Markers().Find(marker) < 0)
			Markers().Insert(marker);
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(EntityID marker)
	{
		int index = Markers().Find(marker);
		if (index >= 0)
			Markers().Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sweep every SWEEP_MS. Idempotent.
	static void Start()
	{
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, SWEEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Sweep(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over every marker.
	//! \param now Unix seconds.
	//! \return How many fresh cars were put out.
	static int Sweep(int now)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		int respawned = 0;
		array<EntityID> markers = {};
		markers.Copy(Markers());
		foreach (EntityID id : markers)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (!entity)
			{
				Unregister(id);
				continue;
			}

			LF_ParkedCarMarkerComponent marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(entity);
			if (marker && marker.Sweep(now))
				respawned++;
		}

		return respawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: a position that counts as a player until ResetStatics.
	static void AddStandIn(vector position)
	{
		if (!s_aStandIns)
			s_aStandIns = new array<vector>();

		s_aStandIns.Insert(position);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a player's body, or a test stand-in, is within `radius` of `at`.
	static bool PlayerNear(vector at, float radius)
	{
		if (s_aStandIns)
		{
			foreach (vector standIn : s_aStandIns)
			{
				if (vector.Distance(standIn, at) <= radius)
					return true;
			}
		}

		return LF_CashVanRoute.PlayersNear(at, radius) > 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
		s_aMarkers = null;
		s_aStandIns = null;
		s_aFailedCars = null;
		s_bNoCarReported = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ParkedCars' statics (LF_StaticReset finds it by reflection).
class LF_ParkedCars_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ParkedCars";
	}

	override void Apply()
	{
		LF_ParkedCars.ResetStatics();
	}
}
