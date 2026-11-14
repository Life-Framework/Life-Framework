// LF_UITheme.c — the one sheet: type, padding, and colours, read from the theme config.
//
// The 1989 look is paper and dull metal, carbon ink, a brass rule. No neon. The values live in
// Configs/UI/LF_UITheme.conf (LF_UIThemeConfig, #966) and a pack overlay can change them by theme id,
// so a modern pack restyles every sheet without touching a layout. This class is the one reader:
// a feature never reads the config, never sets a colour, a font or a size (validate-ui-kit).
// BuiltIn() holds the same 1989 values in code, so a theme file that does not load leaves every
// sheet looking as it always did; LF_Test_UIThemePack proves the core file equals it.
//
// Part layouts are structure and vanilla widget parents. LF_UIScreen paints every part from these
// values when it assembles a screen.
//
// The edit-box well is the exception the vanilla component reads at creation:
// LF_UIEditBox.layout m_BackgroundDefault is FIELD_WELL (214, 200, 170), written there in
// linear space (0.672 0.578 0.402) because the component animates the raw value. It is a layout
// value, not a theme token: a pack with a different paper recolours the well in its own layout skin.
// The sheet is solid ImageWidgets (no texture unless the theme names one): a PanelWidget draws the
// engine's default-style panel image tinted by its colour, which reads as see-through.
// Blur strength (0.4) is authored on the full-screen blur widget in LF_UIFrame.layout;
// its colour multiplies the world behind the sheet, so it is the dim.
class LF_UITheme
{
	//! The built-in print face; the theme's m_Font is empty only when the config does not load.
	static const ResourceName FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";

	static const string FRAME_LAYOUT = "{26EFA919E73EA3B1}UI/Layouts/UIKit/LF_UIFrame.layout";
	static const string TITLE_LAYOUT = "{1266676650F735E3}UI/Layouts/UIKit/LF_UITitle.layout";
	static const string LINE_LAYOUT = "{0A818180AD5E8749}UI/Layouts/UIKit/LF_UILine.layout";
	static const string BUTTON_LAYOUT = "{B90B99E1ADB16E16}UI/Layouts/UIKit/LF_UIButton.layout";
	static const string EDIT_LAYOUT = "{618CD52A98590696}UI/Layouts/UIKit/LF_UIEditBox.layout";
	static const string ROW_LAYOUT = "{635A66887A0CB6BF}UI/Layouts/UIKit/LF_UIListRow.layout";
	static const string STATUS_LAYOUT = "{FF15C6F4AC42BECB}UI/Layouts/UIKit/LF_UIStatusRow.layout";
	static const string QUANTITY_LAYOUT = "{7BD6344F5ACD8B35}UI/Layouts/UIKit/LF_UIQuantity.layout";
	static const string LIST_LAYOUT = "{9A3E5C71B2D84F06}UI/Layouts/UIKit/LF_UIList.layout";
	static const string KEYS_LAYOUT = "{B1C08A9DC82E6FEC}UI/Layouts/UIKit/LF_UIKeyRow.layout";
	static const string HEADING_LAYOUT = "{2EA91E6A335D2FCE}UI/Layouts/UIKit/LF_UIHeading.layout";
	static const string RULE_LAYOUT = "{C200C3E91BC38F87}UI/Layouts/UIKit/LF_UIRule.layout";
	static const string RICH_LAYOUT = "{7AA3823D51F36803}UI/Layouts/UIKit/LF_UIRich.layout";
	static const string IMAGE_LAYOUT = "{E958415CA199CA4F}UI/Layouts/UIKit/LF_UIImage.layout";
	static const string PORTRAIT_LAYOUT = "{16D71EF292B6A54C}UI/Layouts/UIKit/LF_UIPortrait.layout";

	//! The regular face a notice's body is set in, so the bold section labels stand out.
	static const ResourceName FONT_REGULAR = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";

	//! Vanilla mono face (a debug layout in the base game uses it). No font is imported.
	static const ResourceName FONT_MONO = "{0E041C5B1F27DCEA}ui/fonts/robotomono_msdf_28.fnt";

	static const float PAD_LEFT = 28;
	static const float PAD_TOP = 10;
	static const float PAD_RIGHT = 28;
	static const float PAD_BOTTOM = 10;

	//------------------------------------------------------------------------------------------------
	//! The 1989 theme with this id, in code: the fallback when the config does not load, a field a
	//! pack removes, and the reference LF_Test_UIThemePack checks the core file against.
	static LF_UIThemeRecord BuiltIn(string id)
	{
		LF_UIThemeRecord record = new LF_UIThemeRecord();
		record.m_sThemeId = id;
		if (id == "id-card")
		{
			// A laminated identity card: pale registry paper with a faint green cast, a slate-blue rim
			// and state-blue labels (the civil registry's ink).
			record.m_sMetal = "#34465C";
			record.m_sPaper = "#E4E9DA";
			record.m_sRow = "#C4CCBE";
			record.m_sRule = "#284276";
			record.m_sButtonFocus = "#284276";
			record.m_sAccent = "#284276";
			return record;
		}

		if (id == "terminal")
		{
			// Green phosphor on a black tube in a beige-grey 1989 cash machine bezel. Focus is amber.
			record.m_sMetal = "#969284";
			record.m_sPaper = "#060E08";
			record.m_sInk = "#60EC78";
			record.m_sRule = "#226E34";
			record.m_sRow = "#0E2212";
			record.m_sButtonText = "#60EC78";
			record.m_sButtonFocus = "#FFB000";
			record.m_Font = FONT_MONO;
			return record;
		}

		if (id == "price-list")
		{
			// The shop's typed price list: the paper sheet in a mono face.
			record.m_Font = FONT_MONO;
			return record;
		}

		// "paper": the base every theme inherits from. Blued steel rim, warm opaque form paper,
		// carbon ink, a dull brass rule (not a highlight), the world behind dimmed to about a third,
		// a roster strip a step darker than the sheet.
		record.m_sThemeId = LF_UIThemeConfig.BASE_ID;
		record.m_sMetal = "#3E403A";
		record.m_sPaper = "#EEE6D4";
		record.m_sInk = "#1C1A16";
		record.m_sRule = "#7A6640";
		record.m_sBlur = "#A8A49C";
		record.m_sRow = "#DCD0B6";
		record.m_sButtonText = "#1C1A16";
		record.m_sButtonFocus = "#96281E";
		record.m_Font = FONT;
		record.m_FontRegular = FONT_REGULAR;
		record.m_iTitleSize = 28;
		record.m_iBodySize = 18;
		record.m_iButtonSize = 20;
		record.m_iFieldSize = 18;
		// The printed name above a blank on a form.
		record.m_iCaptionSize = 16;
		// A printed notice's section label (small, upper case, in the accent colour).
		record.m_iHeadingSize = 15;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! The base theme: the config's "paper" record with its empty fields filled from the built-in.
	static LF_UIThemeRecord Base()
	{
		LF_UIThemeRecord builtIn = BuiltIn(LF_UIThemeConfig.BASE_ID);
		LF_UIThemeConfig config = LF_UIThemeConfig.Get();
		if (!config)
			return builtIn;

		LF_UIThemeRecord paper = config.Find(LF_UIThemeConfig.BASE_ID);
		if (!paper)
			return builtIn;

		return paper.Inherit(builtIn);
	}

	//------------------------------------------------------------------------------------------------
	//! The theme with this id, every empty field filled from the base theme. An id the config does
	//! not list (a pack removed it) uses the built-in theme of that id.
	static LF_UIThemeRecord Get(string id)
	{
		LF_UIThemeRecord base = Base();
		if (id == LF_UIThemeConfig.BASE_ID)
			return base;

		LF_UIThemeRecord record;
		LF_UIThemeConfig config = LF_UIThemeConfig.Get();
		if (config)
			record = config.Find(id);
		if (!record)
			record = BuiltIn(id);

		return record.Inherit(base);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached theme config so the next sheet is painted from the core file layered with
	//! the packs again (tests, and a server /configreload).
	static void Reload()
	{
		LF_UIThemeConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! The colour of a theme's hex string; a malformed one (logged when the config loads) takes the
	//! built-in paper theme's colour for the same token, so a typo in a pack never paints a sheet black.
	//! \param hex The theme's value, already inherited (never empty).
	//! \param builtIn The built-in paper theme's hex for the same token.
	static Color Token(string hex, string builtIn)
	{
		Color color = LF_UIThemeRecord.ParseColor(hex, null);
		if (color)
			return color;

		return LF_UIThemeRecord.ParseColor(builtIn, Color.FromSRGBA(0, 0, 0, 255));
	}

	//------------------------------------------------------------------------------------------------
	//! The base theme's colours: rim, sheet face, text, rule, backdrop tint and list row strip.
	static Color Metal()
	{
		return Token(Base().m_sMetal, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sMetal);
	}

	//------------------------------------------------------------------------------------------------
	static Color Paper()
	{
		return Token(Base().m_sPaper, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sPaper);
	}

	//------------------------------------------------------------------------------------------------
	static Color Ink()
	{
		return Token(Base().m_sInk, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sInk);
	}

	//------------------------------------------------------------------------------------------------
	static Color Rule()
	{
		return Token(Base().m_sRule, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sRule);
	}

	//------------------------------------------------------------------------------------------------
	static Color Blur()
	{
		return Token(Base().m_sBlur, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sBlur);
	}

	//------------------------------------------------------------------------------------------------
	static Color Row()
	{
		return Token(Base().m_sRow, BuiltIn(LF_UIThemeConfig.BASE_ID).m_sRow);
	}

	//------------------------------------------------------------------------------------------------
	//! The type sizes of the base theme, in reference pixels.
	static int TitleSize()
	{
		return Base().m_iTitleSize;
	}

	//------------------------------------------------------------------------------------------------
	static int BodySize()
	{
		return Base().m_iBodySize;
	}

	//------------------------------------------------------------------------------------------------
	static int ButtonSize()
	{
		return Base().m_iButtonSize;
	}

	//------------------------------------------------------------------------------------------------
	static int FieldSize()
	{
		return Base().m_iFieldSize;
	}

	//------------------------------------------------------------------------------------------------
	//! The printed name above a blank on a form.
	static int CaptionSize()
	{
		return Base().m_iCaptionSize;
	}

	//------------------------------------------------------------------------------------------------
	//! A printed notice's section label.
	static int HeadingSize()
	{
		return Base().m_iHeadingSize;
	}

	//------------------------------------------------------------------------------------------------
	//! The print face and the regular face of the base theme.
	static ResourceName Font()
	{
		return Base().m_Font;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName FontRegular()
	{
		return Base().m_FontRegular;
	}
}
