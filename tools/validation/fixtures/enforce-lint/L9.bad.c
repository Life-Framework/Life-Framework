[BaseContainerProps(configRoot: true)]
class LF_FixtureConfig
{
	[Attribute("1", UIWidgets.CheckBox, "On")]
	bool m_bEnabled = true;

	[Attribute("5", UIWidgets.EditBox, "Seconds")]
	ref array<int> m_aSteps = {5};

	[Attribute("30", UIWidgets.EditBox, "Range")]
	float m_fRange = 30;
}
