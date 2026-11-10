// LF_SignCatalog.c — sign text from Configs/Signs/LF_Signs.conf.
//
// A field that starts with # is a localization key. Anything else is shown as
// written, so a server owner can retitle a sign in config without touching the
// string table. An empty section is left off the card.

//------------------------------------------------------------------------------------------------
//! One sign. LF_SignComponent.m_sSignId names it.
[BaseContainerProps()]
class LF_SignDef
{
	[Attribute("", UIWidgets.EditBox, "Sign id, matched by LF_SignComponent.m_sSignId")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Title. Shown in the prompt as Read: <title>")]
	string m_sTitle;

	[Attribute("", UIWidgets.EditBox, "What this place is")]
	string m_sWhat;

	[Attribute("", UIWidgets.EditBox, "How to use it, with the exact action names")]
	string m_sHow;

	[Attribute("", UIWidgets.EditBox, "Items, faction, or people it needs")]
	string m_sNeeds;

	[Attribute("", UIWidgets.EditBox, "Cooldowns, wanted level, other rules")]
	string m_sRules;

	[Attribute("", UIWidgets.EditBox, "Where to go after this place, so a stranger can follow the loop")]
	string m_sNext;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SignConfig
{
	[Attribute("", UIWidgets.Object, "Signs")]
	ref array<ref LF_SignDef> m_aSigns;
}

//------------------------------------------------------------------------------------------------
class LF_SignCatalog
{
	static const ResourceName CONFIG_PATH = "{FFBF3D8E308E7E7E}Configs/Signs/LF_Signs.conf";

	static const string KEY_READ = "#LF-Sign_Read";
	static const string KEY_WHAT = "#LF-Sign_What";
	static const string KEY_HOW = "#LF-Sign_How";
	static const string KEY_NEEDS = "#LF-Sign_Needs";
	static const string KEY_RULES = "#LF-Sign_Rules";
	static const string KEY_NEXT = "#LF-Sign_Next";
	static const string KEY_CLOSE = "#LF-Sign_Close";

	//! Mark before a list item: a typed hyphen, which every UI font has. (U+2022 is a glyph the bitmap faces may lack.)
	static const string BULLET = "-";
	//! A card with no painted face prints its labels in oxblood.
	static const int DEFAULT_ACCENT_R = 118;
	static const int DEFAULT_ACCENT_G = 38;
	static const int DEFAULT_ACCENT_B = 28;
	//! Labels are small, so the accent is never lighter than this (0..1 luminance).
	static const float MAX_ACCENT_LUMINANCE = 0.3;
	//! The tallest card that fits the 920 high sheet under its title bar and above the sheet's foot.
	static const float MAX_CARD_HEIGHT = 800;
	static const float MIN_CARD_SCALE = 0.7;

	protected static ref LF_SignConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! Loads a sign config. Logs and returns null when it does not resolve.
	static LF_SignConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("Signs", "sign config does not resolve: " + path);
			return null;
		}

		LF_SignConfig config = LF_SignConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Signs", "sign config has an unexpected root class: " + path);

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (s_bLoadAttempted)
			return;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
	}

	//------------------------------------------------------------------------------------------------
	//! The sign with this id in LF_Signs.conf, or null.
	static LF_SignDef Find(string id)
	{
		EnsureLoaded();
		return FindIn(s_Config, id);
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignDef FindIn(LF_SignConfig config, string id)
	{
		if (id.IsEmpty() || !config || !config.m_aSigns)
			return null;

		foreach (LF_SignDef sign : config.m_aSigns)
		{
			if (sign && sign.m_sId == id)
				return sign;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A sign is usable when it has a title. A sign with no title would read "Read: ".
	static bool IsUsable(LF_SignDef sign)
	{
		return sign && !sign.m_sTitle.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsKey(string value)
	{
		return value.Length() > 1 && value.Get(0) == "#";
	}

	//------------------------------------------------------------------------------------------------
	//! A #key is translated. Anything else is returned as written.
	static string Resolve(string value)
	{
		if (!IsKey(value))
			return value;

		return WidgetManager.Translate(value);
	}

	//------------------------------------------------------------------------------------------------
	//! The prompt text: Read: <title>.
	static string ReadName(LF_SignDef sign)
	{
		if (!sign)
			return string.Empty;

		return WidgetManager.Translate(KEY_READ, Resolve(sign.m_sTitle));
	}

	//------------------------------------------------------------------------------------------------
	//! The card skin: the notice sheet, framed in the sign's paint colour when the sign has a painted
	//! face, with the darkest of paint, lettering and rim as the accent for labels and the rule.
	//! An unpainted sign (the cork board, the paper notice) gets the oxblood accent and the steel frame.
	static LF_UISkin CardSkin(bool painted, vector rim, vector paint, vector lettering)
	{
		if (!painted)
			return LF_UISkin.Notice(null, DEFAULT_ACCENT_R, DEFAULT_ACCENT_G, DEFAULT_ACCENT_B);

		vector accent = DarkestOf(paint, lettering, rim);
		int r = Math.Round(accent[0] * 255);
		int g = Math.Round(accent[1] * 255);
		int b = Math.Round(accent[2] * 255);
		int pr = Math.Round(paint[0] * 255);
		int pg = Math.Round(paint[1] * 255);
		int pb = Math.Round(paint[2] * 255);
		return LF_UISkin.Notice(Color.FromSRGBA(pr, pg, pb, 255), r, g, b);
	}

	//------------------------------------------------------------------------------------------------
	protected static float Luminance(vector color)
	{
		return 0.299 * color[0] + 0.587 * color[1] + 0.114 * color[2];
	}

	//------------------------------------------------------------------------------------------------
	//! The darkest of three RGB (0..1) colours, pulled down to at most MAX_ACCENT_LUMINANCE so
	//! small label text stays readable on the paper.
	static vector DarkestOf(vector a, vector b, vector c)
	{
		vector darkest = a;
		if (Luminance(b) < Luminance(darkest))
			darkest = b;

		if (Luminance(c) < Luminance(darkest))
			darkest = c;

		float lum = Luminance(darkest);
		if (lum > MAX_ACCENT_LUMINANCE)
			darkest = darkest * (MAX_ACCENT_LUMINANCE / lum);

		return darkest;
	}

	//------------------------------------------------------------------------------------------------
	//! Splits a section into list items. A newline or | separates items; a leading "- " marks one.
	//! \return True when the text is a list (more than one item, or an item marked with "- ").
	static bool ParseItems(string raw, out array<string> items)
	{
		items = {};
		string text = raw;
		text.Replace("\r", "");
		text.Replace("|", "\n");
		array<string> parts = {};
		text.Split("\n", parts, true);
		bool marked;
		foreach (string part : parts)
		{
			string item = part;
			item.TrimInPlace();
			if (item.StartsWith("- "))
			{
				marked = true;
				item = item.Substring(2, item.Length() - 2);
				item.TrimInPlace();
			}

			if (!item.IsEmpty())
				items.Insert(item);
		}

		return marked || items.Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	//! RichTextWidget markup for one item. *word* becomes the accent colour (no accent: the stars are
	//! dropped); a stray < or > is removed so text can never open a tag. A lone * stays as written.
	static string Highlight(string item, string accentRgba)
	{
		string text = item;
		text.Replace("<", "");
		text.Replace(">", "");

		string result;
		int length = text.Length();
		int i;
		while (i < length)
		{
			string ch = text.Get(i);
			if (ch != "*")
			{
				result += ch;
				i++;
				continue;
			}

			int close = -1;
			for (int j = i + 1; j < length; j++)
			{
				if (text.Get(j) == "*")
				{
					close = j;
					break;
				}
			}

			if (close < 0 || close == i + 1)
			{
				result += ch;
				i++;
				continue;
			}

			string word = text.Substring(i + 1, close - i - 1);
			if (accentRgba.IsEmpty())
				result += word;
			else
				result += "<color rgba='" + accentRgba + "'>" + word + "</color>";

			i = close + 1;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! A section body as RichTextWidget markup: a bullet per item, or the plain paragraph.
	static string RichBody(string raw, string accentRgba)
	{
		array<string> items;
		bool isList = ParseItems(raw, items);
		string body;
		foreach (int i, string item : items)
		{
			if (i > 0)
				body += "<br/>";

			if (isList)
				body += BULLET + " ";

			body += Highlight(item, accentRgba);
		}

		return body;
	}

	//------------------------------------------------------------------------------------------------
	//! Roughly how tall a card is at a type scale, in reference pixels (the sheet is 920 high with 104
	//! of frame, so a card needs to stay under MAX_CARD_HEIGHT). Markup tags do not count as text.
	static float EstimateHeight(array<ref LF_UIPart> parts, float sheetWidth, float scale)
	{
		float fontBody = LF_UITheme.FONT_BODY * scale;
		float pitch = fontBody * 1.35;
		float perLine = (sheetWidth - 56) / (fontBody * 0.46);
		float height = 40;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == LF_EUIPartKind.TITLE)
			{
				float fontTitle = LF_UITheme.FONT_TITLE * 1.35 * scale;
				int titleLines = 1;
				if (part.GetText().Length() * fontTitle * 0.48 > sheetWidth - 56)
					titleLines = 2;

				height += titleLines * fontTitle * 1.3 + 20;
			}
			else if (part.GetKind() == LF_EUIPartKind.RULE)
			{
				height += 14;
			}
			else if (part.GetKind() == LF_EUIPartKind.HEADING)
			{
				height += 18 + LF_UITheme.FONT_HEADING * scale * 1.3;
			}
			else if (part.GetKind() == LF_EUIPartKind.RICH)
			{
				array<string> lines = {};
				part.GetText().Split("<br/>", lines, true);
				foreach (string line : lines)
				{
					height += pitch * Math.Max(1, Math.Ceil(VisibleLength(line) / perLine));
				}

				height += 6;
			}
			else if (part.GetKind() == LF_EUIPartKind.CLOSE)
			{
				height += 72;
			}
		}

		return height;
	}

	//------------------------------------------------------------------------------------------------
	//! Characters of a markup string that print: everything outside <...>.
	protected static int VisibleLength(string markup)
	{
		int count;
		bool inTag;
		for (int i = 0; i < markup.Length(); i++)
		{
			string ch = markup.Get(i);
			if (ch == "<")
				inTag = true;
			else if (ch == ">")
				inTag = false;
			else if (!inTag)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The largest type scale (1, then down in steps to MIN_CARD_SCALE) at which the card fits the sheet.
	static float FitScale(array<ref LF_UIPart> parts, float sheetWidth)
	{
		float scale = 1;
		while (scale > MIN_CARD_SCALE + 0.01 && EstimateHeight(parts, sheetWidth, scale) > MAX_CARD_HEIGHT)
		{
			scale -= 0.05;
		}

		return scale;
	}

	//------------------------------------------------------------------------------------------------
	//! Title, a rule, then for each filled section a label and its body in the order What, How, Needs,
	//! Rules, Next, then Close. A section part is named for the section (the label) and Body (the text).
	static array<ref LF_UIPart> BuildParts(LF_SignDef sign, LF_UISkin skin = null)
	{
		ref array<ref LF_UIPart> parts = {};
		if (!sign)
			return parts;

		string accent;
		if (skin)
			accent = skin.m_sAccentRgba;

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", Resolve(sign.m_sTitle)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "TitleRule", ""));
		AddSection(parts, "What", KEY_WHAT, sign.m_sWhat, accent);
		AddSection(parts, "How", KEY_HOW, sign.m_sHow, accent);
		AddSection(parts, "Needs", KEY_NEEDS, sign.m_sNeeds, accent);
		AddSection(parts, "Rules", KEY_RULES, sign.m_sRules, accent);
		AddSection(parts, "Next", KEY_NEXT, sign.m_sNext, accent);
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", KEY_CLOSE));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddSection(array<ref LF_UIPart> parts, string name, string headingKey, string value, string accentRgba)
	{
		string body = RichBody(Resolve(value), accentRgba);
		body.TrimInPlace();
		if (body.IsEmpty())
			return;

		string heading = Resolve(headingKey);
		heading.ToUpper();
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, name, heading));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, name + "Body", body));
	}
}
