// LF_PlayerMenuData.c: what the personal file (the F2 key, LF_PlayerMenuSheet) prints, and its wire.
// The server fills one LF_PlayerMenuData for the asking player only (LF_PlayerMenuServer) and sends it
// as one string; the client decodes it and prints it. Nothing here is trusted back: the menu is
// read only. Pure, so LOGIC tests drive the encode, the decode, the XP bar and the quest order.
//
// Wire (version 1): records split by ';', fields by '|'. Names have both characters replaced.
//   V|1
//   P|name|faction|onDuty|cash|bank|houses|garageUsed|garageCap|playtimeSeconds|wanted
//   L|licenceNameKey
//   S|job|xp|level|floor|next|nextUnlockKey|tierKey|rank|perk,perk
//                                      next = -1 at the top of the ladder; the last four fields (#4414)
//                                      are optional on read, rank 0 = not ranked this week
//   Q|id|titleKey|descKey|group|state|progress|target

//------------------------------------------------------------------------------------------------
enum LF_EPlayerMenuQuestState
{
	DONE,
	CURRENT,
	LATER
}

//------------------------------------------------------------------------------------------------
enum LF_EPlayerMenuQuestGroup
{
	//! The guided first errands (LF_QuestDefinition.m_sTargetKind), in chain order.
	ARRIVAL,
	//! Every other quest the character has taken.
	ERRANDS
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuSkill
{
	string m_sJob;
	int m_iXp;
	int m_iLevel;
	//! Total XP the current level started at.
	int m_iFloor;
	//! Total XP of the next level; -1 when there is none.
	int m_iNext;
	//! The `#` key naming what the next level opens (LF_TradeUnlocks.NextKey), empty when no row names one.
	string m_sNextUnlockKey;
	//! The `#` key of the tier word for the level (LF_PlayerMenuTrade.TierKeyFor).
	string m_sTierKey;
	//! This week's rank in the trade, 1 and up; 0 until the shift boards (W22) rank the player.
	int m_iRank;
	//! The perk ids the player picked in this trade, in pick-level order.
	ref array<string> m_aPerks = {};
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuQuest
{
	string m_sId;
	string m_sTitleKey;
	string m_sDescKey;
	LF_EPlayerMenuQuestGroup m_eGroup;
	LF_EPlayerMenuQuestState m_eState;
	int m_iProgress;
	int m_iTarget;
}

//------------------------------------------------------------------------------------------------
//! One quest as the server sees it, the input of LF_PlayerMenuQuests.Order.
class LF_PlayerMenuQuestInput
{
	string m_sId;
	string m_sPrevId;
	string m_sTitleKey;
	string m_sDescKey;
	bool m_bGuided;
	//! LF_EQuestStatus value, or -1 when the character never took it.
	int m_iStatus = -1;
	bool m_bTurnedIn;
	int m_iProgress;
	int m_iTarget;

	//------------------------------------------------------------------------------------------------
	static LF_PlayerMenuQuestInput Make(string id, string prevId, bool guided, int status, bool turnedIn)
	{
		LF_PlayerMenuQuestInput input = new LF_PlayerMenuQuestInput();
		input.m_sId = id;
		input.m_sPrevId = prevId;
		input.m_bGuided = guided;
		input.m_iStatus = status;
		input.m_bTurnedIn = turnedIn;
		return input;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuData
{
	static const int VERSION = 1;
	static const string RECORD = ";";
	static const string FIELD = "|";

	string m_sName;
	LF_Faction m_eFaction;
	bool m_bOnDuty;
	int m_iCash;
	int m_iBank;
	int m_iHouses;
	int m_iGarageUsed;
	int m_iGarageCap;
	int m_iPlaytimeSeconds;
	int m_iWanted;
	ref array<string> m_aLicenceKeys = {};
	ref array<ref LF_PlayerMenuSkill> m_aSkills = {};
	ref array<ref LF_PlayerMenuQuest> m_aQuests = {};

	//------------------------------------------------------------------------------------------------
	//! A name or key with the wire's separators taken out.
	static string Clean(string value)
	{
		string clean = value;
		clean.Replace(RECORD, " ");
		clean.Replace(FIELD, " ");
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	string Encode()
	{
		string wire = "V" + FIELD + VERSION.ToString();
		wire += RECORD + "P" + FIELD + Clean(m_sName) + FIELD + m_eFaction.ToString() + FIELD + BoolText(m_bOnDuty);
		wire += FIELD + m_iCash.ToString() + FIELD + m_iBank.ToString() + FIELD + m_iHouses.ToString();
		wire += FIELD + m_iGarageUsed.ToString() + FIELD + m_iGarageCap.ToString();
		wire += FIELD + m_iPlaytimeSeconds.ToString() + FIELD + m_iWanted.ToString();

		foreach (string key : m_aLicenceKeys)
		{
			wire += RECORD + "L" + FIELD + Clean(key);
		}

		foreach (LF_PlayerMenuSkill skill : m_aSkills)
		{
			wire += RECORD + "S" + FIELD + Clean(skill.m_sJob) + FIELD + skill.m_iXp.ToString() + FIELD + skill.m_iLevel.ToString();
			wire += FIELD + skill.m_iFloor.ToString() + FIELD + skill.m_iNext.ToString();
			wire += FIELD + Clean(skill.m_sNextUnlockKey) + FIELD + Clean(skill.m_sTierKey) + FIELD + skill.m_iRank.ToString();
			wire += FIELD + PerkList(skill.m_aPerks);
		}

		foreach (LF_PlayerMenuQuest quest : m_aQuests)
		{
			wire += RECORD + "Q" + FIELD + Clean(quest.m_sId) + FIELD + Clean(quest.m_sTitleKey) + FIELD + Clean(quest.m_sDescKey);
			wire += FIELD + quest.m_eGroup.ToString() + FIELD + quest.m_eState.ToString();
			wire += FIELD + quest.m_iProgress.ToString() + FIELD + quest.m_iTarget.ToString();
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	//! Perk ids joined with ',' (an id with a separator in it has it taken out).
	protected static string PerkList(notnull array<string> perks)
	{
		string list = string.Empty;
		foreach (string perk : perks)
		{
			string clean = Clean(perk);
			clean.Replace(",", " ");
			if (!list.IsEmpty())
				list += ",";

			list += clean;
		}

		return list;
	}

	//------------------------------------------------------------------------------------------------
	protected static string BoolText(bool value)
	{
		if (value)
			return "1";

		return "0";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The data, or null for a wire of another version or with no person record.
	static LF_PlayerMenuData Decode(string wire)
	{
		array<string> records = {};
		wire.Split(RECORD, records, false);
		if (records.Count() < 2 || records[0] != "V" + FIELD + VERSION.ToString())
			return null;

		LF_PlayerMenuData data = new LF_PlayerMenuData();
		bool person = false;
		foreach (string record : records)
		{
			array<string> f = {};
			record.Split(FIELD, f, false);
			if (f.IsEmpty())
				continue;

			if (f[0] == "P" && f.Count() >= 11)
			{
				data.m_sName = f[1];
				data.m_eFaction = f[2].ToInt();
				data.m_bOnDuty = f[3] == "1";
				data.m_iCash = f[4].ToInt();
				data.m_iBank = f[5].ToInt();
				data.m_iHouses = f[6].ToInt();
				data.m_iGarageUsed = f[7].ToInt();
				data.m_iGarageCap = f[8].ToInt();
				data.m_iPlaytimeSeconds = f[9].ToInt();
				data.m_iWanted = f[10].ToInt();
				person = true;
			}
			else if (f[0] == "L" && f.Count() >= 2)
			{
				data.m_aLicenceKeys.Insert(f[1]);
			}
			else if (f[0] == "S" && f.Count() >= 6)
			{
				LF_PlayerMenuSkill skill = new LF_PlayerMenuSkill();
				skill.m_sJob = f[1];
				skill.m_iXp = f[2].ToInt();
				skill.m_iLevel = f[3].ToInt();
				skill.m_iFloor = f[4].ToInt();
				skill.m_iNext = f[5].ToInt();
				// The trade card fields trail the row; a row without them (an older wire) leaves them empty.
				if (f.Count() >= 9)
				{
					skill.m_sNextUnlockKey = f[6];
					skill.m_sTierKey = f[7];
					skill.m_iRank = f[8].ToInt();
				}

				if (f.Count() >= 10 && !f[9].IsEmpty())
					f[9].Split(",", skill.m_aPerks, true);

				data.m_aSkills.Insert(skill);
			}
			else if (f[0] == "Q" && f.Count() >= 8)
			{
				LF_PlayerMenuQuest quest = new LF_PlayerMenuQuest();
				quest.m_sId = f[1];
				quest.m_sTitleKey = f[2];
				quest.m_sDescKey = f[3];
				quest.m_eGroup = f[4].ToInt();
				quest.m_eState = f[5].ToInt();
				quest.m_iProgress = f[6].ToInt();
				quest.m_iTarget = f[7].ToInt();
				data.m_aQuests.Insert(quest);
			}
		}

		if (!person)
			return null;

		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character has a guided arrival chain and every step of it is turned in. The welcome
	//! guide stops sending such a player to the clerk.
	bool ArrivalFinished()
	{
		int done;
		int total = CountGroup(LF_EPlayerMenuQuestGroup.ARRIVAL, done);
		return total > 0 && done == total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many quests of a group there are, and how many of them are done.
	int CountGroup(LF_EPlayerMenuQuestGroup group, out int done)
	{
		done = 0;
		int total = 0;
		foreach (LF_PlayerMenuQuest quest : m_aQuests)
		{
			if (quest.m_eGroup != group)
				continue;

			total++;
			if (quest.m_eState == LF_EPlayerMenuQuestState.DONE)
				done++;
		}

		return total;
	}
}

//------------------------------------------------------------------------------------------------
//! The XP bar of one skill: where the level starts, where the next starts, how full the bar is.
class LF_PlayerMenuXp
{
	//------------------------------------------------------------------------------------------------
	//! \param thresholds Total XP for level 2, 3, ... (LF_JobTrackDef.m_aLevelThresholds, any order).
	//! \param[out] level 1 plus the thresholds xp reached (the same count as LF_JobTrackDef.LevelFor).
	//! \param[out] floor The highest threshold reached, 0 below the first.
	//! \param[out] next The lowest threshold not reached, -1 when every one is.
	static void Span(array<int> thresholds, int xp, out int level, out int floor, out int next)
	{
		level = 1;
		floor = 0;
		next = -1;
		if (!thresholds)
			return;

		foreach (int threshold : thresholds)
		{
			if (xp >= threshold)
			{
				level++;
				if (threshold > floor)
					floor = threshold;
			}
			else if (next < 0 || threshold < next)
			{
				next = threshold;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return How full the bar is, 0 to 1: the share of the way from floor to next. Full at the top.
	static float Fraction(int xp, int floor, int next)
	{
		if (next < 0)
			return 1;

		int span = next - floor;
		if (span <= 0)
			return 1;

		int into = xp - floor;
		if (into <= 0)
			return 0;

		float spanFloat = span;
		float fraction = into / spanFloat; // lf-float: a bar width, not an int that must stay exact
		if (fraction > 1)
			return 1;

		return fraction;
	}
}

//------------------------------------------------------------------------------------------------
//! The quest list of the Progress tab: the guided chain first in its own order, then the rest.
class LF_PlayerMenuQuests
{
	//------------------------------------------------------------------------------------------------
	//! \param inputs Every quest definition with this character's state (any order: map order).
	//! \return The rows to print. The guided chain is ordered by each step's previous quest (ties by
	//! id) and its first step not done is CURRENT. Other quests appear only once taken.
	static array<ref LF_PlayerMenuQuest> Order(notnull array<ref LF_PlayerMenuQuestInput> inputs)
	{
		array<ref LF_PlayerMenuQuest> rows = {};

		array<string> guidedIds = {};
		array<string> otherIds = {};
		map<string, LF_PlayerMenuQuestInput> byId = new map<string, LF_PlayerMenuQuestInput>();
		foreach (LF_PlayerMenuQuestInput input : inputs)
		{
			if (!input || input.m_sId.IsEmpty() || byId.Contains(input.m_sId))
				continue;

			byId.Set(input.m_sId, input);
			if (input.m_bGuided)
				guidedIds.Insert(input.m_sId);
			else
				otherIds.Insert(input.m_sId);
		}

		guidedIds.Sort();
		otherIds.Sort();

		// Chain order: a step goes once its previous step is placed (or is not a guided step).
		array<string> placed = {};
		bool progressed = true;
		while (progressed && placed.Count() < guidedIds.Count())
		{
			progressed = false;
			foreach (string id : guidedIds)
			{
				if (placed.Find(id) != -1)
					continue;

				string prev = byId.Get(id).m_sPrevId;
				if (prev.IsEmpty() || guidedIds.Find(prev) == -1 || placed.Find(prev) != -1)
				{
					placed.Insert(id);
					progressed = true;
				}
			}
		}

		// A loop in the chain (a config error) still lists its steps, after the rest.
		foreach (string loopId : guidedIds)
		{
			if (placed.Find(loopId) == -1)
				placed.Insert(loopId);
		}

		bool currentGiven = false;
		foreach (string stepId : placed)
		{
			LF_PlayerMenuQuestInput step = byId.Get(stepId);
			LF_EPlayerMenuQuestState state = StateOf(step);
			if (state == LF_EPlayerMenuQuestState.LATER && !currentGiven && IsReady(step, byId))
				state = LF_EPlayerMenuQuestState.CURRENT;

			if (state == LF_EPlayerMenuQuestState.CURRENT)
				currentGiven = true;

			rows.Insert(Row(step, LF_EPlayerMenuQuestGroup.ARRIVAL, state));
		}

		foreach (string otherId : otherIds)
		{
			LF_PlayerMenuQuestInput other = byId.Get(otherId);
			if (other.m_iStatus < 0 && !other.m_bTurnedIn)
				continue;

			rows.Insert(Row(other, LF_EPlayerMenuQuestGroup.ERRANDS, StateOf(other)));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EPlayerMenuQuestState StateOf(LF_PlayerMenuQuestInput input)
	{
		if (input.m_bTurnedIn || input.m_iStatus == LF_EQuestStatus.TURNED_IN)
			return LF_EPlayerMenuQuestState.DONE;

		if (input.m_iStatus == LF_EQuestStatus.ACTIVE || input.m_iStatus == LF_EQuestStatus.COMPLETED)
			return LF_EPlayerMenuQuestState.CURRENT;

		return LF_EPlayerMenuQuestState.LATER;
	}

	//------------------------------------------------------------------------------------------------
	//! The step the arrival guide is on (LF_ArrivalGuide.CurrentStep): its previous step is done.
	protected static bool IsReady(LF_PlayerMenuQuestInput step, map<string, LF_PlayerMenuQuestInput> byId)
	{
		if (step.m_sPrevId.IsEmpty())
			return true;

		LF_PlayerMenuQuestInput prev = byId.Get(step.m_sPrevId);
		if (!prev)
			return true;

		return StateOf(prev) == LF_EPlayerMenuQuestState.DONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlayerMenuQuest Row(LF_PlayerMenuQuestInput input, LF_EPlayerMenuQuestGroup group, LF_EPlayerMenuQuestState state)
	{
		LF_PlayerMenuQuest row = new LF_PlayerMenuQuest();
		row.m_sId = input.m_sId;
		row.m_sTitleKey = input.m_sTitleKey;
		row.m_sDescKey = input.m_sDescKey;
		row.m_eGroup = group;
		row.m_eState = state;
		row.m_iProgress = input.m_iProgress;
		row.m_iTarget = input.m_iTarget;
		if (state == LF_EPlayerMenuQuestState.DONE && row.m_iTarget > 0)
			row.m_iProgress = row.m_iTarget;

		return row;
	}
}
