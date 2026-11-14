// LF_QuizSheet.c — the orientation sheet of the rules quiz (#915). Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 theme from LF_UITheme: no new
// .layout, no colours or fonts here (docs/style-guide.md "Screens").
// One question per page: the question, one button per option. After the last answer the choices go
// to the server, which scores them and sends back the result page: the rules the player got wrong
// and a Try again button, or a Continue button once they passed. Nothing here decides anything.
// Like the registration form, an unfinished sheet cannot be put away: it comes back.

class LF_QuizSheet
{
	static const string PART_TITLE = "QuizTitle";
	static const string PART_PROGRESS = "QuizProgress";
	static const string PART_BODY = "QuizBody";
	static const string PART_STATUS = "QuizStatus";
	static const string PART_RETRY = "QuizRetryButton";
	static const string PART_CONTINUE = "QuizContinueButton";
	static const string OPTION_PREFIX = "QuizOption";
	static const string OPTION_SUFFIX = "Button";

	protected static ref LF_QuizSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected ref array<ref array<string>> m_aRows = {};
	protected ref array<int> m_aAnswers = {};
	protected int m_iAttemptsLeft = -1;
	protected bool m_bDone;
	protected bool m_bSwitching;
	//! Parts of the page on screen, so MenuBack puts the same page back.
	protected ref array<ref LF_UIPart> m_aPage;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the quiz on its first question. A sheet already on screen is replaced (a retry).
	//! \param payload The questions as LF_Quiz.BuildPayload wrote them.
	//! \param attemptsLeft Failed attempts left before the kick, -1 unlimited.
	static LF_UIScreen Open(SCR_PlayerController controller, string payload, int attemptsLeft)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error(LF_Quiz.FEATURE, "quiz sheet refused: no player controller");
			return null;
		}

		LF_QuizSheet sheet = new LF_QuizSheet();
		sheet.m_Controller = controller;
		sheet.m_iAttemptsLeft = attemptsLeft;
		if (LF_Quiz.ParsePayload(payload, sheet.m_aRows) == 0)
		{
			LF_Debug.Error(LF_Quiz.FEATURE, "quiz sheet refused: the server sent no questions");
			return null;
		}

		s_Sheet = sheet;
		sheet.ShowQuestion();
		if (!sheet.m_Screen)
		{
			s_Sheet = null;
			return null;
		}

		LF_Debug.Info(LF_Quiz.FEATURE, string.Format("quiz sheet open, %1 question(s)", sheet.m_aRows.Count()));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the sheet with the result page.
	//! \param notes The explanation of each wrong answer, one per line.
	static LF_UIScreen ShowResult(SCR_PlayerController controller, bool passed, int correct, int total, int needed, int attemptsLeft, string notes)
	{
		LF_QuizSheet sheet = s_Sheet;
		if (!sheet)
		{
			sheet = new LF_QuizSheet();
			sheet.m_Controller = controller;
			s_Sheet = sheet;
		}

		sheet.m_iAttemptsLeft = attemptsLeft;
		sheet.Swap(BuildResultParts(passed, correct, total, needed, attemptsLeft, notes));
		if (!sheet.m_Screen)
		{
			s_Sheet = null;
			return null;
		}

		LF_Debug.Info(LF_Quiz.FEATURE, string.Format("quiz result shown: passed %1, %2 of %3", passed, correct, total));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the sheet down without bringing it back.
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_QuizSheet sheet = s_Sheet;
		s_Sheet = null;
		sheet.m_bDone = true;
		sheet.Release();
	}

	//------------------------------------------------------------------------------------------------
	//! One question page: its number, the question, one button per option.
	static array<ref LF_UIPart> BuildQuestionParts(array<string> row, int index, int total, int attemptsLeft)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-Quiz_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, PART_PROGRESS, LF_Terms.Text("#LF-Quiz_Progress", (index + 1).ToString(), total.ToString())));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_BODY, row[0]));
		for (int i = 1; i < row.Count(); i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, OPTION_PREFIX + (i - 1).ToString() + OPTION_SUFFIX, row[i]));
		}

		if (attemptsLeft >= 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, LF_Terms.Text("#LF-Quiz_Attempts", attemptsLeft.ToString())));

		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The result page: the score, the rules to read again and the button that moves on.
	static array<ref LF_UIPart> BuildResultParts(bool passed, int correct, int total, int needed, int attemptsLeft, string notes)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-Quiz_Title"));
		if (passed)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, LF_Terms.Text("#LF-Quiz_Passed", correct.ToString(), total.ToString())));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_CONTINUE, "#LF-Quiz_Continue"));
			return parts;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, LF_Terms.Text("#LF-Quiz_Failed", correct.ToString(), total.ToString(), needed.ToString())));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, PART_PROGRESS, "#LF-Quiz_Rules"));
		array<string> lines = {};
		notes.Split(LF_Quiz.ROW_SEP, lines, true);
		foreach (int i, string line : lines)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_BODY + i.ToString(), line));
		}

		if (attemptsLeft == 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS + "Kick", "#LF-Quiz_NoAttempts"));
		else
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_RETRY, "#LF-Quiz_Retry"));

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
	protected void ShowQuestion()
	{
		int index = m_aAnswers.Count();
		if (index >= m_aRows.Count())
			return;

		Swap(BuildQuestionParts(m_aRows[index], index, m_aRows.Count(), m_iAttemptsLeft));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a page on screen in place of the one showing.
	protected void Swap(array<ref LF_UIPart> parts)
	{
		m_bSwitching = true;
		Release();

		m_bSwitching = false;
		m_aPage = parts;
		m_Screen = LF_UIScreen.Open(parts);
		if (!m_Screen)
		{
			LF_Debug.Error(LF_Quiz.FEATURE, "quiz sheet failed to open");
			return;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (m_bDone || !m_Screen)
			return;

		if (name == PART_CONTINUE)
		{
			SCR_PlayerController controller = m_Controller;
			Hide();
			if (controller)
				controller.LF_AskQuizContinue();

			return;
		}

		if (name == PART_RETRY)
		{
			if (m_Controller)
				m_Controller.LF_AskQuizRetry();

			return;
		}

		if (!name.StartsWith(OPTION_PREFIX) || !name.EndsWith(OPTION_SUFFIX))
			return;

		int prefixLength = OPTION_PREFIX.Length();
		string number = name.Substring(prefixLength, name.Length() - prefixLength - OPTION_SUFFIX.Length());
		m_aAnswers.Insert(number.ToInt());
		if (m_aAnswers.Count() < m_aRows.Count())
		{
			ShowQuestion();
			return;
		}

		string answers;
		foreach (int i, int answer : m_aAnswers)
		{
			if (i > 0)
				answers += ",";

			answers += answer.ToString();
		}

		LF_Debug.Info(LF_Quiz.FEATURE, string.Format("quiz answers sent (%1)", answers));
		ref array<ref LF_UIPart> waiting = {};
		waiting.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-Quiz_Title"));
		waiting.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, "#LF-Quiz_Checking"));
		Swap(waiting);
		if (m_Controller)
			m_Controller.LF_AskQuizAnswers(answers);
	}

	//------------------------------------------------------------------------------------------------
	//! MenuBack closed the sheet before the player was through: the same page comes back next tick.
	protected void OnClosed()
	{
		if (m_bSwitching || m_bDone || s_Sheet != this)
			return;

		LF_Debug.Info(LF_Quiz.FEATURE, "quiz sheet put away unfinished, opening it again");
		GetGame().GetCallqueue().CallLater(Reopen, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Reopen()
	{
		if (m_bDone || s_Sheet != this || !m_aPage)
			return;

		Swap(m_aPage);
	}
}
