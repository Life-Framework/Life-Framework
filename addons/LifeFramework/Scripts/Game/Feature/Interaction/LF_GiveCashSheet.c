// LF_GiveCashSheet.c — how much cash to hand over. Typing an amount is a correct UI (decision
// "In-world first"); everything else about giving is in the world: the notes leave your pocket,
// land in theirs, your arm reaches out, they hear it. Client-only, on LF_UIScreen. The server
// opened it (LF_PlayerInteraction.OpenGiveCash) and re-checks the amount it sends back.
// Closes on Close, MenuBack, death, losing the character, or walking away from the receiver.

class LF_GiveCashSheet
{
	static const string PART_AMOUNT = "GiveCashAmount";
	static const string PART_ERROR = "GiveCashError";
	static const string PART_GIVE = "GiveCashButton";
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_GiveCashSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected RplId m_Target;
	protected int m_iCarried;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(int carried)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "GiveCashTitle", "#LF-Interact_GiveCashTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "GiveCashCarried", LF_Terms.Text("#LF-Interact_GiveCashCarried", carried.ToString())));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_AMOUNT, "#LF-Interact_GiveCashAmount"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ERROR, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_GIVE, "#LF-Interact_GiveCashButton"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "GiveCashClose", "#LF-Interact_GiveCashKeep"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller, RplId target, int carried)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error("Interaction", "give cash sheet refused: no player controller");
			return null;
		}

		LF_GiveCashSheet sheet = new LF_GiveCashSheet();
		sheet.m_Controller = controller;
		sheet.m_Target = target;
		sheet.m_iCarried = carried;
		sheet.m_Screen = LF_UIScreen.Open(BuildParts(carried));
		if (!sheet.m_Screen)
		{
			LF_Debug.Error("Interaction", "give cash sheet failed to open");
			return null;
		}

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseWhenUncontrolled();
		IEntity receiver = LF_NetworkUtils.FindEntityByRplId(target);
		if (receiver)
			sheet.m_Screen.CloseBeyond(CLOSE_DISTANCE, receiver.GetID());
		else
			LF_Debug.Warn("Interaction", "give cash sheet open without the receiver in this client's world; no distance close");

		s_Sheet = sheet;
		LF_Debug.Info("Interaction", "give cash sheet open, carrying " + carried);
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_GiveCashSheet sheet = s_Sheet;
		s_Sheet = null;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name != PART_GIVE || !m_Screen)
			return;

		int amount = LF_PlayerInteraction.ParseAmount(m_Screen.GetFieldText(PART_AMOUNT));
		if (amount <= 0)
		{
			m_Screen.SetLine(PART_ERROR, "#LF-Interact_BadAmount");
			return;
		}

		if (amount > m_iCarried)
		{
			m_Screen.SetLine(PART_ERROR, "#LF-Interact_NotEnoughCash");
			return;
		}

		SCR_PlayerController controller = m_Controller;
		RplId target = m_Target;
		Hide();
		if (!controller)
		{
			LF_Debug.Error("Interaction", "give cash not sent: no player controller");
			return;
		}

		LF_Debug.Info("Interaction", "give cash sheet signed: " + amount);
		controller.LF_AskGiveCash(target, amount);
	}
}
