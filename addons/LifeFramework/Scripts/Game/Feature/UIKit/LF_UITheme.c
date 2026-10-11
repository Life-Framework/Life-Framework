// LF_UITheme.c — the one 1989 sheet: type, padding, and colors.
//
// Paper and dull metal, carbon ink, a brass rule. No neon. Part layouts are
// structure and vanilla widget parents. LF_UIScreen paints every part from
// these values when it assembles a screen.
//
// The edit-box well is the exception the vanilla component reads at creation:
// LF_UIEditBox.layout m_BackgroundDefault is FIELD_WELL (232, 220, 196).
// Blur strength (0.8) is authored on the blur widget in LF_UIFrame.layout.
class LF_UITheme
{
	static const ResourceName FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";

	static const string FRAME_LAYOUT = "{26EFA919E73EA3B1}UI/Layouts/UIKit/LF_UIFrame.layout";
	static const string TITLE_LAYOUT = "{1266676650F735E3}UI/Layouts/UIKit/LF_UITitle.layout";
	static const string LINE_LAYOUT = "{0A818180AD5E8749}UI/Layouts/UIKit/LF_UILine.layout";
	static const string BUTTON_LAYOUT = "{B90B99E1ADB16E16}UI/Layouts/UIKit/LF_UIButton.layout";
	static const string EDIT_LAYOUT = "{618CD52A98590696}UI/Layouts/UIKit/LF_UIEditBox.layout";
	static const string ROW_LAYOUT = "{635A66887A0CB6BF}UI/Layouts/UIKit/LF_UIListRow.layout";

	static const int FONT_TITLE = 28;
	static const int FONT_BODY = 18;
	static const int FONT_BUTTON = 20;
	static const int FONT_FIELD = 18;

	static const float PAD_LEFT = 28;
	static const float PAD_TOP = 10;
	static const float PAD_RIGHT = 28;
	static const float PAD_BOTTOM = 10;

	// Blued steel rim.
	static const int METAL_R = 62;
	static const int METAL_G = 64;
	static const int METAL_B = 58;
	static const int METAL_A = 255;

	// Aged paper. Slightly translucent so the blur sits behind the sheet.
	static const int PAPER_R = 214;
	static const int PAPER_G = 198;
	static const int PAPER_B = 168;
	static const int PAPER_A = 242;

	// Carbon ink.
	static const int INK_R = 28;
	static const int INK_G = 26;
	static const int INK_B = 22;
	static const int INK_A = 255;

	// Dull brass rule. Not a highlight.
	static const int RULE_R = 122;
	static const int RULE_G = 102;
	static const int RULE_B = 64;
	static const int RULE_A = 255;

	// Warm dark frost behind the metal.
	static const int BLUR_R = 48;
	static const int BLUR_G = 44;
	static const int BLUR_B = 36;
	static const int BLUR_A = 255;

	// Roster strip, a step darker than the sheet.
	static const int ROW_R = 186;
	static const int ROW_G = 170;
	static const int ROW_B = 140;
	static const int ROW_A = 255;

	// Blank on the form. LF_UIEditBox.layout writes these as 0.910 0.863 0.769 1.
	static const int FIELD_WELL_R = 232;
	static const int FIELD_WELL_G = 220;
	static const int FIELD_WELL_B = 196;
	static const int FIELD_WELL_A = 255;

	//------------------------------------------------------------------------------------------------
	static Color Metal()
	{
		return Color.FromSRGBA(METAL_R, METAL_G, METAL_B, METAL_A);
	}

	//------------------------------------------------------------------------------------------------
	static Color Paper()
	{
		return Color.FromSRGBA(PAPER_R, PAPER_G, PAPER_B, PAPER_A);
	}

	//------------------------------------------------------------------------------------------------
	static Color Ink()
	{
		return Color.FromSRGBA(INK_R, INK_G, INK_B, INK_A);
	}

	//------------------------------------------------------------------------------------------------
	static Color Rule()
	{
		return Color.FromSRGBA(RULE_R, RULE_G, RULE_B, RULE_A);
	}

	//------------------------------------------------------------------------------------------------
	static Color Blur()
	{
		return Color.FromSRGBA(BLUR_R, BLUR_G, BLUR_B, BLUR_A);
	}

	//------------------------------------------------------------------------------------------------
	static Color Row()
	{
		return Color.FromSRGBA(ROW_R, ROW_G, ROW_B, ROW_A);
	}
}
