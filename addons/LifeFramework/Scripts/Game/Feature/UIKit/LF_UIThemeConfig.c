//------------------------------------------------------------------------------------------------
//! One theme of the UI kit (#966): the colours, faces and sizes a sheet is made of. The id is the
//! skin it feeds: "paper" is the base every other theme inherits from, "id-card", "terminal" and
//! "price-list" are the sheets that look different from the base. A field a theme leaves empty (or
//! 0) takes the "paper" value, so a pack that patches "paper" restyles every sheet that inherits
//! it. Colours are hex strings ("#EEE6D4", or "#RRGGBBAA"), 0..255 sRGB bytes, so a pack author
//! reads what the player sees. Core ships Configs/UI/LF_UITheme.conf (the 1989 paper look); a
//! pack overlay of the same class changes it by id (docs/config/override-model.md).
[BaseContainerProps()]
class LF_UIThemeRecord : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Theme id: paper, id-card, terminal or price-list. Names the skin this theme feeds")]
	string m_sThemeId;

	[Attribute("", UIWidgets.EditBox, "Rim of the sheet, hex #RRGGBB. Empty takes the paper theme's")]
	string m_sMetal;

	[Attribute("", UIWidgets.EditBox, "The sheet's face, hex #RRGGBB. Opaque")]
	string m_sPaper;

	[Attribute("", UIWidgets.EditBox, "Text colour, hex #RRGGBB")]
	string m_sInk;

	[Attribute("", UIWidgets.EditBox, "The rule under a title, hex #RRGGBB")]
	string m_sRule;

	[Attribute("", UIWidgets.EditBox, "Tint of the dimmed world behind the sheet, hex #RRGGBB")]
	string m_sBlur;

	[Attribute("", UIWidgets.EditBox, "A step darker strip behind a list row, hex #RRGGBB")]
	string m_sRow;

	[Attribute("", UIWidgets.EditBox, "Button text, hex #RRGGBB. Empty takes the ink")]
	string m_sButtonText;

	[Attribute("", UIWidgets.EditBox, "Button text when hovered or focused, hex #RRGGBB")]
	string m_sButtonFocus;

	[Attribute("", UIWidgets.EditBox, "Section labels and highlighted words, hex #RRGGBB. Empty: the ink. Not inherited")]
	string m_sAccent;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Print face of titles and labels (a .fnt). Empty takes the paper theme's", params: "fnt")]
	ResourceName m_Font;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The regular face a notice's body is set in (a .fnt). Empty takes the paper theme's", params: "fnt")]
	ResourceName m_FontRegular;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Texture drawn on the sheet's face, tinted by the paper colour. Empty: a flat sheet. Not inherited", params: "edds")]
	ResourceName m_PaperTexture;

	[Attribute("0", UIWidgets.Auto, "Title size in reference pixels. 0 takes the paper theme's", params: "0 96")]
	int m_iTitleSize;

	[Attribute("0", UIWidgets.Auto, "Body text size. 0 takes the paper theme's", params: "0 96")]
	int m_iBodySize;

	[Attribute("0", UIWidgets.Auto, "Button label size. 0 takes the paper theme's", params: "0 96")]
	int m_iButtonSize;

	[Attribute("0", UIWidgets.Auto, "Size of the text typed into a blank. 0 takes the paper theme's", params: "0 96")]
	int m_iFieldSize;

	[Attribute("0", UIWidgets.Auto, "The printed name above a blank on a form. 0 takes the paper theme's", params: "0 96")]
	int m_iCaptionSize;

	[Attribute("0", UIWidgets.Auto, "A notice's section label (small, upper case, in the accent). 0 takes the paper theme's", params: "0 96")]
	int m_iHeadingSize;

	//------------------------------------------------------------------------------------------------
	//! The colour of a hex string, or `fallback` when it is empty or malformed (logged once per
	//! theme load by LF_UIThemeConfig.Check). "#RRGGBB" or "#RRGGBBAA", with or without the "#".
	static Color ParseColor(string hex, Color fallback)
	{
		int r, g, b, a;
		if (!ParseHex(hex, r, g, b, a))
			return fallback;

		return Color.FromSRGBA(r, g, b, a);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `hex` is "#RRGGBB" or "#RRGGBBAA" (the "#" optional); the bytes are out.
	static bool ParseHex(string hex, out int r, out int g, out int b, out int a)
	{
		a = 255;
		string text = hex;
		text.ToUpper();
		if (text.Length() > 0 && text.Substring(0, 1) == "#")
			text = text.Substring(1, text.Length() - 1);

		int length = text.Length();
		if (length != 6 && length != 8)
			return false;

		int values[4];
		values[3] = 255;
		int pairs = length / 2;
		for (int p = 0; p < pairs; p++)
		{
			int high = HexDigit(text.Substring(p * 2, 1));
			int low = HexDigit(text.Substring(p * 2 + 1, 1));
			if (high < 0 || low < 0)
				return false;

			values[p] = high * 16 + low;
		}

		r = values[0];
		g = values[1];
		b = values[2];
		a = values[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static int HexDigit(string digit)
	{
		return "0123456789ABCDEF".IndexOf(digit);
	}

	//------------------------------------------------------------------------------------------------
	//! The same hex string as "r,g,b,a" bytes, for RichTextWidget <color rgba='...'> markup, or empty.
	static string ToRgba(string hex)
	{
		int r, g, b, a;
		if (!ParseHex(hex, r, g, b, a))
			return string.Empty;

		return string.Format("%1,%2,%3,%4", r, g, b, a);
	}

	//------------------------------------------------------------------------------------------------
	//! This theme with every empty field filled from `base` (the paper theme). Texture and accent
	//! are this theme's own: a flat terminal does not inherit the paper's texture.
	LF_UIThemeRecord Inherit(LF_UIThemeRecord base)
	{
		LF_UIThemeRecord record = new LF_UIThemeRecord();
		record.m_sThemeId = m_sThemeId;
		record.m_sMetal = m_sMetal;
		record.m_sPaper = m_sPaper;
		record.m_sInk = m_sInk;
		record.m_sRule = m_sRule;
		record.m_sBlur = m_sBlur;
		record.m_sRow = m_sRow;
		record.m_sButtonText = m_sButtonText;
		record.m_sButtonFocus = m_sButtonFocus;
		record.m_sAccent = m_sAccent;
		record.m_Font = m_Font;
		record.m_FontRegular = m_FontRegular;
		record.m_PaperTexture = m_PaperTexture;
		record.m_iTitleSize = m_iTitleSize;
		record.m_iBodySize = m_iBodySize;
		record.m_iButtonSize = m_iButtonSize;
		record.m_iFieldSize = m_iFieldSize;
		record.m_iCaptionSize = m_iCaptionSize;
		record.m_iHeadingSize = m_iHeadingSize;
		if (!base || base == this)
			return record;

		if (record.m_sMetal.IsEmpty())
			record.m_sMetal = base.m_sMetal;
		if (record.m_sPaper.IsEmpty())
			record.m_sPaper = base.m_sPaper;
		if (record.m_sInk.IsEmpty())
			record.m_sInk = base.m_sInk;
		if (record.m_sRule.IsEmpty())
			record.m_sRule = base.m_sRule;
		if (record.m_sBlur.IsEmpty())
			record.m_sBlur = base.m_sBlur;
		if (record.m_sRow.IsEmpty())
			record.m_sRow = base.m_sRow;
		if (record.m_sButtonText.IsEmpty())
			record.m_sButtonText = base.m_sButtonText;
		if (record.m_sButtonFocus.IsEmpty())
			record.m_sButtonFocus = base.m_sButtonFocus;
		if (record.m_Font.IsEmpty())
			record.m_Font = base.m_Font;
		if (record.m_FontRegular.IsEmpty())
			record.m_FontRegular = base.m_FontRegular;
		if (record.m_iTitleSize <= 0)
			record.m_iTitleSize = base.m_iTitleSize;
		if (record.m_iBodySize <= 0)
			record.m_iBodySize = base.m_iBodySize;
		if (record.m_iButtonSize <= 0)
			record.m_iButtonSize = base.m_iButtonSize;
		if (record.m_iFieldSize <= 0)
			record.m_iFieldSize = base.m_iFieldSize;
		if (record.m_iCaptionSize <= 0)
			record.m_iCaptionSize = base.m_iCaptionSize;
		if (record.m_iHeadingSize <= 0)
			record.m_iHeadingSize = base.m_iHeadingSize;

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! The colour fields that are set but not a valid hex string, as "field=value" for the log.
	void CollectBadColors(notnull array<string> bad)
	{
		CheckColor("metal", m_sMetal, bad);
		CheckColor("paper", m_sPaper, bad);
		CheckColor("ink", m_sInk, bad);
		CheckColor("rule", m_sRule, bad);
		CheckColor("blur", m_sBlur, bad);
		CheckColor("row", m_sRow, bad);
		CheckColor("buttonText", m_sButtonText, bad);
		CheckColor("buttonFocus", m_sButtonFocus, bad);
		CheckColor("accent", m_sAccent, bad);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckColor(string field, string hex, notnull array<string> bad)
	{
		int r, g, b, a;
		if (!hex.IsEmpty() && !ParseHex(hex, r, g, b, a))
			bad.Insert(field + "=" + hex);
	}
}

//------------------------------------------------------------------------------------------------
//! The UI kit's theme: a list of themes by id, the core file layered with the pack overlays of the
//! same class in load order (LF_ConfigOverlay, #962). Client and server both read it; the pack
//! addon is on every client, so no stream is needed. A pack names a theme by id and gives each a
//! verb: `patch` "paper" to restyle the whole kit, `patch` "terminal" for the cash machine only.
//! There is no owner `config.json` section: a look is a pack's job, not a server owner's tweak.
[BaseContainerProps(configRoot: true)]
class LF_UIThemeConfig
{
	static const ResourceName CONFIG_PATH = "{B86AF8DC58DED1CA}Configs/UI/LF_UITheme.conf";

	static const string FEATURE = "UITheme";

	//! The theme every other one inherits from.
	static const string BASE_ID = "paper";

	[Attribute("", UIWidgets.Object, "Themes by id. A pack overlay patches, replaces, adds or removes one by m_sThemeId")]
	ref array<ref LF_UIThemeRecord> m_aThemes;

	protected static ref LF_UIThemeConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered theme, or null (logged once) when the core file does not resolve. Every
	//! reader falls back to the built-in 1989 values then, so a missing file never breaks a sheet.
	static LF_UIThemeConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("theme config does not resolve, sheets use the built-in 1989 look: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_UIThemeConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("theme config has an unexpected root class, sheets use the built-in 1989 look: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached theme so the next Get() loads the core file and layers it again. The next
	//! sheet that opens is painted from the new values.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over this instance (the core file), in load order.
	void ApplyLayers()
	{
		if (!m_aThemes)
			m_aThemes = new array<ref LF_UIThemeRecord>();

		LF_UIThemeList list = new LF_UIThemeList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_UIThemeConfig, "ui-theme", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aThemes", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 themes", names[i], changed));
		}

		foreach (LF_UIThemeRecord record : m_aThemes)
		{
			array<string> bad = {};
			if (record)
				record.CollectBadColors(bad);
			foreach (string field : bad)
				LF_Debug.Error(FEATURE, string.Format("theme %1: %2 is not a hex colour (#RRGGBB); the paper theme's colour is used", record.m_sThemeId, field));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The theme with this id, or null.
	LF_UIThemeRecord Find(string id)
	{
		if (!m_aThemes)
			return null;

		foreach (LF_UIThemeRecord record : m_aThemes)
		{
			if (record && record.m_sThemeId == id)
				return record;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The themes list for the shared merge (LF_ConfigOverlay), keyed by theme id.
class LF_UIThemeList : LF_ConfigList
{
	protected LF_UIThemeConfig m_Config;

	void LF_UIThemeList(LF_UIThemeConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "ui-theme.themes";
	}

	override int Count()
	{
		return m_Config.m_aThemes.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aThemes[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_UIThemeRecord.Cast(record).m_sThemeId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sThemeId", id);
		return id;
	}

	override string IdHint()
	{
		return "a theme id (paper, id-card, terminal, price-list)";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_UIThemeRecord record = new LF_UIThemeRecord();
		record.m_sThemeId = id;
		return record;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_UIThemeRecord theme = LF_UIThemeRecord.Cast(record);
		if (index < m_Config.m_aThemes.Count())
			m_Config.m_aThemes.Set(index, theme);
		else
			m_Config.m_aThemes.Insert(theme);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aThemes.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_UIThemeRecord theme = LF_UIThemeRecord.Cast(record);
		int changed;
		changed += PatchString(container, "m_sMetal", theme.m_sMetal);
		changed += PatchString(container, "m_sPaper", theme.m_sPaper);
		changed += PatchString(container, "m_sInk", theme.m_sInk);
		changed += PatchString(container, "m_sRule", theme.m_sRule);
		changed += PatchString(container, "m_sBlur", theme.m_sBlur);
		changed += PatchString(container, "m_sRow", theme.m_sRow);
		changed += PatchString(container, "m_sButtonText", theme.m_sButtonText);
		changed += PatchString(container, "m_sButtonFocus", theme.m_sButtonFocus);
		changed += PatchString(container, "m_sAccent", theme.m_sAccent);
		changed += PatchResource(container, "m_Font", theme.m_Font);
		changed += PatchResource(container, "m_FontRegular", theme.m_FontRegular);
		changed += PatchResource(container, "m_PaperTexture", theme.m_PaperTexture);
		changed += PatchInt(container, "m_iTitleSize", theme.m_iTitleSize);
		changed += PatchInt(container, "m_iBodySize", theme.m_iBodySize);
		changed += PatchInt(container, "m_iButtonSize", theme.m_iButtonSize);
		changed += PatchInt(container, "m_iFieldSize", theme.m_iFieldSize);
		changed += PatchInt(container, "m_iCaptionSize", theme.m_iCaptionSize);
		changed += PatchInt(container, "m_iHeadingSize", theme.m_iHeadingSize);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PatchString(BaseContainer container, string name, out string value)
	{
		if (container.IsVariableSetDirectly(name) && container.Get(name, value))
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PatchResource(BaseContainer container, string name, out ResourceName value)
	{
		if (container.IsVariableSetDirectly(name) && container.Get(name, value))
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PatchInt(BaseContainer container, string name, out int value)
	{
		if (container.IsVariableSetDirectly(name) && container.Get(name, value))
			return 1;

		return 0;
	}
}
