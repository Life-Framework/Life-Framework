// LF_CharacterSelectSheet.c — the residents on file for this account, shown on arrival when the
// config allows more than one character per account (#331, LF_CharacterConfig). Client-only, on
// LF_UIScreen, in the registry style of form R-89: one line per character and, while a slot is
// free, "register a new resident". The sheet cannot be put away: a player who has not picked has
// nothing to play. The server re-checks every pick (LF_CharacterCreationRules.RefusePick).

class LF_CharacterSelectSheet
{
	//! Pick sent for "register a new resident".
	static const int SLOT_NEW = -1;
	//! Separates the character names in the server's one-string list (a name never holds one).
	static const string NAME_SEPARATOR = "\n";
	static const string PART_SLOT = "CharSlot";
	static const string PART_NEW = "CharNew";

	protected static ref LF_CharacterSelectSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected string m_sNames;
	protected bool m_bCanRegister;
	protected bool m_bPicked;

	//------------------------------------------------------------------------------------------------
	//! \param names Character names in slot order, NAME_SEPARATOR between them.
	static array<ref LF_UIPart> BuildParts(string names, bool canRegister)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "CharSelTitle", "#LF-CharSel_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "CharSelIntro", "#LF-CharSel_Intro"));

		array<string> split = {};
		names.Split(NAME_SEPARATOR, split, false);
		foreach (int slot, string name : split)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_SLOT + slot, name));
		}

		if (canRegister)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_NEW, "#LF-CharSel_New"));

		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The pick a pressed part stands for: a 0-based slot, SLOT_NEW, or -2 for anything else.
	static int SlotOfPart(string name)
	{
		if (name == PART_NEW)
			return SLOT_NEW;

		if (!name.StartsWith(PART_SLOT))
			return -2;

		string digits = name.Substring(PART_SLOT.Length(), name.Length() - PART_SLOT.Length());
		if (digits.IsEmpty())
			return -2;

		return digits.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller, string names, bool canRegister)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error("CharacterCreation", "character pick refused: no player controller");
			return null;
		}

		LF_CharacterSelectSheet sheet = new LF_CharacterSelectSheet();
		sheet.m_Controller = controller;
		sheet.m_sNames = names;
		sheet.m_bCanRegister = canRegister;
		sheet.m_Screen = LF_UIScreen.Open(BuildParts(names, canRegister));
		if (!sheet.m_Screen)
			return null;

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.GetOnClosed().Insert(sheet.OnClosed);
		s_Sheet = sheet;
		LF_Debug.Info("CharacterCreation", "character pick open");
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
	//! Takes the sheet down without re-opening it.
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_CharacterSelectSheet sheet = s_Sheet;
		s_Sheet = null;
		sheet.m_bPicked = true;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (m_bPicked || !m_Screen)
			return;

		int slot = SlotOfPart(name);
		if (slot < SLOT_NEW)
			return;

		if (!m_Controller)
		{
			LF_Debug.Error("CharacterCreation", "character pick not sent: no player controller");
			return;
		}

		LF_Debug.Info("CharacterCreation", "character pick: slot " + slot);
		SCR_PlayerController controller = m_Controller;
		Hide();
		controller.LF_AskSelectCharacter(slot);
	}

	//------------------------------------------------------------------------------------------------
	//! MenuBack closed the sheet before a pick: put it back on the next tick (not inside the close).
	protected void OnClosed()
	{
		if (m_bPicked || s_Sheet != this)
			return;

		LF_Debug.Info("CharacterCreation", "character pick put away, opening it again");
		GetGame().GetCallqueue().CallLater(Reopen, 0, false, m_Controller, m_sNames, m_bCanRegister);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Reopen(SCR_PlayerController controller, string names, bool canRegister)
	{
		Open(controller, names, canRegister);
	}
}
