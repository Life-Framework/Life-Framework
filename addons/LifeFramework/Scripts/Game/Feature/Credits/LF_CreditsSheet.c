//------------------------------------------------------------------------------------------------
//! The in-game credits screen (#481). One kit sheet per credits section: a title, the section
//! heading, the section's entries in a wrapping RICH part, Previous, Next and Close.
//! Every row is a string key from LF_CreditsData, generated from docs/credits.md by
//! tools\cli credits, so the screen, the website and the README read one source.
//! Opened by the /credits chat command (LF_CreditsCommand.c). Local to the player; nothing is sent.
class LF_CreditsSheet
{
	static const string FEATURE = "Credits";
	static const string PART_PREV = "CreditsPrev";
	static const string PART_NEXT = "CreditsNext";
	static const float SHEET_WIDTH = 760;

	protected static ref LF_UIScreen s_Screen;
	protected static ref LF_CreditsSheet s_Sheet;
	protected static int s_iPage;
	protected static int s_iRows;

	protected ref array<string> m_aHeads = {};
	protected ref array<string> m_aBodies = {};
	protected LF_UIScreen m_Subscribed;

	//------------------------------------------------------------------------------------------------
	//! Splits the generated rows into one page per heading: the heading text and the body markup.
	//! A key the string table lacks logs an error and shows as itself, so a broken table is visible.
	//! \return The number of rows read, headings included.
	int Load(array<string> rows)
	{
		m_aHeads.Clear();
		m_aBodies.Clear();
		int read = 0;
		foreach (string row : rows)
		{
			array<string> cells = {};
			row.Split("|", cells, false);
			if (cells.Count() < 2)
			{
				LF_Debug.Error(FEATURE, "bad credits row " + row);
				continue;
			}

			read++;
			CheckKey(cells[1]);
			string label = WidgetManager.Translate(cells[1]);
			if (cells[0] == "H")
			{
				m_aHeads.Insert(label);
				m_aBodies.Insert(string.Empty);
				continue;
			}

			if (m_aHeads.Count() == 0)
			{
				m_aHeads.Insert(string.Empty);
				m_aBodies.Insert(string.Empty);
			}

			string line = label;
			if (cells.Count() > 2 && !cells[2].IsEmpty())
			{
				CheckKey(cells[2]);
				line = label + " (" + WidgetManager.Translate(cells[2]) + ")";
			}

			int last = m_aBodies.Count() - 1;
			m_aBodies.Set(last, m_aBodies[last] + line + "<br/><br/>");
		}

		return read;
	}

	//------------------------------------------------------------------------------------------------
	int PageCount()
	{
		return m_aHeads.Count();
	}

	//------------------------------------------------------------------------------------------------
	array<ref LF_UIPart> BuildParts(int page)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "CreditsTitle", "#LF-Credits_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "CreditsPage", WidgetManager.Translate("#LF-Credits_Page", (page + 1).ToString(), PageCount().ToString())));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "CreditsHeading", m_aHeads[page]));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, "CreditsBody", m_aBodies[page]));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_NEXT, "#LF-Credits_Next"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_PREV, "#LF-Credits_Prev"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "CreditsClose", "#LF-Credits_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the sheet on its first page.
	static LF_UIScreen Open()
	{
		Hide();
		LF_CreditsSheet sheet = new LF_CreditsSheet();
		s_iRows = sheet.Load(LF_CreditsData.GetRows());
		if (sheet.PageCount() == 0)
		{
			LF_Debug.Error(FEATURE, "credits screen refused: no credits rows");
			return null;
		}

		s_Sheet = sheet;
		s_iPage = 0;
		return sheet.Show();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UIScreen Show()
	{
		if (s_Screen)
		{
			s_Screen.Close();
			s_Screen = null;
		}

		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = SHEET_WIDTH;
		LF_UIScreen screen = LF_UIScreen.Open(BuildParts(s_iPage), skin);
		if (!screen)
		{
			LF_Debug.Error(FEATURE, "credits screen failed to open");
			return null;
		}

		screen.GetOnButton().Insert(OnButton);
		m_Subscribed = screen;
		screen.CloseOnDeath();
		screen.CloseWhenUncontrolled();
		s_Screen = screen;
		LF_Debug.Info(FEATURE, string.Format("credits page %1 of %2 open (%3 rows in all)", s_iPage + 1, PageCount(), s_iRows));
		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int count = PageCount();
		if (name == PART_NEXT)
			s_iPage = (s_iPage + 1) % count;
		else if (name == PART_PREV)
			s_iPage = (s_iPage + count - 1) % count;
		else
			return;

		// The press runs inside the old sheet; build the next one after it has finished.
		GetGame().GetCallqueue().CallLater(ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowLater()
	{
		if (s_Sheet == this)
			Show();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the string table has the key (a missing key translates to itself).
	static bool CheckKey(string key)
	{
		if (WidgetManager.Translate(key) != key)
			return true;

		LF_Debug.Error(FEATURE, "string key missing: " + key);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Rows read from the credits data when the sheet was opened, headings included.
	static int RowCount()
	{
		return s_iRows;
	}

	//------------------------------------------------------------------------------------------------
	static int Pages()
	{
		if (!s_Sheet)
			return 0;

		return s_Sheet.PageCount();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		s_Sheet = null;
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_CreditsSheet()
	{
		if (m_Subscribed)
			m_Subscribed.GetOnButton().Remove(OnButton);
	}
}
