//------------------------------------------------------------------------------------------------
//! "Talk to ..." on an NPC whose whole job is its conversation (the fuel attendant, #936). The
//! scene and the server session are LF_DialogueScene and LF_Dialogue; an NPC that is also a quest
//! giver keeps LF_QuestAction instead. Local effect only: the scene is this player's UI, and every
//! server step goes through the dialogue RPCs, which the server validates.
[BaseContainerProps(configRoot: true)]
class LF_DialogueTalkAction : ScriptedUserAction
{
	//! The action shows within this distance; the server allows LF_Dialogue.REACH.
	static const float SHOW_RANGE = 3.0; // lf-fixed: follows LF_Dialogue.REACH, not a balance value

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!user || !owner)
			return false;

		return vector.Distance(user.GetOrigin(), owner.GetOrigin()) <= SHOW_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LF_DialogueComponent.ConfigOf(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!LF_DialogueScene.Begin(pOwnerEntity))
			LF_Debug.Error(LF_Dialogue.FEATURE, "talk action: the conversation did not open");
	}
}
