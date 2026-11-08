// LF_SignCard.c — the paper card a Read opens. One card at a time, client-only.
// Built on LF_UIScreen: Close, MenuBack, death, losing the character, or walking
// CLOSE_DISTANCE meters from the sign closes it.

class LF_SignCard
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(LF_SignDef sign, EntityID anchor)
	{
		Hide();
		if (!sign)
		{
			EL_Debug.Error("Signs", "card refused: no sign");
			return null;
		}

		s_Screen = LF_UIScreen.Open(LF_SignCatalog.BuildParts(sign));
		if (!s_Screen)
		{
			EL_Debug.Error("Signs", "card failed to open for " + sign.m_sId);
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor);
		EL_Debug.Info("Signs", "card open: " + sign.m_sId);
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
