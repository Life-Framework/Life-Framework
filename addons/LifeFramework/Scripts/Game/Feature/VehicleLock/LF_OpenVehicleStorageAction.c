//------------------------------------------------------------------------------------------------
//! Storage access gate for locked vehicles, as a modded override of the vanilla
//! SCR_OpenVehicleStorageAction (not a replaced action class). The vanilla action keeps its
//! full behavior; the lock check is an extra gate on both CanBeShownScript (client UI) and
//! CanBePerformedScript (perform path).
//!
//! SECURITY POSTURE. This is the UI gate: SCR_InventoryAction is a local-effect action, so it
//! runs on the client. The server-side lock is LF_VehicleStorageLockGate (#2411): every storage
//! on a locked vehicle refuses CanRemoveItem and CanStoreItem on the server, so a client that
//! skips this action still cannot take or stash anything.
//!
//! The gate reads LF_VehicleStorageAccessControl on m_Vehicle, so it only applies to vehicles
//! that carry the access control component. Vanilla vehicles without it behave exactly as
//! before.
//------------------------------------------------------------------------------------------------
modded class SCR_OpenVehicleStorageAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		return !IsStorageLocked(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (IsStorageLocked(user))
		{
			SetCannotPerformReason("#EL-VehicleLock_Reason");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsStorageLocked(IEntity user)
	{
		if (!m_Vehicle)
			return false;

		LF_VehicleStorageAccessControl accessControl = LF_Component<LF_VehicleStorageAccessControl>.Find(m_Vehicle);
		if (!accessControl)
			return false;

		return accessControl.IsStorageLocked(user);
	}
}