// LF_UIButtonRelay.c — click target for one assembled pause-menu button.
// SCR_ButtonTextComponent.m_OnClicked is inserted here, on this component,
// after Bind. Close and ordinary buttons share the button layout.
class LF_UIButtonRelay : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected ref LF_UIScreen m_Screen;
	protected string m_sName;
	protected bool m_bClose;
	protected bool m_bWired;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_wRoot = w;
	}

	//------------------------------------------------------------------------------------------------
	void Bind(LF_UIScreen screen, string name, bool close)
	{
		m_Screen = screen;
		m_sName = name;
		m_bClose = close;

		if (m_bWired)
			return;

		if (!m_wRoot)
		{
			EL_Debug.Error("UIKit", "button " + name + " has no widget to wire");
			return;
		}

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.Cast(m_wRoot.FindHandler(SCR_ButtonTextComponent));
		if (!button)
		{
			EL_Debug.Error("UIKit", "button " + name + " is missing SCR_ButtonTextComponent");
			return;
		}

		button.m_OnClicked.Insert(HandleClick);
		m_bWired = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleClick()
	{
		if (!m_Screen)
		{
			EL_Debug.Error("UIKit", "button " + m_sName + " clicked with no screen");
			return;
		}

		if (m_bClose)
		{
			m_Screen.Close();
			return;
		}

		m_Screen.Notify(m_sName);
	}

	//------------------------------------------------------------------------------------------------
	void Activate()
	{
		HandleClick();
	}
}
