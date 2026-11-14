//------------------------------------------------------------------------------------------------
//! "Open storage" on a hideout stash. The client does not open the stash itself: it asks the
//! server (SCR_PlayerController.LF_RequestOpenStash), which checks membership and either opens
//! the stash on that client or refuses with a sound and a toast.
//------------------------------------------------------------------------------------------------
class LF_HideoutStashOpenAction : SCR_OpenStorageAction
{
	//------------------------------------------------------------------------------------------------
	//! A server with Hideouts off offers no stash (#1075).
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_Hideout.SYSTEM))
			return false;

		return super.CanBeShownScript(user);
	}

	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("Hideout", "stash open dropped: no local player controller");
			return;
		}

		controller.LF_RequestOpenStash(pOwnerEntity);
	}
	#endif
}
