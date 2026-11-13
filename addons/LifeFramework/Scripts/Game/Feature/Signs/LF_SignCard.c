// LF_SignCard.c — the paper card a Read opens. One card at a time, client-only.
// Built on LF_UIScreen: Close, MenuBack, death, losing the character, or walking
// CLOSE_DISTANCE meters from the sign closes it.

class LF_SignCard
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(LF_SignDef sign, EntityID anchor, LF_UISkin skin = null)
	{
		Hide();
		if (!sign)
		{
			LF_Debug.Error("Signs", "card refused: no sign");
			return null;
		}

		if (!skin)
			skin = LF_SignCatalog.CardSkin(false, vector.Zero, vector.Zero, vector.Zero);

		array<ref LF_UIPart> parts;
		if (LF_Credits.IsCreditsSign(sign))
			parts = LF_Credits.BuildParts(sign);
		else
			parts = LF_SignCatalog.BuildParts(sign, skin);

		skin.m_fTypeScale = LF_SignCatalog.FitScale(parts, skin.m_fWidth);
		s_Screen = LF_UIScreen.Open(parts, skin);
		if (!s_Screen)
		{
			LF_Debug.Error("Signs", "card failed to open for " + sign.m_sId);
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor);
		LF_Debug.Info("Signs", "card open: " + sign.m_sId);
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
