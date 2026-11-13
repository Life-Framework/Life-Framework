// LF_IdCardView.c — the identity card read on screen (#902): your own from the inventory
// (Inspect), or someone else's held up to you (Show ID, a police request). Client-only, on
// LF_UIScreen with a PORTRAIT part. Everything printed arrives from the server
// (LF_IdCard.SendView); this machine draws the photo from the look (LF_WantedPortrait, a dressed
// dummy in the preview world) and deletes the dummy when the sheet closes. A shown card closes
// after the seconds the server sends, once the person showing it is CLOSE_DISTANCE metres away,
// on death, on losing the character, and on Close or MenuBack.

class LF_IdCardView
{
	static const float CLOSE_DISTANCE = 4;	// lf-fixed: arm's length and a step, as every sheet held up to you (LF_HospitalBillCard)
	static const string PORTRAIT = "IdPortrait";
	static const string VOID_LINE = "IdVoid";
	static const ResourceName STAMP_IMAGESET = "{F02831E43418249A}Assets/UI/Illustrations/LF_IdStamp.imageset";
	static const string STAMP_IMAGE = "full";

	protected static ref LF_UIScreen s_Screen;
	protected static ref LF_WantedPortrait s_Portrait;
	//! Counts opens, so a timer from an earlier card never closes a later one.
	protected static int s_iOpened;

	//------------------------------------------------------------------------------------------------
	//! The card: title, the registry heading, the photo with the stamp and the five printed lines,
	//! VOID when it was replaced since, and Close ("Hand it back" on a card shown to you, "Put it
	//! away" on your own). Empty fields print a dash.
	static array<ref LF_UIPart> BuildParts(string firstName, string lastName, int age, string number, string issued, bool isVoid, bool shown)
	{
		array<string> labels = {"#LF-IdCard_Surname", "#LF-IdCard_GivenName", "#LF-IdCard_Age", "#LF-IdCard_Issued", "#LF-IdCard_Number"};
		array<string> values = {Printed(lastName), Printed(firstName), age.ToString(), Printed(issued), Printed(number)};

		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "IdTitle", "#LF-ID_CardTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "IdRegistry", "#LF-IdCard_Registry"));
		parts.Insert(LF_UIPart.CreatePortrait(PORTRAIT, STAMP_IMAGESET, STAMP_IMAGE, labels, values));
		if (isVoid)
			parts.Insert(LF_UIPart.CreateStatus(VOID_LINE, "#LF-IdCard_VoidLabel", "#LF-IdCard_Void", string.Empty));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "IdRule", string.Empty));
		string close = "#LF-IdCard_PutAway";
		if (shown)
			close = "#LF-ID_CardClose";

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "IdClose", close));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The card number and the date of issue as one value for the wire: "EV 000003|09/06/1989".
	static string PackIssue(string number, string issued)
	{
		return number + "|" + issued;
	}

	//------------------------------------------------------------------------------------------------
	static void UnpackIssue(string issue, out string number, out string issued)
	{
		number = string.Empty;
		issued = string.Empty;
		int bar = issue.IndexOf("|");
		if (bar < 0)
		{
			number = issue;
			return;
		}

		number = issue.Substring(0, bar);
		issued = issue.Substring(bar + 1, issue.Length() - bar - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Printed(string value)
	{
		if (value.IsEmpty())
			return "-";

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! \param look The holder's look at the date of issue (LF_WantedLook); empty leaves the photo
	//! frame blank (a card from before the registry).
	//! \param shower The character holding the card up; null for your own card (no distance check).
	//! \param seconds Close after this long; 0 stays until closed.
	static LF_UIScreen Open(string firstName, string lastName, int age, string number, string issued, string look, bool isVoid, IEntity shower, float seconds)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(firstName, lastName, age, number, issued, isVoid, seconds > 0), LF_UISkin.IdCard());
		if (!s_Screen)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "card view failed to open");
			return null;
		}

		s_iOpened++;
		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		s_Screen.GetOnClosed().Insert(OnClosed);
		if (shower)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, shower.GetID());

		s_Portrait = new LF_WantedPortrait();
		int dressed = s_Portrait.Show(s_Screen.GetPortrait(PORTRAIT), look);
		if (seconds > 0 && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(CloseAfter, Math.Round(seconds * 1000), false, s_iOpened);

		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("card view open: %1, photo dressed %2 item(s), void %3, shown %4, %5 s", number, dressed, isVoid, shower != null, seconds));
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CloseAfter(int opened)
	{
		if (opened != s_iOpened || !s_Screen)
			return;

		LF_Debug.Info(LF_PhotoIdCard.FEATURE, "card view closed: the card was taken back");
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnClosed()
	{
		if (s_Portrait)
			s_Portrait.Clear();

		s_Portrait = null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		LF_UIScreen screen = s_Screen;
		s_Screen = null;
		screen.Close();
		OnClosed();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		Hide();
		s_iOpened = 0;
	}
}
