//------------------------------------------------------------------------------------------------
//! Police applications panel (#1288): the client half of LF_PoliceApplicationGm. A UI kit sheet,
//! opened with /applications (made for Game Master, where staff watch the server). It lists the open
//! applications; Read shows one, Approve or Reject sends that one to the server.
//!
//! It holds no application data of its own: rows come only from the server's answer, which a
//! non-staff player receives empty with the refusal line. The sheet sends a verb and an account uid,
//! nothing else, and the server checks both.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplicationGmPanel
{
	static const string PART_STATUS = "PoliceAppGmStatus";
	static const string PART_LIST = "PoliceAppGmList";
	static const string PART_DETAIL = "PoliceAppGmDetail";
	static const string PART_APPROVE = "PoliceAppGmApprove";
	static const string PART_REJECT = "PoliceAppGmReject";
	static const string PART_CLOSE = "PoliceAppGmClose";
	static const string ROW_PREFIX = "PoliceAppGmRow";

	protected static ref LF_PoliceApplicationGmPanel s_Panel;

	protected ref LF_UIScreen m_Screen;
	protected ref array<string> m_aUids = {};
	protected ref array<string> m_aNames = {};
	protected ref array<string> m_aRows = {};
	protected string m_sSelectedUid;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts()
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "PoliceAppGmTitle", LF_Terms.Text("#LF-PoliceAppGm_Title")));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, "#LF-PoliceAppGm_Waiting"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "PoliceAppGmRule", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL, "#LF-PoliceAppGm_PickFirst"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_APPROVE, "#LF-PoliceAppGm_Approve"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_REJECT, "#LF-PoliceAppGm_Reject"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-PoliceAppGm_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: open the sheet (or bring the open one up to date) and ask the server for the list.
	static void Open()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_PoliceApplicationGm.FEATURE, "panel not opened: no local player controller");
			return;
		}

		if (!s_Panel || !s_Panel.m_Screen || !s_Panel.m_Screen.IsOpen())
		{
			LF_UIScreen screen = LF_UIScreen.Open(BuildParts());
			if (!screen)
			{
				LF_Debug.Error(LF_PoliceApplicationGm.FEATURE, "panel not opened: the UI kit refused the sheet");
				return;
			}

			s_Panel = new LF_PoliceApplicationGmPanel();
			s_Panel.m_Screen = screen;
			screen.GetOnButton().Insert(s_Panel.OnButton);
			screen.CloseOnDeath();
			LF_Debug.Info(LF_PoliceApplicationGm.FEATURE, "panel open");
		}

		controller.LF_AskPoliceAppGmList();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer. Ignored when the sheet is closed.
	static void OnAnswer(array<string> rows, string result, string resultArg)
	{
		if (!s_Panel || !s_Panel.m_Screen || !s_Panel.m_Screen.IsOpen())
		{
			LF_Debug.Info(LF_PoliceApplicationGm.FEATURE, "answer arrived with the panel closed");
			return;
		}

		s_Panel.Show(rows, result, resultArg);
	}

	//------------------------------------------------------------------------------------------------
	//! The open panel's sheet, for the session proof; null when closed.
	static LF_UIScreen Current()
	{
		if (!s_Panel)
			return null;

		return s_Panel.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void Show(array<string> rows, string result, string resultArg)
	{
		m_aUids.Clear();
		m_aNames.Clear();
		m_aRows.Clear();
		m_Screen.ClearList(PART_LIST);

		foreach (string row : rows)
		{
			array<string> p = {};
			row.Split(LF_PoliceApplicationGm.SEP, p, false);
			if (p.Count() < 5)
				continue;

			int i = m_aUids.Count();
			m_aUids.Insert(p[0]);
			m_aNames.Insert(p[1]);
			m_aRows.Insert(row);
			string waited = WidgetManager.Translate("#LF-PoliceAppGm_Waited", (p[4].ToInt() / 3600).ToString());
			m_Screen.AddListRow(PART_LIST, ROW_PREFIX + i.ToString(), p[1], waited, "#LF-PoliceAppGm_Read");
		}

		string status;
		if (!result.IsEmpty())
			status = WidgetManager.Translate(result, resultArg);
		else if (m_aUids.IsEmpty())
			status = "#LF-PoliceApp_NoneOpen";
		else
			status = WidgetManager.Translate("#LF-PoliceAppGm_Count", m_aUids.Count().ToString());

		m_Screen.SetLine(PART_STATUS, status);

		if (m_aUids.Find(m_sSelectedUid) < 0)
		{
			m_sSelectedUid = "";
			m_Screen.SetLine(PART_DETAIL, "#LF-PoliceAppGm_PickFirst");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name.StartsWith(ROW_PREFIX))
		{
			int index = name.Substring(ROW_PREFIX.Length(), name.Length() - ROW_PREFIX.Length()).ToInt();
			Select(index);
			return;
		}

		if (name == PART_APPROVE)
			Decide(LF_PoliceApplicationGm.VERB_APPROVE);
		else if (name == PART_REJECT)
			Decide(LF_PoliceApplicationGm.VERB_REJECT);
	}

	//------------------------------------------------------------------------------------------------
	//! Show one application: name, why, availability.
	void Select(int index)
	{
		if (index < 0 || index >= m_aRows.Count())
			return;

		array<string> p = {};
		m_aRows[index].Split(LF_PoliceApplicationGm.SEP, p, false);
		m_sSelectedUid = p[0];
		m_Screen.SetLine(PART_DETAIL, WidgetManager.Translate("#LF-PoliceAppGm_Detail", p[1], p[2], p[3]));
	}

	//------------------------------------------------------------------------------------------------
	protected void Decide(int verb)
	{
		if (m_sSelectedUid.IsEmpty())
		{
			m_Screen.SetLine(PART_DETAIL, "#LF-PoliceAppGm_PickFirst");
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_PoliceApplicationGm.FEATURE, "decision not sent: no local player controller");
			return;
		}

		m_Screen.SetLine(PART_STATUS, "#LF-PoliceAppGm_Waiting");
		controller.LF_AskPoliceAppGmDecide(verb, m_sSelectedUid);
		m_sSelectedUid = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Session proof: press a button of the open sheet by part name.
	static bool Press(string name)
	{
		if (!s_Panel || !s_Panel.m_Screen || !s_Panel.m_Screen.IsOpen())
			return false;

		s_Panel.OnButton(name);
		return true;
	}
}
