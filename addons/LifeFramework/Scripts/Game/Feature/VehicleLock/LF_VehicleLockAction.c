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
		if (!vehicleLock || !vehicleLock.UserHasValidKey(pUserEntity))
			return;

		vehicleLock.ToggleLocked();
		LF_VehicleOwnership.NoteLockState(vehicleLock.GetVehicleIdentifier(), vehicleLock.IsVehicleLocked());

		PlayerManager playerManager = GetGame().GetPlayerManager();
		int playerId = playerManager.GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		if (vehicleLock.IsVehicleLocked())
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-VehicleLock_Title", "#EL-VehicleLocked");
		else
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-VehicleLock_Title", "#EL-VehicleUnlocked");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-VehicleLock_Action";
		return true;
	}
}