//------------------------------------------------------------------------------------------------
//! Lock/unlock vehicle action, wired on Vehicle_Base so every vehicle built on it is lockable.
//!
//! CAN PERFORM requires a matching key in the user's hand or inventory (client-side UI gate).
//! PERFORM re-validates the key on the server: the client's CanBePerformedScript is not a
//! security boundary, so a forged request cannot toggle the lock without holding the key.
//! Feedback goes through the local notification manager (server to player), not a hint on the
//! performing entity, so remote players see it.
//------------------------------------------------------------------------------------------------
class LF_VehicleLockAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(GetOwner());
		if (!vehicleLock)
		{
			SetCannotPerformReason("#EL-VehicleLock_Reason");
			return false;
		}

		if (!vehicleLock.UserHasValidKey(user))
		{
			SetCannotPerformReason("#EL-VehicleLock_MissingKey");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity || !Replication.IsServer())
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(pOwnerEntity);
		if (!vehicleLock)
			return;

		vehicleLock.TryToggleBy(pUserEntity, "menu");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(GetOwner());
		if (!vehicleLock)
		{
			outName = "#EL-VehicleLock_Action";
			return true;
		}

		outName = LF_VehicleLockComponent.ActionLabelKey(vehicleLock.IsVehicleLocked());
		return true;
	}
}