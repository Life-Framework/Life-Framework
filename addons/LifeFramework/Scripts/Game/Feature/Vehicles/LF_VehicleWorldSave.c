//------------------------------------------------------------------------------------------------
//! Keeps session-owned vehicles out of the engine's world save. A car is a plain vehicle entity, so the
//! engine's PersistenceSystem tracked it and wrote it; at a restart it put the car back where it stood, as a
//! fresh entity with a new lock identifier, no owner and the lock open.
//! - Owned cars (#3614): LF_VehicleOwnership also sent the record to the restart impound, so collecting it
//!   there made a second car, and the copy kept the cargo the engine saved. The vehicle record and its cargo
//!   snapshot are an owned car's only save.
//! - Parked-car marker cars, job trucks and scene vehicles (#4122): the marker or job puts its car out again,
//!   so the restored copy stood unlocked beside it, a free car.
//! The engine may register an entity lazily, after the spawn that held it, so every held vehicle is held
//! again right before each save.
class LF_VehicleWorldSave
{
	//! Every vehicle held this session, re-held before each save. Dead ids are dropped then.
	protected static ref array<EntityID> s_aHeld;
	protected static bool s_bSubscribed;

	//------------------------------------------------------------------------------------------------
	//! The vehicle leaves the world save; a record it had is removed at the next save.
	//! \return True when the vehicle is not tracked afterwards (also when the world has no persistence system).
	static bool Hold(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!system)
			return true;

		Remember(vehicle.GetID());
		if (system.IsTracked(vehicle))
			system.StopTracking(vehicle, true);

		if (system.IsTracked(vehicle))
		{
			LF_Debug.Error("Vehicle", string.Format("a car (%1) is still in the world save: a restart would put back an unlocked copy", LF_Utils.GetPrefabName(vehicle)));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the engine's world save writes this entity.
	static bool IsSaved(IEntity entity)
	{
		if (!entity)
			return false;

		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!system)
			return false;

		return system.IsTracked(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remember(EntityID id)
	{
		if (!s_aHeld)
			s_aHeld = new array<EntityID>();

		if (s_aHeld.Find(id) < 0)
			s_aHeld.Insert(id);

		if (s_bSubscribed)
			return;

		SCR_PersistenceSystem system = SCR_PersistenceSystem.GetScriptedInstance();
		if (!system)
			return;

		system.GetOnBeforeSave().Insert(OnBeforeSave);
		s_bSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnBeforeSave(ESaveGameType saveType)
	{
		HoldAgain();
	}

	//------------------------------------------------------------------------------------------------
	//! Holds every remembered vehicle that still exists again, so one the engine registered after its spawn
	//! is not written. Public so a test drives it.
	//! \return How many vehicles had been tracked again and were taken out.
	static int HoldAgain()
	{
		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!s_aHeld || !system || !GetGame() || !GetGame().GetWorld())
			return 0;

		int retaken = 0;
		for (int i = s_aHeld.Count() - 1; i >= 0; i--)
		{
			IEntity vehicle = GetGame().GetWorld().FindEntityByID(s_aHeld[i]);
			if (!vehicle)
			{
				s_aHeld.Remove(i);
				continue;
			}

			if (!system.IsTracked(vehicle))
				continue;

			system.StopTracking(vehicle, true);
			retaken++;
		}

		if (retaken > 0)
			LF_Debug.Info("Vehicle", string.Format("before the save: %1 held car(s) the engine had registered late were taken out of the world save again", retaken));

		return retaken;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bSubscribed)
		{
			SCR_PersistenceSystem system = SCR_PersistenceSystem.GetScriptedInstance();
			if (system)
				system.GetOnBeforeSave().Remove(OnBeforeSave);
		}

		s_bSubscribed = false;
		s_aHeld = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleWorldSave's statics (LF_StaticReset finds it by reflection).
class LF_VehicleWorldSave_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleWorldSave";
	}

	override void Apply()
	{
		LF_VehicleWorldSave.ResetStatics();
	}
}
