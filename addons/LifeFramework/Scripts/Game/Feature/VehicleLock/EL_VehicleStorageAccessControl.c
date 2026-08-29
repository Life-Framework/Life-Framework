//------------------------------------------------------------------------------------------------
//! Vehicle storage access control: the single rule that says whether a user may open this
//! vehicle's storage. Consumed by the modded SCR_OpenVehicleStorageAction, which is the
//! authoritative gate: a locked vehicle denies opening the storage UI, so no items move.
//!
//! Vanilla vehicles without this component and without EL_VehicleLockComponent keep their
//! vanilla storage behavior: IsStorageLocked falls through to false.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/VehicleLock", description: "Denies storage access while the owner vehicle is locked.")]
class EL_VehicleStorageAccessControlClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleStorageAccessControl : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! \param user The entity requesting access (unused; the lock is vehicle-wide).
	//! \return True when the owner vehicle is locked, denying storage access.
	bool IsStorageLocked(IEntity user)
	{
		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(GetOwner());
		if (!vehicleLock)
			return false;

		return vehicleLock.IsLocked(user, null);
	}
}