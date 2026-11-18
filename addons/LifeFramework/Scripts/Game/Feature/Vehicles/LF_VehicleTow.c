//------------------------------------------------------------------------------------------------
//! Abandoned cars are towed (#2657, the tow part of #774 and #859). A player's car left on the
//! street with nobody in it and its owner more than OWNER_NEAR_METRES away for
//! LF_GarageConfig.TowAfterMinutes is taken off the street. Its record reads TOWED with the unix
//! second it was towed (the "vehiclestate" sidecar, no new payload); it waits at the impound desk
//! like a restart-impounded car, and the desk releases it for LF_GarageConfig.TowFee.
//!
//! Never towed: a car someone sits in, a police car (it carries a siren knob), a car within
//! LF_GarageConfig.TowExemptNearGarage of a garage, and a car flagged stolen (the police and the
//! owner deal with that). Job vehicles are not owned records, so the sweep never sees them.
//------------------------------------------------------------------------------------------------
enum LF_ETowCheck
{
	//! Tow it now.
	TOW,
	//! Somebody is with it: the clock starts again.
	ATTENDED,
	//! Never towed (police car, near a garage, stolen): the clock starts again.
	EXEMPT,
	//! Unattended, but not for long enough yet.
	WAITING
}

//------------------------------------------------------------------------------------------------
//! The pure rule. No world access.
class LF_VehicleTowRules
{
	//------------------------------------------------------------------------------------------------
	//! \param unattendedSince Unix second the car was first seen unattended; 0 when it was attended at the last sweep.
	static LF_ETowCheck Check(bool occupied, bool ownerNear, bool policeCar, bool nearGarage, bool stolen, int unattendedSince, int now, int towAfterSeconds)
	{
		if (occupied || ownerNear)
			return LF_ETowCheck.ATTENDED;

		if (policeCar || nearGarage || stolen)
			return LF_ETowCheck.EXEMPT;

		if (unattendedSince <= 0)
			return LF_ETowCheck.WAITING;

		if (now - unattendedSince < towAfterSeconds)
			return LF_ETowCheck.WAITING;

		return LF_ETowCheck.TOW;
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleTow
{
	static const string FEATURE = "VehicleTow";
	static const int SWEEP_MS = 60000; // lf-fixed: the card's cadence; the delay itself is config
	static const float OWNER_NEAR_METRES = 50; // lf-fixed: "abandoned" = no owner within this

	//! Vehicle id -> unix second the car was first seen unattended (session state: a restart
	//! impounds every street car anyway).
	protected static ref map<string, int> s_mUnattendedSince;
	protected static bool s_bScheduled;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Since()
	{
		if (!s_mUnattendedSince)
			s_mUnattendedSince = new map<string, int>();

		return s_mUnattendedSince;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
		s_mUnattendedSince = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sweep every SWEEP_MS. Called from the game mode start; idempotent.
	static void Start()
	{
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, SWEEP_MS, true);
		LF_Debug.Info(FEATURE, string.Format("tow sweep every %1 ms, tow after %2 min", SWEEP_MS, LF_GarageConfig.TowAfterMinutes()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Sweep(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the owned cars standing in the world.
	//! \param now Unix seconds.
	//! \return The number of cars towed.
	static int Sweep(int now)
	{
		if (!Replication.IsServer() || !LF_Systems.IsEnabled("Vehicles"))
			return 0;

		int towAfterSeconds = LF_GarageConfig.TowAfterMinutes() * 60;
		float exemptRadius = LF_GarageConfig.TowExemptNearGarage();
		int towed = 0;
		array<string> seen = {};
		array<ref LF_VehicleRecord> records = LF_VehicleOwnership.ExportAll();
		foreach (LF_VehicleRecord record : records)
		{
			if (record.m_bStored)
				continue;

			string vehicleId = record.m_sVehicleId;
			IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
			if (!vehicle)
				continue;

			seen.Insert(vehicleId);
			bool occupied = LF_Garage.HasOccupants(vehicle);
			bool ownerNear = OwnerNear(record.m_sOwnerUid, vehicle.GetOrigin());
			bool policeCar = LF_Component<LF_SirenKnobComponent>.Find(vehicle) != null;
			bool nearGarage = NearGarage(vehicle.GetOrigin(), exemptRadius);
			bool stolen = false;
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			if (vehicleLock)
				stolen = vehicleLock.IsStolen();

			int since = 0;
			if (Since().Contains(vehicleId))
				since = Since().Get(vehicleId);

			LF_ETowCheck check = LF_VehicleTowRules.Check(occupied, ownerNear, policeCar, nearGarage, stolen, since, now, towAfterSeconds);
			if (check == LF_ETowCheck.ATTENDED || check == LF_ETowCheck.EXEMPT)
			{
				Since().Remove(vehicleId);
				continue;
			}

			if (check == LF_ETowCheck.WAITING)
			{
				if (since <= 0)
					Since().Set(vehicleId, now);
				continue;
			}

			if (Tow(vehicleId, vehicle, now))
				towed++;
		}

		// A car stored, collected or scrapped since the last pass starts fresh next time.
		array<string> stale = {};
		foreach (string trackedId, int trackedSince : Since())
		{
			if (seen.Find(trackedId) < 0)
				stale.Insert(trackedId);
		}

		foreach (string staleId : stale)
		{
			Since().Remove(staleId);
		}

		return towed;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the car off the street and marks it TOWED. The inspection and compartment fields keep
	//! their values.
	protected static bool Tow(string vehicleId, IEntity vehicle, int now)
	{
		LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(vehicleId);
		if (!state)
			return false;

		string ownerUid = LF_VehicleOwnership.GetOwner(vehicleId);
		string plate = LF_VehicleOwnership.GetPlate(vehicleId);
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (vehicleLock)
			LF_VehicleOwnership.NoteLockState(vehicleId, vehicleLock.IsVehicleLocked());

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		if (LF_VehicleOwnership.GetVehicle(vehicleId))
		{
			LF_Debug.Error(FEATURE, string.Format("tow of %1 failed: the entity is still in the world", vehicleId));
			return false;
		}

		LF_VehicleOwnership.SetState(vehicleId, LF_EVehicleTowState.TOWED, now, state.m_iInspectedAt, state.m_iCompartment);
		Since().Remove(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("towed %1 owner=%2 plate=%3 at %4", vehicleId, ownerUid, plate, now));

		int playerId = PlayerIdOf(ownerUid);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Impound_Title", "#LF-Tow_Towed", 6.0, LF_ENotificationType.WARNING, plate);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What the impound desk charges: the tow fee for a towed car, else the impound fee.
	static int FeeOf(string vehicleId)
	{
		LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(vehicleId);
		if (state && state.m_iTowState == LF_EVehicleTowState.TOWED)
			return LF_GarageConfig.TowFee();

		return LF_VehicleOwnership.ImpoundFeeOf(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	//! The impound desk's release: charges FeeOf, and a released towed car is no longer TOWED.
	static bool Collect(string vehicleId, string ownerUid, IEntity payer, string padId, out string reasonKey)
	{
		int fee = FeeOf(vehicleId);
		if (!LF_VehicleOwnership.CollectFromImpound(vehicleId, ownerUid, payer, fee, padId, reasonKey))
			return false;

		LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(vehicleId);
		if (state && state.m_iTowState == LF_EVehicleTowState.TOWED)
		{
			LF_VehicleOwnership.SetState(vehicleId, LF_EVehicleTowState.NONE, 0, state.m_iInspectedAt, state.m_iCompartment);
			LF_Debug.Info(FEATURE, string.Format("towed %1 released to %2 for %3", vehicleId, ownerUid, fee));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool NearGarage(vector origin, float radius)
	{
		foreach (LF_VehicleGarageComponent garage : LF_VehicleGarageComponent.GetAll())
		{
			if (garage && garage.GetOwner() && vector.Distance(origin, garage.GetOwner().GetOrigin()) <= radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OwnerNear(string ownerUid, vector origin)
	{
		int playerId = PlayerIdOf(ownerUid);
		if (playerId <= 0)
			return false;

		IEntity owner = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		return owner && vector.Distance(owner.GetOrigin(), origin) <= OWNER_NEAR_METRES;
	}

	//------------------------------------------------------------------------------------------------
	//! The online player with this account uid, or 0.
	protected static int PlayerIdOf(string uid)
	{
		if (uid.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return playerId;
		}

		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleTow's statics (LF_StaticReset finds it by reflection).
class LF_VehicleTow_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleTow";
	}

	override void Apply()
	{
		LF_VehicleTow.ResetStatics();
	}
}
