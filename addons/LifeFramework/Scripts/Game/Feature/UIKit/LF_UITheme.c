// LF_UITheme.c — the one 1989 sheet: type, padding, and colors.
//
// Paper and dull metal, carbon ink, a brass rule. No neon. Part layouts are
// structure and vanilla widget parents. LF_UIScreen paints every part from
// these values when it assembles a screen.
//
// The edit-box well is the exception the vanilla component reads at creation:
// LF_UIEditBox.layout m_BackgroundDefault is FIELD_WELL (214, 200, 170), written there in
// linear space (0.672 0.578 0.402) because the component animates the raw value.
// The sheet is solid ImageWidgets (no texture): a PanelWidget draws the engine's
// default-style panel image tinted by its colour, which reads as see-through.
// Blur strength (0.4) is authored on the full-screen blur widget in LF_UIFrame.layout;
// its colour multiplies the world behind the sheet, so it is the dim.
class LF_UITheme
{
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

	static const int FONT_TITLE = 28;
	static const int FONT_BODY = 18;
	static const int FONT_BUTTON = 20;
	static const int FONT_FIELD = 18;
	//! The printed name above a blank on a form.
	static const int FONT_CAPTION = 16;
	//! A printed notice's section label (small, upper case, in the accent colour).
	static const int FONT_HEADING = 15;
	//! The regular face a notice's body is set in, so the bold section labels stand out.
	static const ResourceName FONT_REGULAR = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";

	static const float PAD_LEFT = 28;
	static const float PAD_TOP = 10;
	static const float PAD_RIGHT = 28;
	static const float PAD_BOTTOM = 10;

	// Blued steel rim.
	static const int METAL_R = 62;
	static const int METAL_G = 64;
	static const int METAL_B = 58;
	static const int METAL_A = 255;

	// Warm off-white form paper. Opaque: nothing behind the sheet shows through.
	static const int PAPER_R = 238;
	static const int PAPER_G = 230;
	static const int PAPER_B = 212;
	static const int PAPER_A = 255;

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

	// The world behind the sheet, dimmed to about a third and softly blurred.
	static const int BLUR_R = 168;
	static const int BLUR_G = 164;
	static const int BLUR_B = 156;
	static const int BLUR_A = 255;

	// Roster strip, a step darker than the sheet.
	static const int ROW_R = 220;
	static const int ROW_G = 208;
	static const int ROW_B = 182;
	static const int ROW_A = 255;

	// Blank on the form, a step darker than the sheet. LF_UIEditBox.layout writes it in linear
	// space as 0.672 0.578 0.402 1 (focused and typing: 198, 182, 148 = 0.565 0.468 0.296).
	static const int FIELD_WELL_R = 214;
	static const int FIELD_WELL_G = 200;
	static const int FIELD_WELL_B = 170;
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
