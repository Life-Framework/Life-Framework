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

//------------------------------------------------------------------------------------------------
//! Condition: the reply's quest is not handed in yet and the player holds every item its collect
//! objectives ask for. A reply "I found this for you" is offered only to a player who has it (#3793).
class LF_DialogueHook_QuestItemsHeld : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!tracker || !manager || tracker.HasTurnedIn(param))
			return false;

		return HoldsAll(manager, player, manager.GetDefinition(param));
	}

	//------------------------------------------------------------------------------------------------
	//! True when the quest has at least one collect objective and the player holds each one's target.
	static bool HoldsAll(notnull LF_QuestManager manager, IEntity player, LF_QuestDefinition def)
	{
		if (!def)
			return false;

		bool collects;
		for (int i = 0; i < def.GetObjectiveCount(); i++)
		{
			LF_QuestObjective objective = def.GetObjective(i);
			if (!objective || objective.m_eType != LF_EQuestEventType.COLLECT_REPORT)
				continue;

			collects = true;
			if (manager.CountItem(player, objective.m_sFilter) < objective.m_iTarget)
				return false;
		}

		return collects;
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the player handed in one of the quests lately (#3441). The param is `id[,id]:days`: any
//! of the listed quests turned in within that many game days. Talk of the town fades this way; a
//! restart or a respawn counts the days from the restore (the turn-in time is not saved).
class LF_DialogueHook_QuestTurnedInRecent : LF_DialogueHook
{
	static const float GAME_DAY_SECONDS = 7200;	// lf-fixed: one in-game day at the 2 hour day (LF_OrgWarConfig, M4)

	//------------------------------------------------------------------------------------------------
	//! Splits `id[,id]:days`. \return false when the param names no quest or the days are not a positive number.
	static bool ParseParam(string param, notnull array<string> questIds, out float days)
	{
		questIds.Clear();
		days = 0;
		array<string> halves = {};
		param.Split(":", halves, true);
		if (halves.Count() != 2)
			return false;

		halves[0].Split(",", questIds, true);
		days = halves[1].ToFloat();
		return questIds.Count() > 0 && days > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] turnedInAt World second of the turn-in, -1 when not turned in.
	static bool IsRecent(float turnedInAt, float now, float days)
	{
		if (turnedInAt < 0)
			return false;

		return now - turnedInAt < days * GAME_DAY_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		array<string> questIds = {};
		float days;
		if (!ParseParam(param, questIds, days))
		{
			LF_Debug.Error("Quests", "dialogue condition QuestTurnedInRecent has a bad param '" + param + "' (want id[,id]:days)");
			return false;
		}

		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		if (!tracker)
			return false;

		float now = GetGame().GetWorld().GetWorldTime() / 1000.0;
		foreach (string questId : questIds)
		{
			if (IsRecent(tracker.GetTurnedInAt(questId), now, days))
				return true;
		}

		return false;
	}
}
