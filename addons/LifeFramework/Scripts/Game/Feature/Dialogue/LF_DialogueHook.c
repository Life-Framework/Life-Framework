//------------------------------------------------------------------------------------------------
//! What a reply can ask of another feature. A dialogue file names a hook ("QuestConverse") and the
//! server runs class LF_DialogueHook_QuestConverse, which lives in the feature that owns the work
//! (Feature/Quests/LF_QuestDialogue.c). There is no list to append to: the name is the class.
//! Hooks run on the server only and hold no state.
class LF_DialogueHook : Managed
{
	static const string CLASS_PREFIX = "LF_DialogueHook_";

	//------------------------------------------------------------------------------------------------
	//! As a condition: whether the reply is offered to this player.
	bool Allows(IEntity player, IEntity npc, string param)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! As an action: do the work, having checked everything a client could lie about. Keys put in
	//! `spoken` are what the NPC says in answer (a dialogue line voiced for that key, else a caption).
	void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! The hook for a name, or null (logged): a dialogue file that names a hook nobody wrote.
	static LF_DialogueHook Find(string name)
	{
		if (name.IsEmpty())
			return null;

		typename type = (CLASS_PREFIX + name).ToType();
		if (!type || !type.IsInherited(LF_DialogueHook))
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("dialogue hook '%1' has no class %2%1", name, CLASS_PREFIX));
			return null;
		}

		return LF_DialogueHook.Cast(type.Spawn());
	}

	//------------------------------------------------------------------------------------------------
	//! Verdict of an option's condition. No condition offers it; an unknown hook hides it.
	static bool Offers(LF_DialogueOption option, IEntity player, IEntity npc)
	{
		if (!option)
			return false;

		if (option.m_sCondition.IsEmpty())
			return true;

		LF_DialogueHook hook = Find(option.m_sCondition);
		if (!hook)
			return false;

		return hook.Allows(player, npc, option.m_sParam);
	}
}
