//------------------------------------------------------------------------------------------------
//! "Talk to ..." on an NPC whose whole job is its conversation (the fuel attendant, #936). The
//! scene and the server session are LF_DialogueScene and LF_Dialogue; an NPC that is also a quest
//! giver keeps LF_QuestAction instead. Local effect only: the scene is this player's UI, and every
//! server step goes through the dialogue RPCs, which the server validates.
[BaseContainerProps(configRoot: true)]
class LF_DialogueTalkAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.CheckBox, "A counter clerk: the prompt shows only on the customer side, in front of the clerk, never from behind the counter")]
	protected bool m_bCustomerSideOnly;

	//! The action shows within this distance; the server allows LF_Dialogue.REACH.
	static const float SHOW_RANGE = 3.0; // lf-fixed: follows LF_Dialogue.REACH, not a balance value
	//! The talk sphere on a counter clerk reaches across the counter (about 2.4 m), so a prompt must
	//! not show through a ceiling or floor: the customer and the NPC stand on the same level.
	static const float SAME_LEVEL = 1.2; // lf-fixed: a storey is about 2.8 m; steps and raised counters stay under 1.2

	//------------------------------------------------------------------------------------------------
	static bool OnSameLevel(IEntity a, IEntity b)
	{
		if (!a || !b)
			return false;

		return Math.AbsFloat(a.GetOrigin()[1] - b.GetOrigin()[1]) <= SAME_LEVEL;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!user || !owner)
			return false;

		if (!OnSameLevel(user, owner))
			return false;

		if (m_bCustomerSideOnly && !LF_CounterAccess.OnCustomerSide(user, owner))
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
