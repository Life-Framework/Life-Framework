// LF_Newspaper.c: the weekly newspaper's page (WO-W2.1). A held paper with a Read action opens this page,
// a paper full screen (paperwork is the one allowed kind of full screen) built from the sign card's UI kit parts.
//
// The edition is data: a masthead, a dateline and a few stories. Until the edition builder (WO-W2.2)
// publishes one with SetCurrent, the page prints a placeholder from the string table, so the item is
// never blank. Text fields are localization keys or plain text, like a sign's (LF_SignCatalog.Resolve).
// Core text is neutral; the pack's string table overrides the masthead key with the island's own name.

//------------------------------------------------------------------------------------------------
//! One story: a headline and its text. Either field may be a localization key; *word* draws in the accent.
class LF_NewspaperStory
{
	string m_sHeadline;
	string m_sBody;
}

//------------------------------------------------------------------------------------------------
//! One printed edition.
class LF_NewspaperIssue
{
	string m_sMasthead;
	string m_sDateline;
	ref array<ref LF_NewspaperStory> m_aStories = {};

	//------------------------------------------------------------------------------------------------
	void AddStory(string headline, string body)
	{
		LF_NewspaperStory story = new LF_NewspaperStory();
		story.m_sHeadline = headline;
		story.m_sBody = body;
		m_aStories.Insert(story);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Newspaper
{
	static const string FEATURE = "Newspaper";
	static const string KEY_MASTHEAD = "#LF-Newspaper_Masthead";
	static const string KEY_DATELINE = "#LF-Newspaper_Dateline";
	static const string KEY_QUIET_HEADLINE = "#LF-Newspaper_QuietHeadline";
	static const string KEY_QUIET_BODY = "#LF-Newspaper_QuietBody";
	static const string KEY_BOARDS_HEADLINE = "#LF-Newspaper_BoardsHeadline";
	static const string KEY_BOARDS_BODY = "#LF-Newspaper_BoardsBody";
	static const string KEY_CLOSE = "#LF-Sign_Close";

	//! The page fits the sheet at about this many stories; more are left off.
	static const int MAX_STORIES = 5; // lf-fixed: the sheet height fits five stories
	//! The page closes when the reader walks this far from the paper (or it is deleted).
	static const float CLOSE_DISTANCE = 4; // lf-fixed: same reach as a sign card

	protected static ref LF_NewspaperIssue s_Current;
	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	//! The edition a Read shows: the one last published, else the placeholder.
	static LF_NewspaperIssue Current()
	{
		if (s_Current)
			return s_Current;

		return Placeholder();
	}

	//------------------------------------------------------------------------------------------------
	//! Publishes the edition the next Read shows (the edition builder, WO-W2.2, calls this).
	static void SetCurrent(LF_NewspaperIssue issue)
	{
		if (!issue)
		{
			LF_Debug.Error(FEATURE, "edition refused: null; the page keeps the last edition");
			return;
		}

		s_Current = issue;
		LF_Debug.Info(FEATURE, string.Format("edition published: %1 stories", issue.m_aStories.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! What the page prints before any edition exists.
	static LF_NewspaperIssue Placeholder()
	{
		LF_NewspaperIssue issue = new LF_NewspaperIssue();
		issue.m_sMasthead = KEY_MASTHEAD;
		issue.m_sDateline = KEY_DATELINE;
		issue.AddStory(KEY_QUIET_HEADLINE, KEY_QUIET_BODY);
		issue.AddStory(KEY_BOARDS_HEADLINE, KEY_BOARDS_BODY);
		return issue;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the page for an edition. A masthead and a rule, the dateline, a headline and its text per
	//! story, then Close. A story with no headline text (a key no table knows) is left off.
	static array<ref LF_UIPart> BuildParts(LF_NewspaperIssue issue, LF_UISkin skin)
	{
		ref array<ref LF_UIPart> parts = {};
		if (!issue)
			return parts;

		string accent;
		if (skin)
			accent = skin.m_sAccentRgba;

		string masthead = LF_SignCatalog.Resolve(issue.m_sMasthead);
		if (masthead.IsEmpty())
			masthead = LF_SignCatalog.Resolve(KEY_MASTHEAD);

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Masthead", masthead));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "MastheadRule", ""));

		string dateline = LF_SignCatalog.RichBody(LF_SignCatalog.Resolve(issue.m_sDateline), accent);
		dateline.TrimInPlace();
		if (!dateline.IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, "Dateline", dateline));

		int printed;
		foreach (LF_NewspaperStory story : issue.m_aStories)
		{
			if (printed >= MAX_STORIES)
				break;

			if (!story)
				continue;

			string headline = LF_SignCatalog.Resolve(story.m_sHeadline);
			headline.TrimInPlace();
			if (headline.IsEmpty())
				continue;

			string name = "Story" + printed.ToString();
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, name, headline));
			string body = LF_SignCatalog.RichBody(LF_SignCatalog.Resolve(story.m_sBody), accent);
			body.TrimInPlace();
			if (!body.IsEmpty())
				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, name + "Body", body));

			printed++;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", KEY_CLOSE));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! How many parts of a kind a page has.
	static int CountKind(array<ref LF_UIPart> parts, LF_EUIPartKind kind)
	{
		int count;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == kind)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The words a page prints, one line per part (title, headings, bodies), markup and Close left out.
	static string PageText(array<ref LF_UIPart> parts)
	{
		string text;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == LF_EUIPartKind.CLOSE || part.GetKind() == LF_EUIPartKind.RULE)
				continue;

			string line = part.GetText();
			while (true)
			{
				int open = line.IndexOf("<color");
				if (open < 0)
					break;

				string tail = line.Substring(open, line.Length() - open);
				int close = tail.IndexOf(">");
				if (close < 0)
					break;

				line = line.Substring(0, open) + tail.Substring(close + 1, tail.Length() - close - 1); // lf-bounded: one story line
			}

			line.Replace("</color>", "");
			line.Replace("<br/>", " ");
			text += line + "\n";
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the current edition on the local client, anchored to the paper in hand.
	static LF_UIScreen Open(EntityID anchor)
	{
		Hide();
		LF_UISkin skin = LF_SignCatalog.CardSkin(false, vector.Zero, vector.Zero, vector.Zero);
		array<ref LF_UIPart> parts = BuildParts(Current(), skin);
		skin.m_fTypeScale = LF_SignCatalog.FitScale(parts, skin.m_fWidth);
		s_Screen = LF_UIScreen.Open(parts, skin);
		if (!s_Screen)
		{
			LF_Debug.Error(FEATURE, "the page failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor);
		LF_Debug.Info(FEATURE, "page open");
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen CurrentScreen()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): forget the edition and drop the page.
	static void ResetStatics()
	{
		Hide();
		s_Current = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Newspaper's statics (LF_StaticReset finds it by reflection).
class LF_Newspaper_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Newspaper";
	}

	override void Apply()
	{
		LF_Newspaper.ResetStatics();
	}
}
