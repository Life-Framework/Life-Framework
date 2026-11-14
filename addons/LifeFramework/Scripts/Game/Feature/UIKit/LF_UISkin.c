// LF_UISkin.c — what a sheet is made of: the face, the ink, the type.
//
// LF_UIScreen paints every part from one skin. A skin is built from a theme of the theme config
// (LF_UIThemeConfig, #966; FromTheme): Paper() is the 1989 form every sheet used before skins,
// Terminal() is the ATM's green phosphor screen in a dark bezel, PriceList() is a typed shop
// list (paper, a mono face), IdCard() the registry card. A pack overlay changes any of them by
// theme id. Only the size and the shape of a sheet are set here; no colour or face is.
//
// A skin that recolours buttons rewrites the vanilla button's text colour
// effects (white, orange on focus) to its own ink and focus colours.
class LF_UISkin
{
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
	//! Texture drawn on the sheet's face (the theme's m_PaperTexture). Empty: a flat sheet.
	ResourceName m_PaperTexture;
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
	//! A skin made from the theme with this id (LF_UITheme.Get: empty fields inherit the paper theme).
	//! \param id The theme id: paper, id-card, terminal or price-list.
	static LF_UISkin FromTheme(string id)
	{
		LF_UIThemeRecord theme = LF_UITheme.Get(id);
		LF_UIThemeRecord builtIn = LF_UITheme.BuiltIn(LF_UIThemeConfig.BASE_ID);
		LF_UISkin skin = new LF_UISkin();
		skin.m_sName = id;
		skin.m_Metal = LF_UITheme.Token(theme.m_sMetal, builtIn.m_sMetal);
		skin.m_Paper = LF_UITheme.Token(theme.m_sPaper, builtIn.m_sPaper);
		skin.m_Ink = LF_UITheme.Token(theme.m_sInk, builtIn.m_sInk);
		skin.m_Rule = LF_UITheme.Token(theme.m_sRule, builtIn.m_sRule);
		skin.m_Blur = LF_UITheme.Token(theme.m_sBlur, builtIn.m_sBlur);
		skin.m_Row = LF_UITheme.Token(theme.m_sRow, builtIn.m_sRow);
		skin.m_ButtonText = LF_UITheme.Token(theme.m_sButtonText, builtIn.m_sButtonText);
		skin.m_ButtonFocus = LF_UITheme.Token(theme.m_sButtonFocus, builtIn.m_sButtonFocus);
		skin.m_Font = theme.m_Font;
		skin.m_PaperTexture = theme.m_PaperTexture;
		skin.m_bRecolorButtons = true;
		if (!theme.m_sAccent.IsEmpty())
		{
			skin.m_Accent = LF_UIThemeRecord.ParseColor(theme.m_sAccent, null);
			skin.m_sAccentRgba = LF_UIThemeRecord.ToRgba(theme.m_sAccent);
		}

		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The paper form every sheet used before skins (the "paper" theme).
	static LF_UISkin Paper()
	{
		return FromTheme(LF_UIThemeConfig.BASE_ID);
	}

	//------------------------------------------------------------------------------------------------
	//! A printed notice (a sign card): the paper form, wider, a big title, a regular-weight body,
	//! and an accent for the section labels, the rule under the title and highlighted words.
	//! \param rim The frame colour; null keeps the theme's rim.
	//! \param accent Heading colour as 0..255 bytes.
	static LF_UISkin Notice(Color rim, int accentR, int accentG, int accentB)
	{
		LF_UISkin skin = Paper();
		skin.m_sName = "notice";
		skin.m_fWidth = 640;
		skin.m_fHeight = 920;
		skin.m_fTitleScale = 1.35;
		skin.m_BodyFont = LF_UITheme.FontRegular();
		skin.m_Accent = Color.FromSRGBA(accentR, accentG, accentB, 255);
		skin.m_sAccentRgba = string.Format("%1,%2,%3,255", accentR, accentG, accentB);
		skin.m_Rule = skin.m_Accent;
		skin.m_ButtonFocus = skin.m_Accent;
		if (rim)
			skin.m_Metal = rim;

		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! A laminated identity card (the "id-card" theme): pale registry paper with a faint green cast,
	//! a slate-blue rim and state-blue labels (the civil registry's ink).
	static LF_UISkin IdCard()
	{
		LF_UISkin skin = FromTheme("id-card");
		skin.m_fWidth = 600;
		// The card face: title, the registry line, the photo with its lines, a rule and one button.
		skin.m_fHeight = 560;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The police ID check on the citizen's screen: the identity card's paper, a short slip.
	static LF_UISkin IdCheck()
	{
		LF_UISkin skin = IdCard();
		skin.m_sName = "id-check";
		skin.m_fHeight = 420;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The "terminal" theme: green phosphor on a black tube in a beige-grey 1989 cash machine bezel.
	static LF_UISkin Terminal()
	{
		LF_UISkin skin = FromTheme("terminal");
		skin.m_fWidth = 640;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The shop's typed price list (the "price-list" theme): the paper sheet in a mono face.
	static LF_UISkin PriceList()
	{
		LF_UISkin skin = FromTheme("price-list");
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
