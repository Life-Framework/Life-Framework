//! Feed from the truck (#4191): a van or truck backed up to a processing station gives the station its
//! inputs straight from the vehicle's storage. The server finds the vehicle again, checks the reach and
//! the lock, takes every input from the vehicle or none, and puts the product back in the vehicle (what
//! does not fit goes to the worker, then the ground at the station). The action is `LF_ProcessAction`
//! itself (LF_FeedFromVehicleAction.c), so every station offers it with no prefab change.
class LF_FeedFromVehicle
{
	static const string FEATURE = "Processing";

	//! Metres from the station to the nearest point of the vehicle's box: backed up, not parked nearby.
	static const float REACH = 3.0;

	//! Sphere the vehicle's origin is looked for in; covers a truck's half length plus the reach.
	protected static const float SEARCH_RADIUS = 12.0;

	//------------------------------------------------------------------------------------------------
	//! \return The metres from \p point to the nearest point of \p vehicle's local bounding box; a large
	//! number for a null vehicle.
	static float DistanceToBox(IEntity vehicle, vector point)
	{
		if (!vehicle)
			return float.MAX;

		vector mins, maxs;
		vehicle.GetBounds(mins, maxs);
		vector local = vehicle.CoordToLocal(point);
		vector nearest = Vector(Math.Clamp(local[0], mins[0], maxs[0]), Math.Clamp(local[1], mins[1], maxs[1]), Math.Clamp(local[2], mins[2], maxs[2]));
		return vector.Distance(local, nearest);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when \p vehicle is a live vehicle with a storage manager and its box is within REACH
	//! of \p station.
	static bool IsInReach(IEntity station, IEntity vehicle)
	{
		if (!station || !vehicle || !vehicle.IsInherited(Vehicle))
			return false;

		if (!LF_Component<InventoryStorageManagerComponent>.Find(vehicle))
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.GetState() == EDamageState.DESTROYED)
			return false;

		return DistanceToBox(vehicle, station.GetOrigin()) <= REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The nearest vehicle backed up to \p station, or null when none is in reach.
	static IEntity FindVehicle(IEntity station)
	{
		if (!station)
			return null;

		IEntity best;
		float bestDistance = float.MAX;
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(station.GetOrigin(), SEARCH_RADIUS);
		foreach (IEntity vehicle : nearby)
		{
			if (!IsInReach(station, vehicle))
				continue;

			float distance = DistanceToBox(vehicle, station.GetOrigin());
			if (distance < bestDistance)
			{
				best = vehicle;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the vehicle lock system is on and \p vehicle, or anything it hangs from, is
	//! locked: a locked truck feeds nothing. The feed removes items directly, so the storage lock gate
	//! never runs and this is the only lock check on the path.
	static bool IsLocked(IEntity vehicle)
	{
		if (!LF_Systems.IsEnabled("VehicleLock"))
			return false;

		IEntity entity = vehicle;
		while (entity)
		{
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(entity);
			if (vehicleLock && vehicleLock.IsVehicleLocked())
				return true;

			entity = entity.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle's storage manager, or null.
	static InventoryStorageManagerComponent StorageOf(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		return LF_Component<InventoryStorageManagerComponent>.Find(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers \p outputs. A force-drop station (\p forceDrop) drops them at the station, as a carried
	//! process does. Any other puts them in the vehicle, then the worker's inventory, and drops what still
	//! does not fit at the station. On a short delivery every piece already given is taken back (the
	//! dropped ones by entity), so nothing is left behind to duplicate.
	//! \return True when every output was delivered.
	static bool DeliverOutputs(InventoryStorageManagerComponent vehicleStorage, IEntity user, IEntity station, array<ref LF_ProcessingOutput> outputs, bool forceDrop, vector dropOffset, vector rotation)
	{
		InventoryStorageManagerComponent userStorage = LF_InventoryUtils.GetResponsibleStorageManager(user);
		array<int> inVehicle = {};
		array<int> withUser = {};
		array<IEntity> dropped = {};
		bool ok = true;
		foreach (LF_ProcessingOutput output : outputs)
		{
			int toVehicle = 0;
			int toUser = 0;
			if (!forceDrop)
			{
				toVehicle = LF_InventoryUtils.AddAmount(vehicleStorage, output.m_OutputPrefab, output.m_iOutputAmount, false);
				if (toVehicle < output.m_iOutputAmount && userStorage)
					toUser = LF_InventoryUtils.AddAmount(userStorage, output.m_OutputPrefab, output.m_iOutputAmount - toVehicle, false);
			}

			inVehicle.Insert(toVehicle);
			withUser.Insert(toUser);
			for (int i = toVehicle + toUser; i < output.m_iOutputAmount; i++)
			{
				IEntity item = LF_Utils.SpawnEntityPrefab(output.m_OutputPrefab, station.GetOrigin() + dropOffset, rotation);
				if (!item)
				{
					LF_Debug.Error(FEATURE, string.Format("feed from vehicle: output spawn failed for %1", output.m_OutputPrefab));
					ok = false;
					break;
				}

				dropped.Insert(item);
			}

			if (!ok)
				break;
		}

		if (ok)
			return true;

		for (int j = 0; j < inVehicle.Count(); j++)
		{
			if (inVehicle[j] > 0)
				LF_InventoryUtils.RemoveAmount(vehicleStorage, outputs[j].m_OutputPrefab, inVehicle[j]);
			if (withUser[j] > 0)
				LF_InventoryUtils.RemoveAmount(userStorage, outputs[j].m_OutputPrefab, withUser[j]);
		}

		foreach (IEntity item : dropped)
			SCR_EntityHelper.DeleteEntityAndChildren(item);

		return false;
	}
}
