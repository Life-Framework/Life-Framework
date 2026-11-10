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
	RICH
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
	//! LIST: owning list of a row part, or the row names of a list part, in display order.
	protected string m_sList;
	protected ref array<string> m_aRows = {};
	//! KEYS: one button per key, named and labelled in order.
	protected ref array<string> m_aKeyNames = {};
	protected ref array<string> m_aKeyTexts = {};

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

		return string.Empty;
	}
}
