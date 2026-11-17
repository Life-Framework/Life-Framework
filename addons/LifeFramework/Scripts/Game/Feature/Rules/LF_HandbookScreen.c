// LF_HandbookScreen.c: the handbook screen (#1773, refs #642). Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 theme from LF_UITheme: no new
// .layout, no colours or fonts here (docs/style-guide.md "Screens").
// The pause menu's Handbook button opens it (UI/LF_PauseMenuHandbook.c). Page one is the list of
// pages (LF_Handbook.GetPages); picking a row swaps in that page's title and paragraphs, with a
// button back to the list. Back (Escape, Backspace, gamepad B) and Close put the handbook away from
// either page: LF_UIScreen owns the Back path (LF_UIBack), nothing here reads MenuBack. Nothing here
// decides anything; the pages are config data (LF_Handbook).

class LF_HandbookScreen
{
	static const string FEATURE = "Rules";
	static const string PART_TITLE = "HandbookTitle";
	static const string PART_PICK = "HandbookPick";
	static const string PART_EMPTY = "HandbookEmpty";
	static const string PART_LIST = "HandbookPages";
	static const string PART_PAGE_TITLE = "HandbookPageTitle";
	static const string PART_BODY = "HandbookBody";
	static const string PART_CONTENTS = "HandbookContentsButton";
	static const string PART_CLOSE = "HandbookCloseButton";
	//! A page row is ROW_PREFIX + its place in the list: HandbookPage0, HandbookPage1, ...
	static const string ROW_PREFIX = "HandbookPage";
	static const int MAX_ROWS = 40;
	static const float LIST_HEIGHT = 300;
	//! The paper sheet, wider than the default so a page title fits its row beside the Read button.
	static const float SHEET_WIDTH = 640;

	protected static ref LF_HandbookScreen s_Sheet;

	protected ref LF_UIScreen m_Screen;
	//! The pages in list order, by id: a pick looks the page up again, so a config reload cannot leave a stale record.
	protected ref array<string> m_aPageIds = {};
	protected bool m_bSwitching;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen()
	{
		LF_UIScreen screen = Current();
		return screen && screen.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UISkin Skin()
	{
		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = SHEET_WIDTH;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the handbook on its list of pages. A handbook already on screen is replaced.
	//! \return The screen, or null when it failed to open (logged).
	static LF_UIScreen Open()
	{
		Hide();
		LF_HandbookScreen sheet = new LF_HandbookScreen();
		array<LF_HandbookPage> pages = {};
		LF_Handbook.GetPages(pages);
		foreach (LF_HandbookPage page : pages)
		{
			if (sheet.m_aPageIds.Count() >= MAX_ROWS)
			{
				LF_Debug.Warn(FEATURE, string.Format("handbook lists %1 pages and no more, the rest are left off the list", MAX_ROWS));
				break;
			}

			sheet.m_aPageIds.Insert(page.m_sId);
		}

		s_Sheet = sheet;
		sheet.ShowList();
		if (!sheet.m_Screen)
		{
			s_Sheet = null;
			return null;
		}

		LF_Debug.Info(FEATURE, string.Format("handbook open, %1 page(s)", sheet.m_aPageIds.Count()));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! What the pause menu's button calls on the tick after it closes the pause menu.
	static void OpenFromPause()
	{
		Open();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the handbook down.
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_HandbookScreen sheet = s_Sheet;
		s_Sheet = null;
		sheet.m_bDone = true;
		sheet.Release();
	}

	//------------------------------------------------------------------------------------------------
	//! The row name of the page at this place in the list.
	static string RowName(int index)
	{
		return ROW_PREFIX + index.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The list place a row name stands for, or -1 for any other part name (the list itself is
	//! "HandbookPages", which starts with the row prefix but has no number after it).
	static int RowIndex(string name)
	{
		if (!name.StartsWith(ROW_PREFIX))
			return -1;

		string number = name.Substring(ROW_PREFIX.Length(), name.Length() - ROW_PREFIX.Length());
		if (number.IsEmpty())
			return -1;

		for (int i = 0; i < number.Length(); i++)
		{
			string ch = number.Substring(i, 1);
			if (ch < "0" || ch > "9")
				return -1;
		}

		return number.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! The list page: the heading, the line above the list and the list (its rows are added after
	//! Open), or one line saying there is nothing to read.
	static array<ref LF_UIPart> BuildListParts(bool hasPages)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-Handbook_Title"));
		if (hasPages)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PICK, "#LF-Handbook_Pick"));
			parts.Insert(LF_UIPart.CreateList(PART_LIST));
		}
		else
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_EMPTY, "#LF-Handbook_Empty"));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-Handbook_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! One page: the handbook's name as a small label, the page title, its paragraphs in reading
	//! order, a button back to the list and Close.
	static array<ref LF_UIPart> BuildPageParts(notnull LF_HandbookPage page)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, PART_TITLE, "#LF-Handbook_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_PAGE_TITLE, page.m_sTitleKey));
		if (page.m_aBodyKeys)
		{
			foreach (int i, string body : page.m_aBodyKeys)
			{
				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_BODY + i.ToString(), body));
			}
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_CONTENTS, "#LF-Handbook_Contents"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-Handbook_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Lets go of the screen on show: stops listening to it, then closes it.
	protected void Release()
	{
		if (!m_Screen)
			return;

		m_Screen.GetOnButton().Remove(OnButton);
		m_Screen.GetOnClosed().Remove(OnClosed);
		m_Screen.Close();
		m_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a page of parts on screen in place of the one showing.
	protected bool Swap(array<ref LF_UIPart> parts)
	{
		m_bSwitching = true;
		Release();
		m_bSwitching = false;

		m_Screen = LF_UIScreen.Open(parts, Skin());
		if (!m_Screen)
		{
			LF_Debug.Error(FEATURE, "handbook failed to open");
			return false;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		m_Screen.CloseOnDeath();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowList()
	{
		if (!Swap(BuildListParts(!m_aPageIds.IsEmpty())))
			return;

		if (m_aPageIds.IsEmpty())
			return;

		m_Screen.SetListHeight(PART_LIST, LIST_HEIGHT);
		foreach (int i, string id : m_aPageIds)
		{
			LF_HandbookPage page = LF_Handbook.Find(id);
			if (!page)
				continue;

			m_Screen.AddListRow(PART_LIST, RowName(i), LF_Terms.Shown(page.m_sTitleKey), "", "#LF-Handbook_Read");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPage(int index)
	{
		if (index < 0 || index >= m_aPageIds.Count())
		{
			LF_Debug.Warn(FEATURE, string.Format("handbook pick %1 is not on the list of %2 page(s)", index, m_aPageIds.Count()));
			return;
		}

		LF_HandbookPage page = LF_Handbook.Find(m_aPageIds[index]);
		if (!page)
		{
			LF_Debug.Error(FEATURE, string.Format("handbook page '%1' is gone from the config, back to the list", m_aPageIds[index]));
			ShowList();
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("handbook page '%1' shown", page.m_sId));
		Swap(BuildPageParts(page));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (m_bDone || !m_Screen)
			return;

		if (name == PART_CONTENTS)
		{
			ShowList();
			return;
		}

		int index = RowIndex(name);
		if (index >= 0)
			ShowPage(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Close, Back (Escape, Backspace, gamepad B) or death closed the screen: the handbook is done,
	//! unless this class closed it itself to put the next page on screen.
	protected void OnClosed()
	{
		if (m_bSwitching || m_bDone || s_Sheet != this)
			return;

		m_bDone = true;
		s_Sheet = null;
		m_Screen = null;
		LF_Debug.Info(FEATURE, "handbook closed");
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_Sheet = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `pausemenu [open|close]`: opens the pause menu the way Escape does (or puts it away) so a
//! flow can press its Handbook button (agent-box/flows/ui-handbook.txt). Opening asks the same
//! Game.OpenPauseMenu the Escape key does; an open menu is left as it is.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "pausemenu")
			return super.Handle(cmd);

		MenuManager menus = GetGame().GetMenuManager();
		if (!menus)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_menu");
			return true;
		}

		MenuBase open = menus.FindMenuByPreset(ChimeraMenuPreset.PauseMenu);
		if (cmd.Arg1 == "close")
		{
			if (open)
				menus.CloseMenuByPreset(ChimeraMenuPreset.PauseMenu);

			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return true;
		}

		if (open)
		{
			LF_AgentHarness.Answer(cmd.Id, "ok", "already_open");
			return true;
		}

		GetGame().OpenPauseMenu(false, false);
		LF_AgentHarness.Answer(cmd.Id, "ok", "opened");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HandbookScreen's statics (registered at script load; Core/LF_StaticReset.c).
class LF_HandbookScreen_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_HandbookScreen_StaticResetHook());

	override string Owner()
	{
		return "LF_HandbookScreen";
	}

	override void Apply()
	{
		LF_HandbookScreen.ResetStatics();
	}
}
