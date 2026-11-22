// LF_NewspaperReadAction.c: "Read" on the held newspaper. Opens the paper page on the reading client
// only. The edition is local state, so there is no RPC.

class LF_NewspaperReadAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user == LF_Utils.GetLocalCharacter();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		LF_Newspaper.Open(pOwnerEntity.GetID());
	}
}
