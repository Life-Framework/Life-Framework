// LF_ActivityPage.c: the "What to do next" tab of the personal file (LF_PlayerMenuSheet). Client only,
// UI kit parts only. One list row per activity the player can do now (name; pay an hour, distance and
// state), and under it the picked one: its pitch, where it stands, and one button: START on an
// available activity, GUIDE ME on a running one (the waypoint goes back to its step), nothing on a
// done one. Every press goes to the server (LF_ActivityController), which decides.

class LF_ActivityPage
{
	static const string PART_LIST = "PlayerMenuNextList";
	static const string PART_NAME = "PlayerMenuNextName";
	static const string PART_PITCH = "PlayerMenuNextPitch";
	static const string PART_FACTS = "PlayerMenuNextFacts";
	static const string PART_GO = "PlayerMenuNextGo";
	static const string PART_SUMMARY = "PlayerMenuNextSummary";
	static const string ROW = "PlayerMenuNext_";
	//! Five rows without scrolling (review B1).
	static const float LIST_HEIGHT = 285;

	protected static ref LF_ActivityData s_Data;
	protected static string s_sWire;
	protected static int s_iPicked;
	//! The outcome of the last press, printed under the picked row until the next answer.
	protected static string s_sOutcomeKey;

	//------------------------------------------------------------------------------------------------
	static LF_ActivityData Data()
	{
		return s_Data;
	}

	//------------------------------------------------------------------------------------------------
	static int Picked()
	{
		return s_iPicked;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the tab opened; ask for a fresh list (the last one prints meanwhile).
	static void Ask()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Warn(LF_ActivityTable.FEATURE, "activities not asked: no player controller");
			return;
		}

		controller.LF_AskActivities();
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of the tab's body (the sheet adds its header and footer).
	static void AddParts(notnull array<ref LF_UIPart> parts)
	{
		if (!s_Data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, "#LF-Activity_Waiting"));
			return;
		}

		if (s_Data.m_aRows.IsEmpty())
		{
			string empty = "#LF-Activity_None";
			if (s_Data.m_bNeedsPapers)
				empty = "#LF-Activity_NeedPapers";

			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, empty));
			return;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, "#LF-Activity_Summary"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST).Compact());
		// The picked row names the run; the detail starts at its pitch (room for five rows).
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PITCH, string.Empty));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_FACTS, string.Empty));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_GO, "#LF-Activity_Start"));
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the list rows and prints the picked activity.
	static void Fill(notnull LF_UIScreen screen)
	{
		if (!s_Data || s_Data.m_aRows.IsEmpty())
			return;

		screen.SetListHeight(PART_LIST, LIST_HEIGHT);
		foreach (int i, LF_ActivityRow row : s_Data.m_aRows)
		{
			// A row is picked by pressing its Read button (the same word the errand rows use); the one Start or Guide
			// button sits under the detail. A row with no action hides its button, and then nothing on it can be pressed.
			screen.AddListRow(PART_LIST, ROW + i.ToString(), LF_Terms.Text(row.m_sNameKey), RowStatus(row), "#LF-PlayerMenu_Read");
		}

		if (s_iPicked < 0 || s_iPicked >= s_Data.m_aRows.Count())
			s_iPicked = 0;

		ShowPicked(screen);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the press was this tab's.
	static bool OnButton(LF_UIScreen screen, string name)
	{
		if (name.StartsWith(ROW))
		{
			s_iPicked = name.Substring(ROW.Length(), name.Length() - ROW.Length()).ToInt();
			s_sOutcomeKey = string.Empty;
			if (screen)
				ShowPicked(screen);

			return true;
		}

		if (name != PART_GO)
			return false;

		LF_ActivityRow row = PickedRow();
		if (!row || row.m_eState == LF_EActivityState.DONE)
			return true;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Warn(LF_ActivityTable.FEATURE, "activity press refused: no player controller");
			return true;
		}

		if (controller.LF_AskActivityGo(row.m_sId))
			LF_Debug.Info(LF_ActivityTable.FEATURE, "asked to start or guide " + row.m_sId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer: the list, and the outcome when it answers a press.
	static void OnData(string wire, string activityId, string outcome)
	{
		LF_ActivityData data = LF_ActivityData.Decode(wire);
		if (!data)
		{
			LF_Debug.Error(LF_ActivityTable.FEATURE, "activities answer did not decode: " + wire.Length().ToString() + " chars");
			return;
		}

		bool changed = wire != s_sWire;
		s_sWire = wire;
		s_Data = data;
		if (!activityId.IsEmpty())
		{
			s_sOutcomeKey = OutcomeKey(outcome);
			foreach (int i, LF_ActivityRow row : data.m_aRows)
			{
				if (row.m_sId == activityId)
					s_iPicked = i;
			}

			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("activity %1: %2", activityId, outcome));
		}

		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("activities received: %1 rows", data.m_aRows.Count()));
		LF_PlayerMenuSheet.OnActivities(changed || !activityId.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ActivityRow PickedRow()
	{
		if (!s_Data || s_iPicked < 0 || s_iPicked >= s_Data.m_aRows.Count())
			return null;

		return s_Data.m_aRows[s_iPicked];
	}

	//------------------------------------------------------------------------------------------------
	static void ShowPicked(notnull LF_UIScreen screen)
	{
		LF_ActivityRow row = PickedRow();
		if (!row)
			return;

		screen.SetLine(PART_PITCH, LF_Terms.Text("#LF-Activity_PickedPitch", LF_Terms.Text(row.m_sNameKey), LF_Terms.Text(row.m_sPitchKey)));
		string facts = FactsLine(row);
		if (!s_sOutcomeKey.IsEmpty())
			facts = LF_Terms.Text(s_sOutcomeKey);

		screen.SetLine(PART_FACTS, facts);
		string action = ActionKey(row);
		if (action.IsEmpty())
			action = "#LF-Activity_StateDone";

		screen.SetButtonText(PART_GO, LF_Terms.Text(action));
	}

	//------------------------------------------------------------------------------------------------
	//! "$470/h · 350 m" on the row.
	static string RowStatus(notnull LF_ActivityRow row)
	{
		if (row.m_eState == LF_EActivityState.DONE)
			return LF_Terms.Text("#LF-Activity_StateDone");

		string text = PayText(row.m_iPayPerHour);
		string distance = DistanceText(row.m_iDistance);
		if (!distance.IsEmpty())
			text = LF_Terms.Text("#LF-Activity_PayDistance", text, distance);

		if (row.m_eState == LF_EActivityState.ACTIVE)
			text = LF_Terms.Text("#LF-Activity_RowStarted", text);

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The line under the pitch: the state and, while running, the step the player is on.
	static string FactsLine(notnull LF_ActivityRow row)
	{
		if (row.m_eState == LF_EActivityState.DONE)
			return LF_Terms.Text("#LF-Activity_FactsDone");

		if (row.m_eState == LF_EActivityState.ACTIVE)
			return LF_Terms.Text("#LF-Activity_FactsStep", (row.m_iStep + 1).ToString(), row.m_iSteps.ToString(), LF_Terms.Text(row.m_sStepTitleKey));

		return LF_Terms.Text("#LF-Activity_FactsSteps", row.m_iSteps.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! A pay of 0 is a row that is not a wage (founding a crew pays nothing): it says so, never a made-up
	//! figure. Below 0 the pay is not known.
	static string PayText(int perHour)
	{
		if (perHour < 0)
			return LF_Terms.Text("#LF-Activity_PayUnknown");

		if (perHour == 0)
			return LF_Terms.Text("#LF-Activity_PayNone");

		return LF_Terms.Text("#LF-Activity_PayHour", LF_MoneyFormat.FormatMoney(perHour));
	}

	//------------------------------------------------------------------------------------------------
	//! "350 m" or "1.2 km"; empty when not placed.
	static string DistanceText(int metres)
	{
		if (metres < 0)
			return string.Empty;

		if (metres < 1000)
			return LF_Terms.Text("#LF-Activity_Metres", metres.ToString());

		int tenths = Math.Round(metres / 100.0);
		return LF_Terms.Text("#LF-Activity_Kilometres", (tenths / 10).ToString() + "." + (tenths % 10).ToString());
	}

	//------------------------------------------------------------------------------------------------
	static string ActionKey(notnull LF_ActivityRow row)
	{
		if (row.m_eState == LF_EActivityState.AVAILABLE)
			return "#LF-Activity_Start";

		if (row.m_eState == LF_EActivityState.ACTIVE)
			return "#LF-Activity_Guide";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static string OutcomeKey(string outcome)
	{
		if (outcome == "started")
			return "#LF-Activity_Started";

		if (outcome == "guided")
			return "#LF-Activity_Guided";

		if (outcome == "done")
			return "#LF-Activity_FactsDone";

		return "#LF-Activity_Refused";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Data = null;
		s_sWire = string.Empty;
		s_iPicked = 0;
		s_sOutcomeKey = string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ActivityPage's statics (LF_StaticReset finds it by reflection).
class LF_ActivityPage_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ActivityPage";
	}

	override void Apply()
	{
		LF_ActivityPage.ResetStatics();
	}
}
