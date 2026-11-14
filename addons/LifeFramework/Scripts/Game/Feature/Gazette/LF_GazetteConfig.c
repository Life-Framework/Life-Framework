//------------------------------------------------------------------------------------------------
//! One headline rule (#792): which bus event class counts, and how it is worded. The words are
//! string keys with terms placeholders ({police}, {Town}); a pack overlays the table with its own
//! keys, so the core carries no voice. %1 is the name, %2 a detail (an offence key, a kind), %3 a number.
[BaseContainerProps()]
class LF_GazetteRule
{
	[Attribute("", UIWidgets.EditBox, "Bus event class, e.g. LF_OnCrimeCommitted")]
	string m_sEvent;

	[Attribute("", UIWidgets.EditBox, "Kind of story. The per-kind cap counts by it")]
	string m_sKind;

	[Attribute("", UIWidgets.EditBox, "String key of the story. %1 name, %2 detail, %3 number; terms such as {police} are allowed")]
	string m_sTextKey;

	[Attribute("", UIWidgets.EditBox, "String key used when the name is withheld (not identified). Empty: the story is dropped")]
	string m_sAnonKey;

	[Attribute("1", UIWidgets.Auto, "Higher runs first when the paper is full")]
	int m_iPriority;

	[Attribute("1", UIWidgets.CheckBox, "1 = the story may carry a name; 0 = nobody is ever named in it")]
	bool m_bNamed;

	[Attribute("1", UIWidgets.CheckBox, "1 = the name prints only once the suspect is identified; 0 = public (an arrest)")]
	bool m_bNeedsIdentified;

	[Attribute("0", UIWidgets.Auto, "Most stories of this kind per edition; 0 = the config's default cap")]
	int m_iKindCap;

	//------------------------------------------------------------------------------------------------
	static LF_GazetteRule Make(string eventClass, string kind, string textKey, string anonKey, int priority, bool named = true, bool needsIdentified = true)
	{
		LF_GazetteRule rule = new LF_GazetteRule();
		rule.m_sEvent = eventClass;
		rule.m_sKind = kind;
		rule.m_sTextKey = textKey;
		rule.m_sAnonKey = anonKey;
		rule.m_iPriority = priority;
		rule.m_bNamed = named;
		rule.m_bNeedsIdentified = needsIdentified;
		return rule;
	}
}

//------------------------------------------------------------------------------------------------
//! The gazette's settings (Configs/Gazette/LF_Gazette.conf, #792). A missing or broken file logs once
//! and the gazette stays off. A pack overlays the rule table with its own words and tone.
[BaseContainerProps(configRoot: true)]
class LF_GazetteConfig
{
	static const ResourceName CONFIG_PATH = "{DA7ED0853C72446B}Configs/Gazette/LF_Gazette.conf";

	protected static ref LF_GazetteConfig s_Config;
	protected static bool s_bLoadAttempted;

	[Attribute("#LF-Gazette_Title", UIWidgets.EditBox, "String key of the paper's name (the pack supplies its own)")]
	string m_sPaperNameKey;

	[Attribute("1440", UIWidgets.Auto, "Minutes between two editions")]
	int m_iEditionIntervalMinutes;

	[Attribute("6", UIWidgets.Auto, "Most headlines in an edition")]
	int m_iMaxHeadlines;

	[Attribute("2", UIWidgets.Auto, "Most stories of one kind in an edition, so one griefer cannot fill the paper")]
	int m_iPerKindCap;

	[Attribute("4", UIWidgets.Auto, "Editions kept for the stand")]
	int m_iEditionsKept;

	[Attribute("300", UIWidgets.Auto, "Most events waiting for the next edition; the oldest go first")]
	int m_iMaxPending;

	[Attribute("", UIWidgets.Object, "Which event classes count and how they are worded")]
	ref array<ref LF_GazetteRule> m_aRules;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config instead of the file.
	static void UseForTest(LF_GazetteConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	static LF_GazetteConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Gazette.FEATURE, string.Format("gazette config does not resolve, the gazette is off: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_GazetteConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Gazette.FEATURE, string.Format("gazette config has an unexpected root class, the gazette is off: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule for an event class, or null.
	LF_GazetteRule FindRule(string eventClass)
	{
		if (!m_aRules)
			return null;

		foreach (LF_GazetteRule rule : m_aRules)
		{
			if (rule && rule.m_sEvent == eventClass)
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A built-in table for tests.
	static LF_GazetteConfig MakeForTest(int maxHeadlines, int perKindCap)
	{
		LF_GazetteConfig config = new LF_GazetteConfig();
		config.m_sPaperNameKey = "#LF-Gazette_Title";
		config.m_iEditionIntervalMinutes = 60;
		config.m_iMaxHeadlines = maxHeadlines;
		config.m_iPerKindCap = perKindCap;
		config.m_iEditionsKept = 2;
		config.m_iMaxPending = 50;
		config.m_aRules = {};
		config.m_aRules.Insert(LF_GazetteRule.Make("LF_OnCrimeCommitted", "crime", "#LF-Gazette_Crime", "#LF-Gazette_CrimeAnon", 5));
		config.m_aRules.Insert(LF_GazetteRule.Make("LF_OnArrested", "arrest", "#LF-Gazette_Arrest", "", 8, true, false));
		config.m_aRules.Insert(LF_GazetteRule.Make("LF_OnJobCompleted", "job", "#LF-Gazette_Job", "", 2, false));
		return config;
	}
}
