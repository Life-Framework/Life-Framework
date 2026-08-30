// LF_QuestMenuRowComponent.c - per-row handler for the quest menu. Each row is an
// instance of QuestMenuRow.layout; this component fills the title/status/button
// widgets and routes the action button back to the menu.

class LF_QuestMenuRowComponent : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected TextWidget m_wQuestTitle;
	protected TextWidget m_wStatusText;
	protected ButtonWidget m_wActionButton;
	protected TextWidget m_wActionLabel;

	protected LF_QuestMenu m_Menu;
	protected string m_sQuestId;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wQuestTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("QuestTitle"));
		m_wStatusText = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusText"));
		m_wActionButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("ActionButton"));
		m_wActionLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("ActionButtonLabel"));
	}

	//------------------------------------------------------------------------------------------------
	void SetQuest(LF_QuestMenu menu, LF_QuestDefinition def, LF_QuestLogEntry entry)
	{
		m_Menu = menu;
		if (!def)
			return;

		m_sQuestId = def.m_sQuestId;

		if (m_wQuestTitle)
			m_wQuestTitle.SetText(WidgetManager.Translate(def.m_sTitleKey));

		int status = LF_EQuestStatus.AVAILABLE;
		if (entry)
			status = entry.m_iStatus;

		string statusText;
		string buttonLabel;

		switch (status)
		{
			case LF_EQuestStatus.AVAILABLE:
				statusText = "#LF-Quest_Status_Available";
				buttonLabel = "#LF-Quest_Accept";
				break;

			case LF_EQuestStatus.ACTIVE:
				statusText = GetProgressText(entry, def);
				buttonLabel = "#LF-Quest_InProgress";
				break;

			case LF_EQuestStatus.COMPLETED:
				statusText = "#LF-Quest_Status_Complete";
				buttonLabel = "#LF-Quest_TurnIn";
				break;

			case LF_EQuestStatus.TURNED_IN:
				statusText = "#LF-Quest_Status_TurnedIn";
				buttonLabel = "#LF-Quest_InProgress";
				break;

			case LF_EQuestStatus.FAILED:
				statusText = "#LF-Quest_Status_Failed";
				buttonLabel = "#LF-Quest_Accept";
				break;
		}

		if (m_wStatusText)
			m_wStatusText.SetText(WidgetManager.Translate(statusText));

		if (m_wActionLabel)
			m_wActionLabel.SetText(WidgetManager.Translate(buttonLabel));
	}

	//------------------------------------------------------------------------------------------------
	//! "1/3  0/2" style progress per objective.
	protected string GetProgressText(LF_QuestLogEntry entry, LF_QuestDefinition def)
	{
		string parts = "";
		for (int i = 0; i < def.GetObjectiveCount(); i++)
		{
			int progress = 0;
			if (entry && entry.m_aProgress && i < entry.m_aProgress.Count())
				progress = entry.m_aProgress[i];

			LF_QuestObjective objective = def.GetObjective(i);
			int target = 0;
			if (objective)
				target = objective.m_iTarget;

			if (!parts.IsEmpty())
				parts += "   ";
			parts += progress.ToString();
			parts += "/";
			parts += target.ToString();
		}
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_wActionButton && w == m_wActionButton)
		{
			if (m_Menu && !m_sQuestId.IsEmpty())
				m_Menu.OnActionButton(m_sQuestId);
			return true;
		}

		return super.OnClick(w, x, y, button);
	}
}