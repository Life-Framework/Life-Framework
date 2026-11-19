//------------------------------------------------------------------------------------------------
//! Keeps owned vehicles out of the engine's world save (#3614). An owned car is a plain vehicle entity,
//! so the engine's PersistenceSystem tracked it and wrote it; at a restart it put the car back where it
//! was parked, as a fresh entity with a new lock identifier and no owner, while LF_VehicleOwnership also
//! sent the record to the restart impound. Collecting it there made a second car, and the copy kept the
//! cargo the engine saved while ApplyCargo put the snapshot into the impound car. The vehicle record and
//! its cargo snapshot are an owned car's only save: every car LF_VehicleOwnership spawns stops being
//! tracked, and a record it had is dropped at the next save.
class LF_VehicleWorldSave
{
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

		if (system.IsTracked(vehicle))
			system.StopTracking(vehicle, true);

		if (system.IsTracked(vehicle))
		{
			LF_Debug.Error("Vehicle", string.Format("an owned car (%1) is still in the world save: a restart would put back an unowned copy", LF_Utils.GetPrefabName(vehicle)));
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
}
