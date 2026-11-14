[BaseContainerProps(configRoot: true)]
class LF_FixtureConfig
{
	[Attribute("1", UIWidgets.CheckBox, "On")]
	bool m_bEnabled;

	[Attribute("30", UIWidgets.EditBox, "Range")]
	float m_fRange;

	// An attribute-less field keeps its initializer, and a constant is not a member.
	int m_iRuntime = 5;
	static const int DEFAULT_RANGE = 30;

	static LF_FixtureConfig CreateDefault()
	{
		LF_FixtureConfig config = new LF_FixtureConfig();
		config.m_bEnabled = true;
		config.m_fRange = 30;
		return config;
	}
}
