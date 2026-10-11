// LF_UISampleScreen.c — one screen that lists every kit part.
// Nothing in the world, no action, and no menu preset calls Open.
// The agent-box `kit` command is the only opener, and only in a play session.
class LF_UISampleScreen
{
	protected static ref LF_UIScreen s_Screen;
	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> GetParts()
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-UIKit_SampleTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Line", "#LF-UIKit_SampleLine"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Balance", "#LF-UIKit_SampleBalance"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Field", "#LF-UIKit_SampleField"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.ROW, "Row", "#LF-UIKit_SampleRow"));
		parts.Insert(LF_UIPart.CreateStatus("Status", "#LF-UIKit_SampleStatusLabel", "#LF-UIKit_SampleStatus", "#LF-UIKit_SampleAction"));
		parts.Insert(LF_UIPart.CreateQuantity("Quantity", "#LF-UIKit_SampleQuantity", 5, 2));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Button", "#LF-UIKit_SampleButton"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-UIKit_SampleClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open()
	{
		Hide();
		s_Screen = LF_UIScreen.Open(GetParts());
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
}
