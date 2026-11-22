// LF_UIPart.c — one line in a screen assembled from the shared part layouts.

enum LF_EUIPartKind
{
	TITLE,
	LINE,
	FIELD,
	ROW,
	BUTTON,
	CLOSE,
	STATUS,
	QUANTITY,
	LIST,
	//! A row of buttons side by side (a keypad row, a till's keys). MenuLeft/MenuRight move along it.
	KEYS,
	//! A small upper-case section label in the skin's accent colour (a printed notice).
	HEADING,
	//! A thin rule across the sheet in the skin's accent colour.
	RULE,
	//! Body text that reads RichTextWidget markup (<color>, <br/>). The text is markup, not plain.
	RICH,
	//! A picture: text = imageset resource, status = image name in it (tools/cli ui-image makes both).
	IMAGE,
	//! A photo of a person (head and shoulders, drawn by the feature into LF_UIScreen.GetPortrait)
	//! with an optional stamp over its corner and up to LF_UIScreen.PORTRAIT_LINES label and value
	//! lines beside it: the face of an identity card. text = stamp imageset, status = its image name.
	PORTRAIT,
	//! A thin progress bar (an XP bar): the filled share in the skin's accent, the rest in its row colour.
	//! LF_UIScreen.SetBar sets how full it is; it starts empty.
	BAR,
	//! Side-by-side columns (a wide sheet). Parts flagged InColumn(name, i) stack inside column i;
	//! the columns share the sheet's width by their weights.
	COLUMNS,
	//! A label on the left and its value set flush right, with an optional small badge between them
	//! (a ledger line: "Cash in hand ... $20"). text = label, status = value, action = badge.
	PAIR
}

//! A badge's colour. The kit takes each from the theme: PLAIN the row strip, BLUE the id-card
//! theme's state blue, RED the sheet's focus red; a toned badge prints its text in the paper colour.
enum LF_EUIBadgeTone
{
	PLAIN,
	BLUE,
	RED
}

class LF_UIPart
{
	protected LF_EUIPartKind m_eKind;
	protected string m_sName;
	protected string m_sText;
	protected string m_sStatus;
	protected string m_sAction;
	protected int m_iTotal;
	protected int m_iTake;
	//! Sits in the sheet's scrolling body, not the pinned header or footer (#3036).
	protected bool m_bInBody;
	//! LIST: owning list of a row part, or the row names of a list part, in display order.
	protected string m_sList;
	protected ref array<string> m_aRows = {};
	//! KEYS: one button per key, named and labelled in order.
	protected ref array<string> m_aKeyNames = {};
	protected ref array<string> m_aKeyTexts = {};
	//! PORTRAIT: the printed lines beside the photo, a small label over each value.
	protected ref array<string> m_aLabels = {};
	protected ref array<string> m_aValues = {};
	//! KEYS: the key-cap printed before each key ("Esc"), empty for none.
	protected ref array<string> m_aKeyCaps = {};
	//! COLUMNS: the share of the width each column takes.
	protected ref array<float> m_aWeights = {};
	//! The COLUMNS part this part sits in, and the column index; empty for the sheet itself.
	protected string m_sColumns;
	protected int m_iColumn;
	//! LIST: short rows with the status in a badge (LF_UICompactRow.layout), for a list in a column.
	//! PAIR: the label is printed large (a header line).
	protected bool m_bCompact;
	//! PAIR: an icon of the kit set (LF_UITheme.ICONS_IMAGESET) before the label; empty for none.
	protected string m_sIcon;
	//! PAIR: the badge's colour (LF_EUIBadgeTone).
	protected LF_EUIBadgeTone m_eTone;
	//! COLUMNS: each column is a card on a plate.
	protected bool m_bCards;
	//! PORTRAIT: a sized photo window showing the whole figure; 0 keeps the head-and-shoulders card.
	protected float m_fBoxWidth;
	protected float m_fBoxHeight;

	//------------------------------------------------------------------------------------------------
	static LF_UIPart Create(LF_EUIPartKind kind, string name, string text)
	{
		LF_UIPart part = new LF_UIPart();
		part.m_eKind = kind;
		part.m_sName = name;
		part.m_sText = text;
		return part;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIPart CreateImage(string name, ResourceName imageset, string imageName)
	{
		LF_UIPart part = Create(LF_EUIPartKind.IMAGE, name, imageset);
		part.m_sStatus = imageName;
		return part;
	}

	//------------------------------------------------------------------------------------------------
	//! A portrait with the stamp image over its corner (empty imageset: no stamp) and one label and
	//! value per line beside it, at most LF_UIScreen.PORTRAIT_LINES.
	static LF_UIPart CreatePortrait(string name, ResourceName stampImageset, string stampImage, array<string> labels, array<string> values)
	{
		LF_UIPart part = Create(LF_EUIPartKind.PORTRAIT, name, stampImageset);
		part.m_sStatus = stampImage;
		if (!labels || !values || labels.Count() != values.Count())
		{
			LF_Debug.Error("UIKit", "portrait " + name + " needs one value per label");
			return part;
		}

		part.m_aLabels.Copy(labels);
		part.m_aValues.Copy(values);
		return part;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetLabels()
	{
		return m_aLabels;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetValues()
	{
		return m_aValues;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIPart CreateStatus(string name, string label, string status, string action)
	{
		LF_UIPart part = Create(LF_EUIPartKind.STATUS, name, label);
		part.m_sStatus = status;
		part.m_sAction = action;
		return part;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIPart CreateQuantity(string name, string label, int total, int take)
	{
		LF_UIPart part = Create(LF_EUIPartKind.QUANTITY, name, label);
		part.m_iTotal = total;
		part.m_iTake = take;
		return part;
	}

	//------------------------------------------------------------------------------------------------
	//! A scrolling list. Rows are added after Open with LF_UIScreen.AddListRow.
	static LF_UIPart CreateList(string name)
	{
		return Create(LF_EUIPartKind.LIST, name, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! A row of buttons. Each key reports its own name through LF_UIScreen.GetOnButton.
	//! Key names are unique across the screen; names ending in "Button" are clickable by the harness.
	static LF_UIPart CreateKeys(string name, array<string> keyNames, array<string> keyTexts)
	{
		LF_UIPart part = Create(LF_EUIPartKind.KEYS, name, string.Empty);
		if (!keyNames || !keyTexts || keyNames.Count() != keyTexts.Count())
		{
			LF_Debug.Error("UIKit", "keys " + name + " need one text per key name");
			return part;
		}

		part.m_aKeyNames.Copy(keyNames);
		part.m_aKeyTexts.Copy(keyTexts);
		return part;
	}

	//------------------------------------------------------------------------------------------------
	//! Prints a key-cap chip ("Esc", "F3") before each key of a KEYS row; an empty cap prints none.
	LF_UIPart WithCaps(array<string> caps)
	{
		if (!caps || caps.Count() != m_aKeyNames.Count())
		{
			LF_Debug.Error("UIKit", "keys " + m_sName + " need one cap per key");
			return this;
		}

		m_aKeyCaps.Copy(caps);
		return this;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetKeyCaps()
	{
		return m_aKeyCaps;
	}

	//------------------------------------------------------------------------------------------------
	//! Columns side by side, one per weight; parts go in with InColumn(name, index).
	static LF_UIPart CreateColumns(string name, array<float> weights)
	{
		LF_UIPart part = Create(LF_EUIPartKind.COLUMNS, name, string.Empty);
		if (!weights || weights.IsEmpty())
		{
			LF_Debug.Error("UIKit", "columns " + name + " need at least one weight");
			part.m_aWeights.Insert(1);
			return part;
		}

		part.m_aWeights.Copy(weights);
		return part;
	}

	//------------------------------------------------------------------------------------------------
	array<float> GetWeights()
	{
		return m_aWeights;
	}

	//------------------------------------------------------------------------------------------------
	//! A label and its value flush right; a non-empty badge prints as a small chip between them.
	static LF_UIPart CreatePair(string name, string label, string value, string badge = "")
	{
		LF_UIPart part = Create(LF_EUIPartKind.PAIR, name, label);
		part.m_sStatus = value;
		part.m_sAction = badge;
		return part;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the part in column `index` of the COLUMNS part `columns` (added earlier on the same sheet).
	LF_UIPart InColumn(string columns, int index)
	{
		m_sColumns = columns;
		m_iColumn = index;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	string GetColumns()
	{
		return m_sColumns;
	}

	//------------------------------------------------------------------------------------------------
	int GetColumn()
	{
		return m_iColumn;
	}

	//------------------------------------------------------------------------------------------------
	//! PAIR: prints the kit icon `icon` (a quad of LF_UITheme.ICONS_IMAGESET) before the label.
	LF_UIPart WithIcon(string icon)
	{
		m_sIcon = icon;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	string GetIcon()
	{
		return m_sIcon;
	}

	//------------------------------------------------------------------------------------------------
	//! PAIR: the badge's colour.
	LF_UIPart Tone(LF_EUIBadgeTone tone)
	{
		m_eTone = tone;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	LF_EUIBadgeTone GetTone()
	{
		return m_eTone;
	}

	//------------------------------------------------------------------------------------------------
	//! COLUMNS: each column is a card, its parts on a plate a step darker than the sheet.
	LF_UIPart Cards()
	{
		m_bCards = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	bool HasCards()
	{
		return m_bCards;
	}

	//------------------------------------------------------------------------------------------------
	//! PORTRAIT: a photo window of this size showing the whole figure (a character preview).
	LF_UIPart Sized(float width, float height)
	{
		m_fBoxWidth = width;
		m_fBoxHeight = height;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	float GetBoxWidth()
	{
		return m_fBoxWidth;
	}

	//------------------------------------------------------------------------------------------------
	float GetBoxHeight()
	{
		return m_fBoxHeight;
	}

	//------------------------------------------------------------------------------------------------
	//! LIST: short rows with the status in a badge. PAIR: a large label (a header line).
	LF_UIPart Compact()
	{
		m_bCompact = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	bool IsCompact()
	{
		return m_bCompact;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetKeyNames()
	{
		return m_aKeyNames;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetKeyTexts()
	{
		return m_aKeyTexts;
	}

	//------------------------------------------------------------------------------------------------
	string GetList()
	{
		return m_sList;
	}

	//------------------------------------------------------------------------------------------------
	void SetList(string list)
	{
		m_sList = list;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetRows()
	{
		return m_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the part in the sheet's scrolling body (#3036). Parts before the first scrolling part (the
	//! title) and after the last (the action rows) stay pinned; a sheet at the screen cap scrolls the
	//! body instead of cutting the pinned rows off. Flag one contiguous run of parts.
	LF_UIPart InBody()
	{
		m_bInBody = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInBody()
	{
		return m_bInBody;
	}

	//------------------------------------------------------------------------------------------------
	LF_EUIPartKind GetKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetText()
	{
		return m_sText;
	}

	//------------------------------------------------------------------------------------------------
	void SetText(string text)
	{
		m_sText = text;
	}

	//------------------------------------------------------------------------------------------------
	string GetStatus()
	{
		return m_sStatus;
	}

	//------------------------------------------------------------------------------------------------
	void SetStatus(string status)
	{
		m_sStatus = status;
	}

	//------------------------------------------------------------------------------------------------
	string GetAction()
	{
		return m_sAction;
	}

	//------------------------------------------------------------------------------------------------
	void SetAction(string action)
	{
		m_sAction = action;
	}

	//------------------------------------------------------------------------------------------------
	int GetTotal()
	{
		return m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	void SetTotal(int total)
	{
		m_iTotal = total;
	}

	//------------------------------------------------------------------------------------------------
	int GetTake()
	{
		return m_iTake;
	}

	//------------------------------------------------------------------------------------------------
	void SetTake(int take)
	{
		m_iTake = take;
	}

	//------------------------------------------------------------------------------------------------
	static string LayoutFor(LF_EUIPartKind kind)
	{
		if (kind == LF_EUIPartKind.TITLE)
			return LF_UITheme.TITLE_LAYOUT;

		if (kind == LF_EUIPartKind.LINE)
			return LF_UITheme.LINE_LAYOUT;

		if (kind == LF_EUIPartKind.FIELD)
			return LF_UITheme.EDIT_LAYOUT;

		if (kind == LF_EUIPartKind.ROW)
			return LF_UITheme.ROW_LAYOUT;

		if (kind == LF_EUIPartKind.BUTTON)
			return LF_UITheme.BUTTON_LAYOUT;

		if (kind == LF_EUIPartKind.CLOSE)
			return LF_UITheme.BUTTON_LAYOUT;

		if (kind == LF_EUIPartKind.STATUS)
			return LF_UITheme.STATUS_LAYOUT;

		if (kind == LF_EUIPartKind.QUANTITY)
			return LF_UITheme.QUANTITY_LAYOUT;

		if (kind == LF_EUIPartKind.LIST)
			return LF_UITheme.LIST_LAYOUT;

		if (kind == LF_EUIPartKind.KEYS)
			return LF_UITheme.KEYS_LAYOUT;

		if (kind == LF_EUIPartKind.HEADING)
			return LF_UITheme.HEADING_LAYOUT;

		if (kind == LF_EUIPartKind.RULE)
			return LF_UITheme.RULE_LAYOUT;

		if (kind == LF_EUIPartKind.RICH)
			return LF_UITheme.RICH_LAYOUT;
		if (kind == LF_EUIPartKind.IMAGE)
			return LF_UITheme.IMAGE_LAYOUT;

		if (kind == LF_EUIPartKind.PORTRAIT)
			return LF_UITheme.PORTRAIT_LAYOUT;

		if (kind == LF_EUIPartKind.BAR)
			return LF_UITheme.BAR_LAYOUT;

		if (kind == LF_EUIPartKind.COLUMNS)
			return LF_UITheme.COLUMNS_LAYOUT;

		if (kind == LF_EUIPartKind.PAIR)
			return LF_UITheme.PAIR_LAYOUT;

		return string.Empty;
	}
}
