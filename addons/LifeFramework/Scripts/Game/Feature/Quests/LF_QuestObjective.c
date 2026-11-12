// LF_QuestObjective.c - a single quest objective. Data-driven for v1: the type,
// filter and progress mode fields cover every primitive (kill, gather, process,
// collect/deliver, talk, area). Typed subclasses can be added later when a
// primitive needs more config than a string filter.

[BaseContainerProps()]
class LF_QuestObjective
{
	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Objective type", "", ParamEnumArray.FromEnum(LF_EQuestEventType))]
	LF_EQuestEventType m_eType;

	[Attribute(defvalue: "1", UIWidgets.EditBox, "Target count")]
	int m_iTarget;

	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Progress mode", "", ParamEnumArray.FromEnum(LF_EQuestProgressMode))]
	LF_EQuestProgressMode m_eMode;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Filter key (faction, resource, area id, giver id; empty = any)")]
	string m_sFilter;

	//! AREA_ENTER only: world-space center and radius the manager checks against
	//! the player position.
	[Attribute(defvalue: "0 0 0", UIWidgets.EditBox, "Area center (AREA_ENTER only)")]
	vector m_vPosition;

	[Attribute(defvalue: "0", UIWidgets.EditBox, "Area radius in meters (AREA_ENTER only)")]
	float m_fRadius;

	//------------------------------------------------------------------------------------------------
	//! Whether this objective is satisfied at the given progress.
	bool IsComplete(int progress)
	{
		return progress >= m_iTarget;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a world event should be considered by this objective at all.
	bool Matches(LF_QuestEvent questEvent)
	{
		if (!questEvent)
			return false;
		if (questEvent.m_eType != m_eType)
			return false;
		if (!m_sFilter.IsEmpty() && !FilterMatches(questEvent.m_sFilter))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A GATHER or PROCESS filter names a prefab and matches by GUID (LF_Utils.SamePrefab), so it
	//! survives a resource move. Other filters (an NPC, an area, a signal) are plain ids.
	protected bool FilterMatches(string filter)
	{
		if (m_eType == LF_EQuestEventType.GATHER || m_eType == LF_EQuestEventType.PROCESS)
			return LF_Utils.SamePrefab(filter, m_sFilter);

		return filter == m_sFilter;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies a matching event to the current progress and returns the new progress.
	//! ADD modes add the event value; SET modes replace progress with the reported value
	//! (a collect objective reflects what the player currently holds).
	int ApplyEvent(int progress, LF_QuestEvent questEvent)
	{
		if (!Matches(questEvent))
			return progress;

		if (m_eMode == LF_EQuestProgressMode.SET)
		{
			if (questEvent.m_iValue < 0)
				return progress;
			return questEvent.m_iValue;
		}

		if (questEvent.m_iValue <= 0)
			return progress;
		return progress + questEvent.m_iValue;
	}
}