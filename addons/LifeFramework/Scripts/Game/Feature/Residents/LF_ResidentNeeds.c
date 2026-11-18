// LF_ResidentNeeds.c - a resident asks for help (#2862, epic #1012 beat 2). The need is a fixed quest
// this wave: a resident whose definition names `m_sNeedQuest` spawns as a quest giver offering that
// quest from Configs/Quests/LF_ResidentNeedQuests.conf, and turning it in is remembered as `helped`
// (LF_ResidentMemory). A resident who refuses the player offers nothing, in words. Pay and reward stay
// in the quest definition; this file moves no money. Needs driven by the live economy are the next wave.

//------------------------------------------------------------------------------------------------
//! The need quests a server offers. Loaded from Configs/Quests/LF_ResidentNeedQuests.conf.
[BaseContainerProps(configRoot: true)]
class LF_ResidentNeedQuestsConfig
{
	static const ResourceName CONFIG_PATH = "{5B6D1F3A9C2E7048}Configs/Quests/LF_ResidentNeedQuests.conf";

	[Attribute("", UIWidgets.Object, "Quest definitions residents ask for, by quest id")]
	ref array<ref LF_QuestDefinition> m_aQuests;

	//------------------------------------------------------------------------------------------------
	LF_QuestDefinition Find(string questId)
	{
		if (!m_aQuests || questId.IsEmpty())
			return null;

		foreach (LF_QuestDefinition def : m_aQuests)
		{
			if (def && def.m_sQuestId == questId)
				return def;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ResidentNeeds
{
	static const string FEATURE = "Residents";
	static const string REASON_HELPED = "helped";
	//! The neutral civilian with a quest giver on it: what a resident who asks for help spawns as.
	static const ResourceName GIVER_PREFAB = "{7A3C95E1B04D2F68}Prefabs/Characters/Residents/Resident_Needy.et";

	protected static ref LF_ResidentNeedQuestsConfig s_Config;
	protected static bool s_bLoadAttempted;
	//! Quest id -> the resident who asked for it.
	protected static ref map<string, string> s_mAskedBy;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentNeedQuestsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_ResidentNeedQuestsConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("resident need quests config does not resolve: %1", LF_ResidentNeedQuestsConfig.CONFIG_PATH));
			return null;
		}

		s_Config = LF_ResidentNeedQuestsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("resident need quests config has an unexpected root class: %1", LF_ResidentNeedQuestsConfig.CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until ResetStatics.
	static void UseForTest(LF_ResidentNeedQuestsConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: makes the spawned resident `entity` a quest giver offering the need quest of `def`.
	//! A resident with no need is left alone. An unknown quest id, or a character with no quest giver,
	//! is logged and the resident stands with no offer.
	//! \return True when the resident now offers their quest.
	static bool Attach(IEntity entity, LF_ResidentDef def)
	{
		if (!entity || !def || def.m_sNeedQuest.IsEmpty())
			return false;

		LF_ResidentNeedQuestsConfig config = Get();
		if (!config)
			return false;

		LF_QuestDefinition quest = config.Find(def.m_sNeedQuest);
		if (!quest)
		{
			LF_Debug.Error(FEATURE, string.Format("resident '%1' asks for quest '%2' which the need quests config does not know; no offer", def.m_sId, def.m_sNeedQuest));
			return false;
		}

		LF_QuestGiverComponent giver = LF_QuestGiverComponent.Cast(entity.FindComponent(LF_QuestGiverComponent));
		if (!giver)
		{
			LF_Debug.Error(FEATURE, string.Format("resident '%1' has no quest giver on its prefab; the need '%2' is not offered", def.m_sId, def.m_sNeedQuest));
			return false;
		}

		giver.m_sGiverId = "resident_" + def.m_sId;
		giver.m_aQuests = {};
		giver.m_aQuests.Insert(quest);
		LF_QuestManager.GetInstance().RegisterGiver(giver);

		if (!s_mAskedBy)
			s_mAskedBy = new map<string, string>();

		s_mAskedBy.Set(quest.m_sQuestId, def.m_sId);
		LF_Debug.Info(FEATURE, string.Format("resident '%1' asks for help: quest '%2'", def.m_sId, quest.m_sQuestId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The resident who asks for `questId`, or empty when it is not a resident's need.
	static string ResidentOf(string questId)
	{
		if (!s_mAskedBy)
			return string.Empty;

		return s_mAskedBy.Get(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the resident behind `questId` refuses the character `uid` (their memory tier is refuses).
	static bool Refuses(string questId, string uid)
	{
		string resident = ResidentOf(questId);
		if (resident.IsEmpty() || uid.IsEmpty())
			return false;

		return LF_ResidentMemory.Tier(resident, uid) == LF_ResidentMemory.TIER_REFUSES;
	}

	//------------------------------------------------------------------------------------------------
	//! The quest `questId` was turned in by `uid`: the resident who asked remembers the help.
	//! \return True when a resident remembered it.
	static bool OnTurnedIn(string questId, string uid)
	{
		string resident = ResidentOf(questId);
		if (resident.IsEmpty())
			return false;

		int now;
		if (GetGame() && GetGame().GetWorld())
			now = GetGame().GetWorld().GetWorldTime() / 1000;

		return LF_ResidentMemory.Apply(resident, uid, REASON_HELPED, now);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_mAskedBy = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentNeeds' statics (LF_StaticReset finds it by reflection).
class LF_ResidentNeeds_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentNeeds";
	}

	override void Apply()
	{
		LF_ResidentNeeds.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The two seams on the quest tracker: a resident who refuses the player gives no errand, and a
//! turned-in need is remembered as help. Quests no resident asked for pass straight through.
modded class LF_QuestTrackerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool AcceptQuest(string questId, bool chained = false, bool quiet = false)
	{
		if (Replication.IsServer() && LF_ResidentNeeds.Refuses(questId, OwnerUid()))
		{
			LF_Debug.Info("Residents", string.Format("%1 refuses quest '%2' to %3", LF_ResidentNeeds.ResidentOf(questId), questId, OwnerUid()));
			Notify("#LF-ResidentNeed_Refused", "#LF-Quest_Title");
			return false;
		}

		return super.AcceptQuest(questId, chained, quiet);
	}

	//------------------------------------------------------------------------------------------------
	override bool TurnInQuest(string questId)
	{
		bool turnedIn = super.TurnInQuest(questId);
		if (turnedIn)
			LF_ResidentNeeds.OnTurnedIn(questId, OwnerUid());

		return turnedIn;
	}
}
