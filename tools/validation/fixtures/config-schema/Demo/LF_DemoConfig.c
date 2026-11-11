//! A demo section: range, enum, string, bool and a keyed record list.
[BaseContainerProps(configRoot: true)]
class LF_DemoConfig
{
	[Attribute("30", UIWidgets.Slider, desc: "Cooldown in seconds", params: "5 600 1")]
	int m_iCooldownSec;

	[Attribute("0.5", UIWidgets.Auto, desc: "Success chance")]
	float m_fChance;

	[Attribute("true", UIWidgets.CheckBox, desc: "Feature on")]
	bool m_bEnabled;

	[Attribute("", UIWidgets.Auto, desc: "Display name, with a comma, and \"quotes\"")]
	string m_sName;

	[Attribute("2", UIWidgets.Auto, desc: "Mode")]
	LF_EDemoMode m_eMode;

	[Attribute("", UIWidgets.Object, desc: "Records")]
	ref array<ref LF_DemoRecord> m_aRecords;

	static const string SECTION = "demo";
};

[BaseContainerProps()]
class LF_DemoRecord
{
	[Attribute("", UIWidgets.Auto, desc: "Key")]
	string m_sKey;

	[Attribute("7", UIWidgets.Auto, desc: "Amount")]
	int m_iAmount;
};
