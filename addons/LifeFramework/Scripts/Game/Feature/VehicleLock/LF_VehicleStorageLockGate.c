//------------------------------------------------------------------------------------------------
//! Server-side lock on a locked vehicle's storage (#2411). Every inventory move, vanilla drag and
//! drop or LF RPC, asks the source storage CanRemoveItem and the target storage CanStoreItem on
//! the server that performs it. A storage that sits on a locked vehicle (its owner, or anything
//! the owner hangs from, carries a locked LF_VehicleLockComponent) refuses both, so a modified
//! client cannot take from or stash in a locked car by any path. The open action keeps its own
//! UI gate (LF_OpenVehicleStorageAction.c).
//!
//! A character seated in the car hangs from the car too: the climb stops at a living character, so what
//! the occupant carries stays theirs to move. The VehicleLock switch (#1076) turns the gate off
//! with the rest of the lock.
//------------------------------------------------------------------------------------------------
class LF_VehicleStorageLockGate
{
	//------------------------------------------------------------------------------------------------
	//! \return The locked vehicle lock this storage sits on, or null when the storage is open.
	static LF_VehicleLockComponent LockedVehicleOf(BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return null;

		IEntity entity = storage.GetOwner();
		while (entity)
		{
			if (IsLiveCharacter(entity))
				return null;

			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(entity);
			if (vehicleLock)
			{
				if (vehicleLock.IsVehicleLocked() && LF_Systems.IsEnabled("VehicleLock"))
					return vehicleLock;

				return null;
			}

			entity = entity.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A living character owns what it carries, even seated in a locked car. A dead body left in
	//! the seat does not: it stays behind the car's lock.
	static bool IsLiveCharacter(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		DamageManagerComponent damage = character.GetDamageManager();
		return !damage || damage.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the move must be refused; logs the refusal once per call.
	static bool Refuses(BaseInventoryStorageComponent storage, IEntity item, string verb)
	{
		LF_VehicleLockComponent vehicleLock = LockedVehicleOf(storage);
		if (!vehicleLock)
			return false;

		// Only the server's answer decides. The client copy refuses too, so its UI matches,
		// but the log line is written where the move is actually refused.
		if (Replication.IsServer())
			LF_Debug.Warn("VehicleLock", string.Format("%1 refused: vehicle %2 is locked", verb, vehicleLock.GetVehicleIdentifier()));

		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_UniversalInventoryStorageComponent
{
	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (!super.CanStoreItem(item, slotID))
			return false;

		return !LF_VehicleStorageLockGate.Refuses(this, item, "store");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanRemoveItem(IEntity item)
	{
		if (!super.CanRemoveItem(item))
			return false;

		return !LF_VehicleStorageLockGate.Refuses(this, item, "take");
	}
}
