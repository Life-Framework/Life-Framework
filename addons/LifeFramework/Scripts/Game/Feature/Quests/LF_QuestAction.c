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
		if (giver)
			giver.OpenQuestMenu(pUserEntity);
	}
}