//------------------------------------------------------------------------------------------------
//! "Open storage" on a hideout stash. The client does not open the stash itself: it asks the
//! server (SCR_PlayerController.LF_RequestOpenStash), which checks membership and either opens
//! the stash on that client or refuses with a sound and a toast.
//------------------------------------------------------------------------------------------------
class LF_HideoutStashOpenAction : SCR_OpenStorageAction
{
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			EL_Debug.Error("Hideout", "stash open dropped: no local player controller");
			return;
		}

		controller.LF_RequestOpenStash(pOwnerEntity);
	}
	#endif
}
