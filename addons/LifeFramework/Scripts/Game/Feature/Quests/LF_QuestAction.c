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

		return LF_Component<LF_QuestGiverComponent>.Find(owner) != null;
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

		LF_QuestGiverComponent giver = LF_Component<LF_QuestGiverComponent>.Find(pOwnerEntity);
		if (!giver)
			return;

		// A converse giver (the town clerk) talks in person: the server moves the quests along.
		if (giver.IsConverse())
		{
			// With a dialogue on the NPC the talk is a scene; its reply runs the same server talk
			// (LF_DialogueHook_QuestConverse). A broken dialogue falls back to the plain talk.
			if (LF_DialogueComponent.ConfigOf(pOwnerEntity) && LF_DialogueScene.Begin(pOwnerEntity))
				return;

			LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(pUserEntity);
			if (tracker)
				tracker.AskConverse(giver.GetGiverId());
			else
				LF_Debug.Error("Quests", "talk not sent: the player has no quest tracker");
			return;
		}

		giver.OpenQuestMenu(pUserEntity);
	}
}