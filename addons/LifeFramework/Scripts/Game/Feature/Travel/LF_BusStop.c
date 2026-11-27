// LF_BusStop.c - the bus between stops (server). A player at a stop pays the cash fare for another
// stop (SCR_PlayerController.LF_AskBusTrip), waits at the stop for the bus, and is moved to the far
// stop's arrival spot. Walking away, a fight, damage or a downed, cuffed or seated body cancels the
// wait and refunds the fare. Nothing is saved: the money change is the only lasting effect.
// Stops are found by component (LF_BusStopComponent registers kind BusStop in LF_Places), never by
// map coordinate.

//------------------------------------------------------------------------------------------------
enum LF_EBusRefusal
{
	NONE,
	NO_CHARACTER,
	NO_STOP,
	SAME_STOP,
	NOT_AT_STOP,
	DEAD,
	DOWNED,
	CUFFED,
	DRIVING,
	COMBAT,
	PURSUIT,
	NO_CASH,
	BUSY,
	TOO_SOON
}

//------------------------------------------------------------------------------------------------
//! What a waiting trip does on this tick.
enum LF_EBusStep
{
	WAIT,
	DEPART,
	CANCEL_MOVED,
	CANCEL_HURT,
	CANCEL_STATE,
	CANCEL_NO_ARRIVAL,
	CANCEL_ASKED
}

//------------------------------------------------------------------------------------------------
//! Bus numbers (fares, the wait, the cancel rules): Configs/Travel/LF_BusStops.conf with each pack
//! overlay of this class layered over it. No owner section: the client prices the timetable from the
//! same files the server charges by, so the two always agree.
[BaseContainerProps(configRoot: true)]
class LF_BusStopConfig
{
	static const ResourceName CONFIG_PATH = "{5C0E3A7B91D24F68}Configs/Travel/LF_BusStops.conf";

	static const int DEFAULT_BASE_FARE = 10;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_FARE_PER_KM = 5;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const int DEFAULT_MAX_FARE = 40;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_WAIT_SECONDS = 20;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_WAIT_PER_KM = 2;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_MAX_WAIT_SECONDS = 30;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_STOP_REACH = 5;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_CANCEL_RADIUS = 4;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_ASK_COOLDOWN = 3;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_TRIP_COOLDOWN = 60;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_PURSUIT_RADIUS = 150;	// lf-fixed: fallback only, mirrors LF_BusStops.conf
	static const float DEFAULT_FADE_SECONDS = 1;	// lf-fixed: fallback only, mirrors LF_BusStops.conf

	[Attribute("10", UIWidgets.Auto, "Bus fare: what any trip costs before distance", params: "0 100000")]
	int m_iBaseFare;

	[Attribute("5", UIWidgets.Auto, "Bus fare: cash added per kilometre of straight-line distance between the stops", params: "0 100000")]
	float m_fFarePerKm;

	[Attribute("40", UIWidgets.Auto, "Bus fare: the most a trip costs, however far. Not below the base fare", params: "0 100000")]
	int m_iMaxFare;

	[Attribute("20", UIWidgets.Auto, "Seconds a passenger waits at the stop before a short trip leaves", params: "1 600")]
	float m_fWaitSeconds;

	[Attribute("2", UIWidgets.Auto, "Seconds of wait added per kilometre between the stops", params: "0 600")]
	float m_fWaitPerKm;

	[Attribute("30", UIWidgets.Auto, "The longest wait, however far. Not below the base wait", params: "1 600")]
	float m_fMaxWaitSeconds;

	[Attribute("5", UIWidgets.Auto, "How close to the stop sign (metres) a player must stand to buy a ticket", params: "1 50")]
	float m_fStopReach;

	[Attribute("4", UIWidgets.Auto, "Walking this many metres from where the ticket was bought cancels the wait and refunds it", params: "1 50")]
	float m_fCancelRadius;

	[Attribute("3", UIWidgets.Auto, "Seconds between two ticket requests from one player (spam guard)", params: "0 600")]
	float m_fAskCooldown;

	[Attribute("60", UIWidgets.Auto, "Seconds after an arrival before the same player can ride again", params: "0 3600")]
	float m_fTripCooldown;

	[Attribute("150", UIWidgets.Auto, "A wanted player with an on-duty officer this close (metres) is refused: no bus out of a chase", params: "0 5000")]
	float m_fPursuitRadius;

	[Attribute("1", UIWidgets.Auto, "Seconds of fade to black before the move and back after it", params: "0 10")]
	float m_fFadeSeconds;

	[Attribute("0", UIWidgets.Auto, "Bus stop action list: a stop closer than this (metres) is not offered as a destination, so the top row is never a ride you could walk. A stop with nothing farther keeps its list", params: "0 100000")]
	float m_fMinRideMetres;

	protected static ref LF_BusStopConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_BusStopConfig CreateDefault()
	{
		LF_BusStopConfig config = new LF_BusStopConfig();
		config.m_iBaseFare = DEFAULT_BASE_FARE;
		config.m_fFarePerKm = DEFAULT_FARE_PER_KM;
		config.m_iMaxFare = DEFAULT_MAX_FARE;
		config.m_fWaitSeconds = DEFAULT_WAIT_SECONDS;
		config.m_fWaitPerKm = DEFAULT_WAIT_PER_KM;
		config.m_fMaxWaitSeconds = DEFAULT_MAX_WAIT_SECONDS;
		config.m_fStopReach = DEFAULT_STOP_REACH;
		config.m_fCancelRadius = DEFAULT_CANCEL_RADIUS;
		config.m_fAskCooldown = DEFAULT_ASK_COOLDOWN;
		config.m_fTripCooldown = DEFAULT_TRIP_COOLDOWN;
		config.m_fPursuitRadius = DEFAULT_PURSUIT_RADIUS;
		config.m_fFadeSeconds = DEFAULT_FADE_SECONDS;
		config.m_fMinRideMetres = 0;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config; the shipped numbers (logged once) when the file does not resolve.
	static LF_BusStopConfig Get()
	{
		if (s_bLoadAttempted && s_Config)
			return s_Config;

		if (s_bLoadAttempted)
			return CreateDefault();

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_BusStopConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, string.Format("bus config does not resolve, using the shipped numbers: %1", CONFIG_PATH));
			s_Config = CreateDefault();
			return s_Config;
		}

		s_Config.ApplyLayers();
		if (!s_Config.IsUsable())
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus config is unusable (a cap below its base), using the shipped numbers");
			s_Config = CreateDefault();
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_BusStopConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Each pack overlay of this class in load order, only the fields it sets directly.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_BusStopConfig, "busStops", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus config: pack overlay %1 changed %2 fields", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fields changed.
	int ApplyContainer(BaseContainer container)
	{
		if (!container)
			return 0;

		int changed;
		if (container.IsVariableSetDirectly("m_iBaseFare") && container.Get("m_iBaseFare", m_iBaseFare))
			changed++;
		if (container.IsVariableSetDirectly("m_iMaxFare") && container.Get("m_iMaxFare", m_iMaxFare))
			changed++;
		if (ReadFloat(container, "m_fFarePerKm", m_fFarePerKm))
			changed++;
		if (ReadFloat(container, "m_fWaitSeconds", m_fWaitSeconds))
			changed++;
		if (ReadFloat(container, "m_fWaitPerKm", m_fWaitPerKm))
			changed++;
		if (ReadFloat(container, "m_fMaxWaitSeconds", m_fMaxWaitSeconds))
			changed++;
		if (ReadFloat(container, "m_fStopReach", m_fStopReach))
			changed++;
		if (ReadFloat(container, "m_fCancelRadius", m_fCancelRadius))
			changed++;
		if (ReadFloat(container, "m_fAskCooldown", m_fAskCooldown))
			changed++;
		if (ReadFloat(container, "m_fTripCooldown", m_fTripCooldown))
			changed++;
		if (ReadFloat(container, "m_fPursuitRadius", m_fPursuitRadius))
			changed++;
		if (ReadFloat(container, "m_fFadeSeconds", m_fFadeSeconds))
			changed++;
		if (ReadFloat(container, "m_fMinRideMetres", m_fMinRideMetres))
			changed++;

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadFloat(BaseContainer container, string name, out float value)
	{
		if (!container.IsVariableSetDirectly(name))
			return false;

		return container.Get(name, value);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the numbers can run a bus: nothing negative, caps not below their bases.
	bool IsUsable()
	{
		if (m_iBaseFare < 0 || m_fFarePerKm < 0 || m_iMaxFare < m_iBaseFare)
			return false;

		if (m_fWaitSeconds <= 0 || m_fWaitPerKm < 0 || m_fMaxWaitSeconds < m_fWaitSeconds)
			return false;

		return m_fStopReach > 0 && m_fCancelRadius > 0;
	}
}

//------------------------------------------------------------------------------------------------
//! One paid ticket waiting for its bus (server).
class LF_BusTrip
{
	int m_iSlot;
	int m_iPlayerId;
	string m_sKey;
	EntityID m_BodyId = EntityID.INVALID;
	string m_sFrom;
	string m_sTo;
	int m_iFare;
	float m_fBoughtAt;
	float m_fDepartAt;
	vector m_vStandAt;
	float m_fHealthAtStart;
	bool m_bDeparting;
}

//------------------------------------------------------------------------------------------------
class LF_BusStop
{
	static const string FEATURE = "Travel";
	//! The owner's switch for the bus (Configs/Systems/LF_SystemCatalog.conf).
	static const string SYSTEM = "Travel";
	//! LF_Places kind of a placed stop; the map's POI row has the same kind.
	static const string PLACE_KIND = "BusStop";
	static const string REASON = "bus fare";
	static const string REASON_REFUND = "bus fare refund";
	//! How often a waiting trip is checked.
	static const int TICK_MS = 500;	// lf-fixed: half a second is the cancel latency a player notices, not tuning
	//! Health lost below the ticket's start that counts as being hurt (rounding noise is ignored).
	static const float HURT_EPSILON = 0.5;	// lf-fixed: noise floor of the damage manager's health

	protected static ref map<int, ref LF_BusTrip> s_mTrips;
	//! Character key -> world seconds of the last ticket request.
	protected static ref map<string, float> s_mAskedAt;
	//! Character key -> world seconds of the last arrival.
	protected static ref map<string, float> s_mArrivedAt;
	protected static bool s_bTicking;
	protected static int s_iTrips;
	protected static int s_iBodySlots;

	//------------------------------------------------------------------------------------------------
	//! The fare between two stops `metres` apart, by straight line.
	static int Fare(LF_BusStopConfig config, float metres)
	{
		if (!config)
			config = LF_BusStopConfig.CreateDefault();

		if (metres < 0)
			metres = 0;

		int fare = config.m_iBaseFare + Math.Round(config.m_fFarePerKm * metres / 1000);
		return LF_Utils.ClampInt(fare, config.m_iBaseFare, config.m_iMaxFare);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds a passenger waits for a trip of `metres`.
	static float WaitSeconds(LF_BusStopConfig config, float metres)
	{
		if (!config)
			config = LF_BusStopConfig.CreateDefault();

		if (metres < 0)
			metres = 0;

		float wait = config.m_fWaitSeconds + config.m_fWaitPerKm * metres / 1000;
		if (wait > config.m_fMaxWaitSeconds)
			return config.m_fMaxWaitSeconds;

		return wait;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the refusal for a body in this state. officerNear is an on-duty officer within the
	//! pursuit radius (read only when the player is wanted).
	static LF_EBusRefusal RefuseFor(LF_CharacterSwitchState state, bool officerNear)
	{
		if (!state)
			return LF_EBusRefusal.NO_CHARACTER;

		if (!state.m_bAlive)
			return LF_EBusRefusal.DEAD;

		if (state.m_bDowned)
			return LF_EBusRefusal.DOWNED;

		if (state.m_bCuffed || state.m_bJailed)
			return LF_EBusRefusal.CUFFED;

		if (state.m_bInVehicle)
			return LF_EBusRefusal.DRIVING;

		if (state.m_bInCombat)
			return LF_EBusRefusal.COMBAT;

		if (state.m_iWanted > 0 && officerNear)
			return LF_EBusRefusal.PURSUIT;

		return LF_EBusRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a request at `now` is inside either cooldown. Negative times mean never.
	static bool IsTooSoon(LF_BusStopConfig config, float lastAskAt, float lastArriveAt, float now)
	{
		if (!config)
			config = LF_BusStopConfig.CreateDefault();

		if (lastAskAt >= 0 && now - lastAskAt < config.m_fAskCooldown)
			return true;

		return lastArriveAt >= 0 && now - lastArriveAt < config.m_fTripCooldown;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: what a waiting trip does now. at is where the body stands; health is its current health.
	static LF_EBusStep Step(notnull LF_BusTrip trip, LF_BusStopConfig config, float now, vector at, LF_CharacterSwitchState state, float lastHitAt, float health)
	{
		if (!config)
			config = LF_BusStopConfig.CreateDefault();

		if (!state || RefuseFor(state, false) != LF_EBusRefusal.NONE)
			return LF_EBusStep.CANCEL_STATE;

		if (vector.DistanceXZ(at, trip.m_vStandAt) > config.m_fCancelRadius)
			return LF_EBusStep.CANCEL_MOVED;

		if (lastHitAt >= trip.m_fBoughtAt || health < trip.m_fHealthAtStart - HURT_EPSILON)
			return LF_EBusStep.CANCEL_HURT;

		if (now >= trip.m_fDepartAt)
			return LF_EBusStep.DEPART;

		return LF_EBusStep.WAIT;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player's ticket request. playerId 0 is a body with no player (a test).
	static LF_EBusRefusal Board(int playerId, IEntity body, string key, string fromId, string toId)
	{
		LF_BusStopConfig config = LF_BusStopConfig.Get();
		float now = LF_CharacterSwitch.Now();
		if (!LF_Systems.IsEnabled(SYSTEM))
			return Refuse(LF_EBusRefusal.NO_STOP, key, fromId, toId);

		if (!body || key.IsEmpty())
			return Refuse(LF_EBusRefusal.NO_CHARACTER, key, fromId, toId);

		float askedAt = -1;
		float arrivedAt = -1;
		if (s_mAskedAt)
			s_mAskedAt.Find(key, askedAt);
		if (s_mArrivedAt)
			s_mArrivedAt.Find(key, arrivedAt);

		if (IsTooSoon(config, askedAt, arrivedAt, now))
			return Refuse(LF_EBusRefusal.TOO_SOON, key, fromId, toId);

		if (!s_mAskedAt)
			s_mAskedAt = new map<string, float>();

		s_mAskedAt.Set(key, now);
		if (TripOf(playerId, key))
			return Refuse(LF_EBusRefusal.BUSY, key, fromId, toId);

		IEntity from = LF_Places.Find(PLACE_KIND, fromId);
		IEntity to = LF_Places.Find(PLACE_KIND, toId);
		if (!from || !to)
			return Refuse(LF_EBusRefusal.NO_STOP, key, fromId, toId);

		if (fromId == toId)
			return Refuse(LF_EBusRefusal.SAME_STOP, key, fromId, toId);

		if (vector.Distance(body.GetOrigin(), from.GetOrigin()) > config.m_fStopReach)
			return Refuse(LF_EBusRefusal.NOT_AT_STOP, key, fromId, toId);

		LF_CharacterSwitchState state = ReadState(playerId, body, key);
		bool officerNear = state.m_iWanted > 0 && LF_WantedReports.IsOfficerNear(body, config.m_fPursuitRadius);
		LF_EBusRefusal refusal = RefuseFor(state, officerNear);
		if (refusal != LF_EBusRefusal.NONE)
			return Refuse(refusal, key, fromId, toId);

		float metres = vector.Distance(from.GetOrigin(), to.GetOrigin());
		int fare = Fare(config, metres);
		if (fare > 0 && LF_MoneyUtils.GetCash(body) < fare)
			return Refuse(LF_EBusRefusal.NO_CASH, key, fromId, toId);

		if (fare > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(body, fare, REASON);
			if (taken < fare)
			{
				if (taken > 0)
					LF_MoneyUtils.AddCash(body, taken, REASON_REFUND);

				return Refuse(LF_EBusRefusal.NO_CASH, key, fromId, toId);
			}
		}

		LF_BusTrip trip = new LF_BusTrip();
		trip.m_iPlayerId = playerId;
		trip.m_sKey = key;
		trip.m_BodyId = body.GetID();
		trip.m_sFrom = fromId;
		trip.m_sTo = toId;
		trip.m_iFare = fare;
		trip.m_fBoughtAt = now;
		trip.m_fDepartAt = now + WaitSeconds(config, metres);
		trip.m_vStandAt = body.GetOrigin();
		trip.m_fHealthAtStart = HealthOf(body);
		trip.m_iSlot = NewSlot(playerId);
		Trips().Set(trip.m_iSlot, trip);
		s_iTrips++;
		StartTicking();
		LF_Debug.Info(FEATURE, string.Format("bus ticket: %1 paid %2 from %3 to %4 (%5 m), leaves in %6 s", key, fare, fromId, toId, Math.Round(metres), Math.Round(trip.m_fDepartAt - now)));
		return LF_EBusRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: checks every waiting trip once. Public so a test can step time without the call queue.
	static void Tick()
	{
		if (!s_mTrips || s_mTrips.IsEmpty())
		{
			StopTicking();
			return;
		}

		LF_BusStopConfig config = LF_BusStopConfig.Get();
		float now = LF_CharacterSwitch.Now();
		array<int> done = {};
		foreach (int slot, LF_BusTrip trip : s_mTrips)
		{
			if (trip.m_bDeparting)
				continue;

			IEntity body = Resolve(trip.m_BodyId);
			LF_EBusStep step = LF_EBusStep.CANCEL_STATE;
			if (body)
			{
				LF_CharacterSwitchState state = ReadState(trip.m_iPlayerId, body, trip.m_sKey);
				step = Step(trip, config, now, body.GetOrigin(), state, LF_CharacterSwitch.LastHitAt(trip.m_sKey), HealthOf(body));
			}

			if (step == LF_EBusStep.WAIT)
				continue;

			if (step == LF_EBusStep.DEPART)
			{
				vector spot;
				if (!LF_BusArrival.Find(LF_Places.Find(PLACE_KIND, trip.m_sTo), body, spot))
					step = LF_EBusStep.CANCEL_NO_ARRIVAL;
				else
				{
					Depart(trip, body, spot, config);
					if (trip.m_bDeparting)
						continue;
				}
			}

			if (step != LF_EBusStep.DEPART)
				Cancel(trip, body, step);

			done.Insert(slot);
		}

		foreach (int slot : done)
			s_mTrips.Remove(slot);

		if (s_mTrips.IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the waiting trip of a player (or of a test body), or null.
	static LF_BusTrip TripOf(int playerId, string key)
	{
		if (!s_mTrips)
			return null;

		foreach (int slot, LF_BusTrip trip : s_mTrips)
		{
			if (playerId > 0 && trip.m_iPlayerId == playerId)
				return trip;

			if (trip.m_sKey == key)
				return trip;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static int CountTrips()
	{
		if (!s_mTrips)
			return 0;

		return s_mTrips.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: make the trip leave on the next tick.
	static void DepartNowForTest(string key)
	{
		LF_BusTrip trip = TripOf(0, key);
		if (trip)
			trip.m_fDepartAt = LF_CharacterSwitch.Now();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the passenger takes back a waiting ticket (the "Cancel trip" action). The fare is refunded
	//! like any other cancel. False, logged, when there is no waiting trip or the bus is already leaving.
	static bool CancelAsked(int playerId, string key)
	{
		LF_BusTrip trip = TripOf(playerId, key);
		if (!trip)
		{
			LF_Debug.Warn(FEATURE, string.Format("bus cancel refused for %1: no waiting ticket", key));
			return false;
		}

		if (trip.m_bDeparting)
		{
			LF_Debug.Warn(FEATURE, string.Format("bus cancel refused for %1: the bus is already leaving", trip.m_sKey));
			return false;
		}

		s_mTrips.Remove(trip.m_iSlot);
		Cancel(trip, Resolve(trip.m_BodyId), LF_EBusStep.CANCEL_ASKED);
		if (s_mTrips.IsEmpty())
			StopTicking();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The string-table key of a refusal, for the action list and the toast.
	static string RefusalKey(LF_EBusRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EBusRefusal.NONE: return "#LF-BusStop_Boarded";
			case LF_EBusRefusal.NO_STOP: return "#LF-BusStop_NoStop";
			case LF_EBusRefusal.SAME_STOP: return "#LF-BusStop_SameStop";
			case LF_EBusRefusal.NOT_AT_STOP: return "#LF-BusStop_NotAtStop";
			case LF_EBusRefusal.DEAD: return "#LF-BusStop_Dead";
			case LF_EBusRefusal.DOWNED: return "#LF-BusStop_Downed";
			case LF_EBusRefusal.CUFFED: return "#LF-BusStop_Cuffed";
			case LF_EBusRefusal.DRIVING: return "#LF-BusStop_Driving";
			case LF_EBusRefusal.COMBAT: return "#LF-BusStop_Combat";
			case LF_EBusRefusal.PURSUIT: return "#LF-BusStop_Pursuit";
			case LF_EBusRefusal.NO_CASH: return "#LF-BusStop_NoCash";
			case LF_EBusRefusal.BUSY: return "#LF-BusStop_Busy";
			case LF_EBusRefusal.TOO_SOON: return "#LF-BusStop_TooSoon";
		}

		return "#LF-BusStop_NoCharacter";
	}

	//------------------------------------------------------------------------------------------------
	//! The string-table key of a cancelled wait.
	static string CancelKey(LF_EBusStep step)
	{
		switch (step)
		{
			case LF_EBusStep.CANCEL_MOVED: return "#LF-BusStop_CancelMoved";
			case LF_EBusStep.CANCEL_HURT: return "#LF-BusStop_CancelHurt";
			case LF_EBusStep.CANCEL_NO_ARRIVAL: return "#LF-BusStop_CancelNoArrival";
			case LF_EBusStep.CANCEL_ASKED: return "#LF-BusStop_CancelAsked";
		}

		return "#LF-BusStop_CancelState";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		s_mTrips = null;
		s_mAskedAt = null;
		s_mArrivedAt = null;
		s_iTrips = 0;
		s_iBodySlots = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EBusRefusal Refuse(LF_EBusRefusal refusal, string key, string fromId, string toId)
	{
		LF_Debug.Warn(FEATURE, string.Format("bus ticket refused for %1 (%2 to %3): %4", key, fromId, toId, typename.EnumToString(LF_EBusRefusal, refusal)));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cancel(notnull LF_BusTrip trip, IEntity body, LF_EBusStep step)
	{
		int refunded = 0;
		string where = "cash";
		if (body && trip.m_iFare > 0)
			refunded = LF_MoneyUtils.AddCash(body, trip.m_iFare, REASON_REFUND, "", true);

		// No body to hold the cash (a character switch or a rejoin removed it mid-wait): the fare goes to
		// that character's bank account, keyed by the same character key, so it is never lost.
		if (refunded < trip.m_iFare && !trip.m_sKey.IsEmpty() && Replication.IsServer())
		{
			LF_BankAccount account = LF_ATMManager.GetOrCreate(trip.m_sKey);
			if (account)
			{
				account.Deposit(trip.m_iFare - refunded, REASON_REFUND);
				refunded = trip.m_iFare;
				where = "bank";
			}
		}

		LF_Debug.Info(FEATURE, string.Format("bus wait cancelled for %1 (%2 to %3): %4, refunded %5 of %6 to %7", trip.m_sKey, trip.m_sFrom, trip.m_sTo, typename.EnumToString(LF_EBusStep, step), refunded, trip.m_iFare, where));
		LF_BusTravelClient.SendDone(trip.m_iPlayerId, false);
		if (trip.m_iPlayerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(trip.m_iPlayerId, "#LF-BusStop_Title", CancelKey(step), 6.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Fade out on the passenger's screen, then the move. A zero fade (tests) moves at once.
	protected static void Depart(notnull LF_BusTrip trip, notnull IEntity body, vector spot, LF_BusStopConfig config)
	{
		LF_BusTravelClient.SendFade(trip.m_iPlayerId, true, config.m_fFadeSeconds);
		int delayMs = Math.Round(config.m_fFadeSeconds * 1000);
		if (delayMs <= 0)
		{
			Arrive(trip, spot);
			return;
		}

		trip.m_bDeparting = true;
		GetGame().GetCallqueue().CallLater(ArriveLater, delayMs, false, trip.m_iSlot, spot);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ArriveLater(int slot, vector spot)
	{
		if (!s_mTrips)
			return;

		LF_BusTrip trip = s_mTrips.Get(slot);
		if (!trip)
			return;

		s_mTrips.Remove(slot);
		Arrive(trip, spot);
		if (s_mTrips.IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Arrive(notnull LF_BusTrip trip, vector spot)
	{
		IEntity body = Resolve(trip.m_BodyId);
		LF_BusStopConfig config = LF_BusStopConfig.Get();
		// The tick skips a departing trip, so every rule is checked again here: cuffed, hit, hurt, in a
		// vehicle or walked off inside the fade, the bus does not take them and the fare goes back.
		LF_EBusStep step = LF_EBusStep.CANCEL_STATE;
		if (body)
		{
			LF_CharacterSwitchState state = ReadState(trip.m_iPlayerId, body, trip.m_sKey);
			step = Step(trip, config, trip.m_fDepartAt, body.GetOrigin(), state, LF_CharacterSwitch.LastHitAt(trip.m_sKey), HealthOf(body));
		}

		if (step != LF_EBusStep.DEPART)
		{
			Cancel(trip, body, step);
			LF_BusTravelClient.SendFade(trip.m_iPlayerId, false, config.m_fFadeSeconds);
			return;
		}

		IEntity stop = LF_Places.Find(PLACE_KIND, trip.m_sTo);
		if (stop)
			LF_Jail.MoveBodyFacing(body, spot, LF_BusArrival.FacingAt(stop));
		else
			LF_Jail.MoveBody(body, spot);

		if (!s_mArrivedAt)
			s_mArrivedAt = new map<string, float>();

		s_mArrivedAt.Set(trip.m_sKey, LF_CharacterSwitch.Now());
		LF_BusTravelClient.SendFade(trip.m_iPlayerId, false, config.m_fFadeSeconds);
		LF_BusTravelClient.SendDone(trip.m_iPlayerId, true);
		LF_Debug.Info(FEATURE, string.Format("bus arrived: %1 from %2 to %3 at %4 (fare %5)", trip.m_sKey, trip.m_sFrom, trip.m_sTo, spot, trip.m_iFare));
		if (trip.m_iPlayerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(trip.m_iPlayerId, "#LF-BusStop_Title", "#LF-BusStop_Arrived", 5.0, LF_ENotificationType.INFO, LF_BusStopComponent.LabelOf(LF_Places.Find(PLACE_KIND, trip.m_sTo), trip.m_sTo));
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CharacterSwitchState ReadState(int playerId, IEntity body, string key)
	{
		LF_CharacterSwitchState state = new LF_CharacterSwitchState();
		LF_PlayerAccount account;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (playerId > 0 && accounts)
		{
			string accountUid = LF_Utils.GetAccountUID(playerId);
			if (!accountUid.IsEmpty())
				account = accounts.GetAccount(accountUid);
		}

		return LF_CharacterSwitch.ReadStateFor(state, account, key, body);
	}

	//------------------------------------------------------------------------------------------------
	protected static float HealthOf(IEntity body)
	{
		if (!body)
			return 0;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return 0;

		return damage.GetHealth();
	}

	//------------------------------------------------------------------------------------------------
	//! A player's slot is their id; a body with no player (a test) takes the next negative slot.
	protected static int NewSlot(int playerId)
	{
		if (playerId > 0)
			return playerId;

		s_iBodySlots++;
		return -s_iBodySlots;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<int, ref LF_BusTrip> Trips()
	{
		if (!s_mTrips)
			s_mTrips = new map<int, ref LF_BusTrip>();

		return s_mTrips;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Resolve(EntityID id)
	{
		if (id == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BusStop's statics (LF_StaticReset finds it by reflection).
class LF_BusStop_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BusStop";
	}

	override void Apply()
	{
		LF_BusStop.ResetStatics();
		LF_BusStopConfig.Reload();
	}
}
