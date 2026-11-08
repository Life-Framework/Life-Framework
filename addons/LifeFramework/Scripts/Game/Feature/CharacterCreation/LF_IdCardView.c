// LF_IdCardView.c — what you see when someone holds their ID card up to you.
// Client-only, on LF_UIScreen. The printed fields arrive from the server (LF_ShowIdCardView);
// the sheet closes on Close, MenuBack, death, losing the character, or once the person showing
// it is CLOSE_DISTANCE meters away.

class LF_IdCardView
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(string firstName, string lastName, int age)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "IdTitle", "#LF-ID_CardTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.ROW, "IdName", firstName + " " + lastName));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "IdAge", WidgetManager.Translate("#LF-ID_CardAge", age)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "IdClose", "#LF-ID_CardClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! \param shower The character holding the card up; null skips the distance check.
	static LF_UIScreen Open(string firstName, string lastName, int age, IEntity shower)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(firstName, lastName, age));
		if (!s_Screen)
		{
			EL_Debug.Error("IdCard", "card view failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (shower)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, shower.GetID());
		else
			EL_Debug.Warn("IdCard", "card view open without the shower in this client's world; no distance close");

		EL_Debug.Info("IdCard", "card view open");
		return s_Screen;
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

		s_Screen.Close();
		s_Screen = null;
	}
}
