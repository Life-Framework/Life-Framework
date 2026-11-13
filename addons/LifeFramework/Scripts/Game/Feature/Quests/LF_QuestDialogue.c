//------------------------------------------------------------------------------------------------
//! What a conversation can ask of the quests (Feature/Dialogue names these hooks in a dialogue
//! file; the class name is the registration). Server only.
//------------------------------------------------------------------------------------------------
//! Action: one talk with the NPC's quest giver, exactly as "Talk to the clerk" did it
//! (LF_QuestTrackerComponent.RequestConverse: hand in what is done, take on the next errand).
//! What the giver would have shown as slips is handed to the conversation to be spoken.
class LF_DialogueHook_QuestConverse : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		LF_QuestGiverComponent giver = LF_Component<LF_QuestGiverComponent>.Find(npc);
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		if (!giver || !tracker)
		{
			LF_Debug.Error("Quests", "dialogue talk not run: the NPC is no quest giver or the player has no quest tracker");
			return;
		}

		tracker.ConverseSpoken(giver.GetGiverId(), spoken);
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the player has handed in the quest named by the reply's param.
class LF_DialogueHook_QuestTurnedIn : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		return tracker && tracker.HasTurnedIn(param);
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the player has not yet handed in the quest named by the reply's param.
class LF_DialogueHook_QuestNotTurnedIn : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		return tracker && !tracker.HasTurnedIn(param);
	}
}
