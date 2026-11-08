[BaseContainerProps(configRoot: true)]
class LF_QuestAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		return EL_Component<LF_QuestGiverComponent>.Find(owner) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (vector.Distance(user.GetOrigin(), owner.GetOrigin()) > 3.0)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The menu is local UI: run PerformAction only on the acting client. Without this a listen
	//! host also runs it for every other player and opens the menu on the host screen. Server
	//! work goes through the menu RPCs, which the server validates.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		LF_QuestGiverComponent giver = EL_Component<LF_QuestGiverComponent>.Find(pOwnerEntity);
		if (!giver)
			return;

		// A converse giver (the town clerk) talks in person: the server moves the quests along.
		if (giver.IsConverse())
		{
			LF_QuestTrackerComponent tracker = EL_Component<LF_QuestTrackerComponent>.Find(pUserEntity);
			if (tracker)
				tracker.AskConverse(giver.GetGiverId());
			else
				EL_Debug.Error("Quests", "talk not sent: the player has no quest tracker");
			return;
		}

		giver.OpenQuestMenu(pUserEntity);
	}
}