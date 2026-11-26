// LF_UIQuantityRelay.c — the slider on a quantity part tells the screen the new take.
class LF_UIQuantityRelay : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected ref LF_UIScreen m_Screen;
	protected string m_sName;
	protected bool m_bWired;
	protected LF_ThinSliderWidget m_Slider;

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
			LF_Debug.Error("UIKit", "quantity " + name + " has no widget");
			return;
		}

		Widget sliderRoot = m_wRoot.FindAnyWidget("QuantitySlider");
		if (!sliderRoot)
		{
			LF_Debug.Error("UIKit", "quantity " + name + " is missing the slider");
			return;
		}

		LF_ThinSliderWidget slider = LF_ThinSliderWidget.Cast(sliderRoot.FindHandler(LF_ThinSliderWidget));
		if (!slider)
		{
			LF_Debug.Error("UIKit", "quantity " + name + " slider has no handler");
			return;
		}

		slider.GetOnValueChanged().Insert(OnChanged);
		m_Slider = slider;
		m_bWired = true;
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_Slider && m_Slider.GetOnValueChanged())
			m_Slider.GetOnValueChanged().Remove(OnChanged);

		m_Slider = null;
		m_bWired = false;
		super.HandlerDeattached(w);
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
