//------------------------------------------------------------------------------------------------
//! "Open your locker" on a locker bank (#2661). The engine runs PerformAction on the server for the
//! acting character; the server decides everything (LF_LockerStorageManagerComponent.OpenNearest:
//! bank from the character's own position, renter from their own record key) and, when the locker
//! opens, tells that one player's client to show the container (SCR_PlayerController.LF_SendOpenLocker).
class LF_LockerOpenAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_LockerBank.FEATURE);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Rentals are server data, so no renter check here: the server refuses a non-renter with a toast.
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_LockerBank.FEATURE, "LF_LockerOpenAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, "open action performed without a character");
			return;
		}

		IEntity container;
		LF_ELockerOpenResult result = LF_LockerStorageManagerComponent.OpenNearest(pUserEntity, container);
		if (result != LF_ELockerOpenResult.OPENED)
		{
			LF_Utils.NotifyEntity(pUserEntity, LF_LockerStorageManagerComponent.ResultKey(result), "#LF-LockerBank_Title", 4.0);
			return;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		int playerId = playerManager.GetPlayerIdFromControlledEntity(pUserEntity);
		SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
		RplComponent rpl = LF_Component<RplComponent>.Find(container);
		if (!controller || !rpl)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, string.Format("locker opened for player %1 but cannot be shown: controller or RplComponent missing", playerId));
			LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.Cast(container.FindComponent(LF_LockerStorageManagerComponent));
			if (manager)
				manager.Release("cannot be shown");
			return;
		}

		controller.LF_SendOpenLocker(rpl.Id());
	}
}
