//------------------------------------------------------------------------------------------------
//! "Open the container" on a short container (#4195): with the tally clerk's key, the whole stock
//! moves into the player's inventory; without it, or while it is empty, the row says why.
class LF_StashContainerAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	protected LF_StashContainerComponent GetContainer()
	{
		return LF_Component<LF_StashContainerComponent>.Find(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

		return GetContainer() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_StashContainerComponent container = GetContainer();
		if (!container)
			return false;

		LF_EStashTake check = container.Check(user);
		if (check == LF_EStashTake.OK)
			return true;

		SetCannotPerformReason(LF_StashContainerStock.RefusalKey(check));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_StashContainerAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_StashContainerComponent container = LF_Component<LF_StashContainerComponent>.Find(pOwnerEntity);
		if (!container)
		{
			LF_Debug.Error(LF_StashContainerStock.FEATURE, "open refused: the action's owner has no LF_StashContainerComponent");
			return;
		}

		container.Take(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-StashContainer_Open";
		return true;
	}
}
