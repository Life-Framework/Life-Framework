//------------------------------------------------------------------------------------------------
//! What a check of an inspection sticker reads (#3433).
enum LF_EInspectionStatus
{
	NONE,		//!< The car was never inspected (this session).
	VALID,		//!< A sticker whose expiry day has not come yet.
	EXPIRED		//!< A sticker whose expiry day has come or gone.
}

//------------------------------------------------------------------------------------------------
//! Inspection stickers (#3433, card Q8 of #1035): an officer's inspection stamps a sticker that is
//! valid for N game days; a road check reads valid, expired or none. The sticker is the expiry game
//! day (days since 1970-01-01, the licence desk's day count) per vehicle id: the lock's vehicle
//! identifier, so a car keeps its sticker wherever it is driven. An owned car's sticker is saved in
//! the "vehiclecache" section (LF_VehicleCacheSave.c, #3435) under its garage id, which is also its
//! lock identifier; a car nobody owns loses its sticker at a restart. Server-side state; the actions
//! answer the officer by toast.
//------------------------------------------------------------------------------------------------
class LF_Inspection
{
	static const string FEATURE = "Inspection";

	//! Game days a fresh sticker stays valid when the caller gives no window.
	static const int VALID_DAYS = 7;	// lf-fixed: one game week; Q10 may move it to config

	//! How far from the action's owner (a desk) a car counts as "beside the desk".
	static const float REACH_METRES = 8;	// lf-fixed: a desk and the car parked at it

	protected static ref map<string, int> s_mExpiryDay;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Stickers()
	{
		if (!s_mExpiryDay)
			s_mExpiryDay = new map<string, int>();

		return s_mExpiryDay;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mExpiryDay = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps a sticker valid for `validDays` game days from `today`.
	//! \return The expiry game day, or -1 when the id is empty or the window is not at least one day (logged).
	static int Inspect(string vehicleId, int today, int validDays = VALID_DAYS)
	{
		if (vehicleId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "inspect refused: the vehicle has no id");
			return -1;
		}

		if (validDays < 1)
		{
			LF_Debug.Error(FEATURE, string.Format("inspect refused for %1: a window of %2 days is not valid", vehicleId, validDays));
			return -1;
		}

		int expiry = today + validDays;
		Stickers().Set(vehicleId, expiry);
		LF_Debug.Info(FEATURE, string.Format("vehicle %1 inspected on day %2, sticker valid to day %3", vehicleId, today, expiry));
		return expiry;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets a sticker's expiry day from a save (#3435); a negative day removes the sticker.
	static void Restore(string vehicleId, int expiry)
	{
		if (vehicleId.IsEmpty())
			return;

		if (expiry < 0)
			Stickers().Remove(vehicleId);
		else
			Stickers().Set(vehicleId, expiry);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Valid while `today` is before the expiry day, expired from it on, none without a sticker.
	static LF_EInspectionStatus Check(string vehicleId, int today)
	{
		if (vehicleId.IsEmpty())
			return LF_EInspectionStatus.NONE;

		int expiry;
		if (!Stickers().Find(vehicleId, expiry))
			return LF_EInspectionStatus.NONE;

		if (today < expiry)
			return LF_EInspectionStatus.VALID;

		return LF_EInspectionStatus.EXPIRED;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The expiry game day of the car's sticker, or -1 when it has none.
	static int ExpiryOf(string vehicleId)
	{
		int expiry;
		if (vehicleId.IsEmpty() || !Stickers().Find(vehicleId, expiry))
			return -1;

		return expiry;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Today's game day from the world clock, or -1 when the world has none (logged by the desk).
	static int Today()
	{
		int minutes = LF_LicenceDesk.NowMinutes();
		if (minutes < 0)
			return -1;

		return minutes / LF_LicenceDesk.MINUTES_PER_DAY;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id a car's sticker is kept under: its lock's vehicle identifier; empty for a car without one.
	static string IdOf(IEntity vehicle)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return string.Empty;

		return vehicleLock.GetVehicleIdentifier();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The car the action means: the owner itself when it is a vehicle, else the nearest vehicle
	//! within REACH_METRES of the owner (a desk). Null when there is none.
	static IEntity FindCar(IEntity owner)
	{
		if (!owner)
			return null;

		if (Vehicle.Cast(owner))
			return owner;

		if (!GetGame() || !GetGame().GetWorld())
			return null;

		LF_InspectionFinder finder = new LF_InspectionFinder(owner.GetOrigin());
		GetGame().GetWorld().QueryEntitiesBySphere(finder.m_vFrom, REACH_METRES, finder.Visit, null, EQueryEntitiesFlags.ALL);
		return finder.m_Best;
	}

	//------------------------------------------------------------------------------------------------
	//! The player id of a character entity, 0 when it has none.
	static int PlayerIdOf(IEntity user)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !user)
			return 0;

		return playerManager.GetPlayerIdFromControlledEntity(user);
	}
}

//------------------------------------------------------------------------------------------------
//! One query's state for LF_Inspection.FindCar: keeps the vehicle nearest to a point.
class LF_InspectionFinder
{
	vector m_vFrom;
	IEntity m_Best;
	protected float m_fBest;

	void LF_InspectionFinder(vector from)
	{
		m_vFrom = from;
		m_fBest = LF_Inspection.REACH_METRES + 1;
	}

	//------------------------------------------------------------------------------------------------
	bool Visit(IEntity entity)
	{
		if (!Vehicle.Cast(entity))
			return true;

		float distance = vector.Distance(entity.GetOrigin(), m_vFrom);
		if (distance < m_fBest)
		{
			m_fBest = distance;
			m_Best = entity;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Inspection's statics.
class LF_Inspection_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Inspection";
	}

	override void Apply()
	{
		LF_Inspection.ResetStatics();
	}
}
