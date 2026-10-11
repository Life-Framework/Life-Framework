// LF_UIPart.c — one line in a screen assembled from the shared part layouts.

enum LF_EUIPartKind
{
	TITLE,
	LINE,
	FIELD,
	ROW,
	BUTTON,
	CLOSE
}

class LF_UIPart
{
	protected LF_EUIPartKind m_eKind;
	protected string m_sName;
	protected string m_sText;

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

		return string.Empty;
	}
}
