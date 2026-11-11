// LF_UISkin.c — what a sheet is made of: the face, the ink, the type.
//
// LF_UIScreen paints every part from one skin. Paper() is the 1989 form every
// sheet used before skins (LF_UITheme values, unchanged). Terminal() is the
// ATM's green phosphor screen in a dark bezel. PriceList() is a typed shop
// list: paper, a mono face, and ink-coloured buttons.
//
// A skin that recolours buttons rewrites the vanilla button's text colour
// effects (white, orange on focus) to its own ink and focus colours.
class LF_UISkin
{
	//! Vanilla mono face (a debug layout in the base game uses it). No font is imported.
	static const ResourceName FONT_MONO = "{0E041C5B1F27DCEA}system/robotomono_msdf_28.fnt";

	ref Color m_Metal;
	ref Color m_Paper;
	ref Color m_Ink;
	ref Color m_Rule;
	ref Color m_Blur;
	ref Color m_Row;
	ref Color m_ButtonText;
	ref Color m_ButtonFocus;
	//! Section labels and highlighted words on a notice. Null: the ink.
	ref Color m_Accent;
	//! The accent as 0..255 bytes, for RichTextWidget <color rgba='r,g,b,a'> markup.
	string m_sAccentRgba;
	ResourceName m_Font = LF_UITheme.FONT;
	//! Face of RICH body text. Empty: the sheet's font.
	ResourceName m_BodyFont;
	//! Multiplies the title size only (1 is LF_UITheme.FONT_TITLE).
	float m_fTitleScale = 1;
	bool m_bRecolorButtons;
	string m_sName = "paper";
	//! Sheet width in reference pixels (LF_UIFrame.layout is 560).
	float m_fWidth = 560;
	//! Sheet height in reference pixels (LF_UIFrame.layout is 900). 0 keeps the layout's.
	float m_fHeight;
	//! Multiplies every font size on the sheet (1 is LF_UITheme). A shop list is read at arm's length.
	float m_fTypeScale = 1;
	//! Dim and soften the world behind the sheet (the full-screen blur in LF_UIFrame.layout).
	bool m_bBlur = true;

	//------------------------------------------------------------------------------------------------
	static LF_UISkin Paper()
	{
		LF_UISkin skin = new LF_UISkin();
		skin.m_sName = "paper";
		skin.m_Metal = LF_UITheme.Metal();
		skin.m_Paper = LF_UITheme.Paper();
		skin.m_Ink = LF_UITheme.Ink();
		skin.m_Rule = LF_UITheme.Rule();
		skin.m_Blur = LF_UITheme.Blur();
		skin.m_Row = LF_UITheme.Row();
		skin.m_ButtonText = LF_UITheme.Ink();
		skin.m_ButtonFocus = Color.FromSRGBA(150, 40, 30, 255);
		skin.m_bRecolorButtons = true;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! A printed notice (a sign card): the paper form, wider, a big title, a regular-weight body,
	//! and an accent for the section labels, the rule under the title and highlighted words.
	//! \param rim The frame colour; null keeps the blued steel.
	//! \param accent Heading colour as 0..255 bytes.
	static LF_UISkin Notice(Color rim, int accentR, int accentG, int accentB)
	{
		LF_UISkin skin = Paper();
		skin.m_sName = "notice";
		skin.m_fWidth = 640;
		skin.m_fHeight = 920;
		skin.m_fTitleScale = 1.35;
		skin.m_BodyFont = LF_UITheme.FONT_REGULAR;
		skin.m_Accent = Color.FromSRGBA(accentR, accentG, accentB, 255);
		skin.m_sAccentRgba = string.Format("%1,%2,%3,255", accentR, accentG, accentB);
		skin.m_Rule = skin.m_Accent;
		skin.m_ButtonFocus = skin.m_Accent;
		if (rim)
			skin.m_Metal = rim;

		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! Green phosphor on a black tube in a beige-grey 1989 cash machine bezel. Focus is amber.
	static LF_UISkin Terminal()
	{
		LF_UISkin skin = new LF_UISkin();
		skin.m_sName = "terminal";
		skin.m_Metal = Color.FromSRGBA(150, 146, 132, 255);
		skin.m_Paper = Color.FromSRGBA(6, 14, 8, 255);
		skin.m_Ink = Color.FromSRGBA(96, 236, 120, 255);
		skin.m_Rule = Color.FromSRGBA(34, 110, 52, 255);
		skin.m_Blur = LF_UITheme.Blur();
		skin.m_Row = Color.FromSRGBA(14, 34, 18, 255);
		skin.m_ButtonText = Color.FromSRGBA(96, 236, 120, 255);
		skin.m_ButtonFocus = Color.FromSRGBA(255, 176, 0, 255);
		skin.m_Font = FONT_MONO;
		skin.m_bRecolorButtons = true;
		skin.m_fWidth = 640;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The shop's typed price list: the paper sheet, a mono face, carbon ink, a brass focus.
	static LF_UISkin PriceList()
	{
		LF_UISkin skin = Paper();
		skin.m_sName = "price-list";
		skin.m_Font = FONT_MONO;
		// Maintainer playtest 2026-11-08: the store list was too small to read at 1920x1080.
		skin.m_fWidth = 900;
		skin.m_fTypeScale = 1.35;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! Rewrites the colour effect on a vanilla button's Text widget to this skin's ink and hides
	//! its arrow glyph (Image0).
	//! \return How many effects were rewritten (0 when the skin keeps vanilla colours).
	int RecolorButton(Widget button)
	{
		if (!m_bRecolorButtons || !button)
			return 0;

		SCR_ModularButtonComponent modular = SCR_ModularButtonComponent.FindComponent(button);
		if (!modular)
			return 0;

		int rewritten = 0;
		foreach (SCR_ButtonEffectBase effect : modular.GetAllEffects())
		{
			SCR_ButtonEffectColor colorEffect = SCR_ButtonEffectColor.Cast(effect);
			if (!colorEffect)
				continue;

			Widget target = colorEffect.GetTargetWidget();
			if (!target)
				continue;

			// The pause-menu arrow: a menu glyph, not part of a key or a typed list.
			if (target.GetName() == "Image0")
			{
				colorEffect.m_cFocusGained = Color.FromSRGBA(0, 0, 0, 0);
				colorEffect.m_cFocusLost = Color.FromSRGBA(0, 0, 0, 0);
				rewritten++;
				continue;
			}

			if (target.GetName() != "Text")
				continue;

			colorEffect.m_cDefault = Color.FromInt(m_ButtonText.PackToInt());
			colorEffect.m_cHovered = Color.FromInt(m_ButtonFocus.PackToInt());
			colorEffect.m_cFocusGained = Color.FromInt(m_ButtonFocus.PackToInt());
			colorEffect.m_cFocusLost = Color.FromInt(m_ButtonText.PackToInt());
			rewritten++;
		}

		modular.InvokeAllEnabledEffects(true);
		return rewritten;
	}
}
