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
	QUANTITY
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

		return string.Empty;
	}
}
