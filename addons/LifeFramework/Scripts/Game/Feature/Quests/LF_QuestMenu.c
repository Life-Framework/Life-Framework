// LF_QuestMenu.c - quest menu opened from a quest giver. Shows every quest the
// giver offers with a status row and a single action button (Accept or Turn In);
// the server decides what is actually valid and replies with the fresh log.

class LF_QuestMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected Widget m_wQuestList;
	protected TextWidget m_wGiverTitle;

	protected LF_QuestGiverComponent m_Giver;
	protected IEntity m_PlayerEntity;
	protected LF_QuestTrackerComponent m_TrackerComponent;
	protected bool m_bSubscribedToTracker;

	protected ref array<ref LF_QuestLogEntry> m_aLog;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		m_wQuestList = m_wRoot.FindAnyWidget("QuestList");
		m_wGiverTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("GiverTitle"));

		BindTracker();

		// The open handshake reports the TALK objective, recounts collect
		// objectives and pulls the fresh log from the server.
		if (m_TrackerComponent && m_Giver)
			m_TrackerComponent.AskOpenGiver(m_Giver.GetGiverId());
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (m_TrackerComponent && m_TrackerComponent.GetOnLogReceived())
			m_TrackerComponent.GetOnLogReceived().Remove(OnLogReceived);

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	void SetGiver(LF_QuestGiverComponent giver, IEntity playerEntity)
	{
		m_Giver = giver;
		m_PlayerEntity = playerEntity;

		if (m_wGiverTitle && giver)
			m_wGiverTitle.SetText(giver.GetGiverId());

		// OpenMenu may call OnMenuOpen before this setter, so complete the
		// client-side binding here as well as in OnMenuOpen.
		BindTracker();
		if (m_TrackerComponent && m_Giver)
		{
			m_TrackerComponent.AskOpenGiver(m_Giver.GetGiverId());
			OnLogReceived(m_TrackerComponent.GetClientLogCache());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BindTracker()
	{
		if (!m_PlayerEntity || m_bSubscribedToTracker)
			return;
		m_TrackerComponent = LF_QuestTrackerComponent.Cast(m_PlayerEntity.FindComponent(LF_QuestTrackerComponent));
		if (m_TrackerComponent)
		{
			m_TrackerComponent.GetOnLogReceived().Insert(OnLogReceived);
			m_bSubscribedToTracker = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fresh log from the server: rebuild every row.
	void OnLogReceived(string serialized)
	{
		LF_QuestLogFormat.Decode(serialized, m_aLog);
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void OnActionButton(string questId)
	{
		if (!m_TrackerComponent)
			return;

		LF_QuestLogEntry entry = FindLogEntry(questId);
		int status = LF_EQuestStatus.AVAILABLE;
		if (entry)
			status = entry.m_iStatus;
		if (status == LF_EQuestStatus.COMPLETED)
			m_TrackerComponent.AskTurnIn(questId);
		else
			m_TrackerComponent.AskAccept(questId);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		if (!m_wQuestList || !m_Giver)
			return;

		ClearRows();

		foreach (LF_QuestDefinition def : m_Giver.GetQuests())
		{
			if (!def)
				continue;

			Widget rowWidget = GetGame().GetWorkspace().CreateWidgets("{CE14CB47B66299DC}UI/Layouts/QuestMenuRow.layout", m_wQuestList);
			if (!rowWidget)
			{
				EL_Debug.Error("Quests", string.Format("quest row layout failed to create for %1", def.m_sQuestId));
				continue;
			}

			LF_QuestMenuRowComponent row = LF_QuestMenuRowComponent.Cast(rowWidget.FindHandler(LF_QuestMenuRowComponent));
			if (row)
				row.SetQuest(this, def, FindLogEntry(def.m_sQuestId));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestLogEntry FindLogEntry(string questId)
	{
		if (!m_aLog)
			return null;

		foreach (LF_QuestLogEntry entry : m_aLog)
		{
			if (entry.m_sQuestId == questId)
				return entry;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRows()
	{
		if (!m_wQuestList)
			return;

		while (m_wQuestList.GetChildren())
			m_wQuestList.GetChildren().RemoveFromHierarchy();
	}
}
