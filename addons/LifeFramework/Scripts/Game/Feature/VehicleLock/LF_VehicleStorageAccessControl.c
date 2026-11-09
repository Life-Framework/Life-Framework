//------------------------------------------------------------------------------------------------
//! Vehicle storage access control: the single rule that says whether a user may open this
//! vehicle's storage. Consumed by the modded SCR_OpenVehicleStorageAction, which is the
//! authoritative gate: a locked vehicle denies opening the storage UI, so no items move.
//!
//! Vanilla vehicles without this component and without LF_VehicleLockComponent keep their
//! vanilla storage behavior: IsStorageLocked falls through to false.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/VehicleLock", description: "Denies storage access while the owner vehicle is locked.")]
class LF_VehicleStorageAccessControlClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_VehicleStorageAccessControl : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! \param user The entity requesting access (unused; the lock is vehicle-wide).
	//! \return True when the owner vehicle is locked, denying storage access.
	bool IsStorageLocked(IEntity user)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(GetOwner());
		if (!vehicleLock)
			return false;

		return vehicleLock.IsLocked(user, null);
	}
}