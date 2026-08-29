//------------------------------------------------------------------------------------------------
//! Storage access gate for locked vehicles, as a modded override of the vanilla
//! SCR_OpenVehicleStorageAction (not a replaced action class). The vanilla action keeps its
//! full behavior; the lock check is an extra gate on both CanBeShownScript (client UI) and
//! CanBePerformedScript (perform path).
//!
//! SECURITY POSTURE. This is a soft UI gate matching the vanilla model: SCR_InventoryAction is
//! a local-effect action, so the gate runs on the client and a modified client can bypass it.
//! The vanilla faction checks on inventory have the same softness. Server-side enforcement of
//! the lock against inventory transfers is not wired here; that is a follow-up when vehicle
//! ownership lands.
//!
//! The gate reads EL_VehicleStorageAccessControl on m_Vehicle, so it only applies to vehicles
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

		EL_VehicleStorageAccessControl accessControl = EL_Component<EL_VehicleStorageAccessControl>.Find(m_Vehicle);
		if (!accessControl)
			return false;

		return accessControl.IsStorageLocked(user);
	}
}