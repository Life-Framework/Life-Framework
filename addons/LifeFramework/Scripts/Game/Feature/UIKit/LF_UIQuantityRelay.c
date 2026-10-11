// LF_UIQuantityRelay.c — the slider on a quantity part tells the screen the new take.
class LF_UIQuantityRelay : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected ref LF_UIScreen m_Screen;
	protected string m_sName;
	protected bool m_bWired;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_wRoot = w;
	}

	//------------------------------------------------------------------------------------------------
	void Bind(LF_UIScreen screen, string name)
	{
		m_Screen = screen;
		m_sName = name;

		if (m_bWired)
			return;

		if (!m_wRoot)
		{
			EL_Debug.Error("UIKit", "quantity " + name + " has no widget");
			return;
		}

		Widget sliderRoot = m_wRoot.FindAnyWidget("QuantitySlider");
		if (!sliderRoot)
		{
			EL_Debug.Error("UIKit", "quantity " + name + " is missing the slider");
			return;
		}

		EL_ThinSliderWidget slider = EL_ThinSliderWidget.Cast(sliderRoot.FindHandler(EL_ThinSliderWidget));
		if (!slider)
		{
			EL_Debug.Error("UIKit", "quantity " + name + " slider has no handler");
			return;
		}

		slider.GetOnValueChanged().Insert(OnChanged);
		m_bWired = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChanged(float value, bool finished)
	{
		if (!m_Screen)
			return;

		int take = (int)value;
		m_Screen.NotifyQuantity(m_sName, take, finished);
	}
}
