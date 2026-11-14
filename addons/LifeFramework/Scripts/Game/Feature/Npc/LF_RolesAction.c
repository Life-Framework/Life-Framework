// LF_RolesAction.c: the "Talk" action on an NPC that has a role conversation.

class LF_RolesAction : ScriptedUserAction
{
	protected static const float REACH = 3.0; // lf-fixed: follows the dialogue's own reach (LF_Dialogue.REACH)

	//------------------------------------------------------------------------------------------------
	//! A role conversation exists, the NPC has no authored dialogue of its own (the clerk talks
	//! through its quest action), and it is not stunned.
	protected bool HasRoleTalk(IEntity npc)
	{
		if (!npc || LF_NpcStun.BlocksTalk(npc))
			return false;

		if (LF_Component<LF_DialogueComponent>.Find(npc))
			return false;

		return LF_Roles.DialogueOf(npc) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!user || !owner)
			return false;

		if (vector.Distance(user.GetOrigin(), owner.GetOrigin()) > REACH)
			return false;

		return HasRoleTalk(owner);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return user != null && HasRoleTalk(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-NpcTalk_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The scene is local UI: run it on the acting client only. The server validates every reply.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_Roles.FEATURE, "talk action performed with no owner or user");
			return;
		}

		if (!LF_DialogueScene.Begin(pOwnerEntity))
			LF_Debug.Warn(LF_Roles.FEATURE, "talk did not open: the conversation scene refused");
	}
}
