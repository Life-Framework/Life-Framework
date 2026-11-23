// LF_NewspaperEditionConfig.c: the sections of the weekly front page (WO-W2.2), from
// Configs/Signs/LF_NewspaperSections.conf. Each section is one story: a headline key and a line key
// per fact. Which kinds of fact exist, and which of them may carry a name, is decided in code
// (LF_NewspaperEdition.IsPrintable, NameAllowed); a section row can only narrow it, never widen it.

//------------------------------------------------------------------------------------------------
//! The kinds of fact the edition log holds. Only ARREST, REWARD, OPENING, MARKET and CRIME are ever
//! printed. OPEN_CASE and HIDDEN exist so a fact that reaches the log by mistake is refused by kind.
enum LF_ENewspaperFact
{
	ARREST,
	REWARD,
	OPENING,
	MARKET,
	CRIME,
	OPEN_CASE,
	HIDDEN
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("m_sId")]
class LF_NewspaperSectionDef
{
	[Attribute("", desc: "Section id")]
	string m_sId;

	[Attribute("0", UIWidgets.ComboBox, "Fact kind this section prints", enums: ParamEnumArray.FromEnum(LF_ENewspaperFact))]
	LF_ENewspaperFact m_eKind;

	[Attribute("", desc: "Headline (localization key)")]
	string m_sHeadline;

	[Attribute("", desc: "One line per fact, named: %1 name, %2 detail, %3 amount, %4 second amount")]
	string m_sLine;

	[Attribute("", desc: "One line per fact, no name: %2 detail, %3 amount, %4 second amount")]
	string m_sLineNoName;

	[Attribute("1", desc: "Print character names (only arrests, rewards and openings can)")]
	bool m_bNames;

	[Attribute("4", desc: "Most lines this section prints")]
	int m_iMaxLines;

	[Attribute("", desc: "Money reasons (LF_OnMoneyMoved.m_sSource) that count for a REWARD or OPENING section")]
	ref array<string> m_aSources;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_NewspaperSectionsConfig
{
	[Attribute("", UIWidgets.Object, "Sections, in print order")]
	ref array<ref LF_NewspaperSectionDef> m_aSections;

	static const ResourceName CONFIG_PATH = "{9B3E51D7A20C6F84}Configs/Signs/LF_NewspaperSections.conf";

	protected static ref LF_NewspaperSectionsConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The loaded sections, or an empty config when the file is missing (logged once).
	static LF_NewspaperSectionsConfig Get()
	{
		if (s_Config)
			return s_Config;

		Resource resource = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (resource && resource.IsValid())
			s_Config = LF_NewspaperSectionsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(LF_Newspaper.FEATURE, "sections config failed to load: " + CONFIG_PATH);
			s_Config = new LF_NewspaperSectionsConfig();
		}

		if (!s_Config.m_aSections)
			s_Config.m_aSections = new array<ref LF_NewspaperSectionDef>();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperSectionsConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_NewspaperSectionsConfig";
	}

	override void Apply()
	{
		LF_NewspaperSectionsConfig.ResetStatics();
	}
}
