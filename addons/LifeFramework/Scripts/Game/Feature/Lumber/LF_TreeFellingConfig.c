//------------------------------------------------------------------------------------------------
//! The species table and the yield table of tree felling (#3259, design docs/roadmap/lumber.md).
//! Core file Configs/Lumber/LF_TreeSpecies.conf. A species folder of the base game's
//! Prefabs/Vegetation/Tree/ maps to a wood class; a class and a size map to the chop time, the
//! logs, the XP action and the stamina cost. A folder with no row is not felled (orchards, willow,
//! rowan, debris). Server and client read the same file; nothing here is in the config stream yet,
//! a pack overlay is the economy card's (#3270) to add.

enum LF_TreeWoodClass
{
	NONE,
	SOFTWOOD,
	HARDWOOD
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_TreeSpeciesRow
{
	[Attribute("", UIWidgets.EditBox, "Species folder under Prefabs/Vegetation/Tree/, lower case (t_pinus_sylvestris)")]
	string m_sFolder;

	[Attribute("0", UIWidgets.ComboBox, "Wood class of the species. NONE is never felled", "", ParamEnumArray.FromEnum(LF_TreeWoodClass))]
	LF_TreeWoodClass m_eClass;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_TreeYieldRow
{
	[Attribute("1", UIWidgets.ComboBox, "Wood class this row prices", "", ParamEnumArray.FromEnum(LF_TreeWoodClass))]
	LF_TreeWoodClass m_eClass;

	[Attribute("1", UIWidgets.EditBox, "Tree size from the prefab name (1 to 3; 0 is a sapling and never felled)", params: "1 3")]
	int m_iSize;

	[Attribute("20", UIWidgets.EditBox, "Seconds of chopping (the hold)", params: "1 600")]
	int m_iChopSeconds;

	[Attribute("1", UIWidgets.EditBox, "Logs the tree drops", params: "1 10")]
	int m_iLogs;

	[Attribute("gather_logging", UIWidgets.EditBox, "Job XP award action (LF_Jobs.conf m_aXpAwards)")]
	string m_sXpAction;

	[Attribute("20", UIWidgets.EditBox, "Stamina the fell costs, percent of full", params: "0 100")]
	int m_iStaminaPct;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_TreeFellingConfig
{
	static const ResourceName CONFIG_PATH = "{6EB6BECFF22FD78A}Configs/Lumber/LF_TreeSpecies.conf";

	[Attribute("", UIWidgets.Object, "Species folder to wood class")]
	ref array<ref LF_TreeSpeciesRow> m_aSpecies;

	[Attribute("", UIWidgets.Object, "Wood class and size to chop time, logs, XP and stamina")]
	ref array<ref LF_TreeYieldRow> m_aYields;

	[Attribute("3", UIWidgets.EditBox, "Most metres from the trunk a fell is allowed", params: "0.5 10")]
	float m_fReachM;

	[Attribute("4", UIWidgets.EditBox, "A road or other hard surface nearer than this many metres refuses the fell", params: "0 50")]
	float m_fRoadClearM;

	[Attribute("12", UIWidgets.EditBox, "A building nearer than this many metres refuses the fell", params: "0 100")]
	float m_fBuildingClearM;

	[Attribute("2700", UIWidgets.EditBox, "Seconds a felled softwood tree stays down", params: "60 86400")]
	int m_iSoftwoodRegrowS;

	[Attribute("5400", UIWidgets.EditBox, "Seconds a felled hardwood tree stays down", params: "60 86400")]
	int m_iHardwoodRegrowS;

	protected static ref LF_TreeFellingConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The core file, loaded once. Null when it is missing or broken (logged); callers refuse every
	//! fell then, so a bad file degrades lumber only.
	static LF_TreeFellingConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads and checks one file. Exposed for the test, which reads the shipped file directly.
	static LF_TreeFellingConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error(LF_TreeFelling.FEATURE, string.Format("tree species config does not resolve: %1", path));
			return null;
		}

		LF_TreeFellingConfig config = LF_TreeFellingConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
		{
			LF_Debug.Error(LF_TreeFelling.FEATURE, string.Format("tree species config has an unexpected root class: %1", path));
			return null;
		}

		config.Normalise();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Lower-cases the folders and drops rows that cannot work, with an error naming each.
	void Normalise()
	{
		if (!m_aSpecies)
			m_aSpecies = new array<ref LF_TreeSpeciesRow>();
		if (!m_aYields)
			m_aYields = new array<ref LF_TreeYieldRow>();

		for (int i = m_aSpecies.Count() - 1; i >= 0; i--)
		{
			LF_TreeSpeciesRow species = m_aSpecies[i];
			if (!species || species.m_sFolder.IsEmpty())
			{
				LF_Debug.Error(LF_TreeFelling.FEATURE, string.Format("tree species row %1 has no folder, dropped", i));
				m_aSpecies.Remove(i);
				continue;
			}

			species.m_sFolder.ToLower();
		}

		for (int i = m_aYields.Count() - 1; i >= 0; i--)
		{
			LF_TreeYieldRow yield = m_aYields[i];
			if (!yield || yield.m_eClass == LF_TreeWoodClass.NONE || yield.m_iSize < 1 || yield.m_iLogs < 1 || yield.m_iChopSeconds < 1)
			{
				LF_Debug.Error(LF_TreeFelling.FEATURE, string.Format("tree yield row %1 is unusable (class NONE, size below 1, no logs or no chop time), dropped", i));
				m_aYields.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	LF_TreeWoodClass ClassOfFolder(string folder)
	{
		foreach (LF_TreeSpeciesRow species : m_aSpecies)
		{
			if (species.m_sFolder == folder)
				return species.m_eClass;
		}

		return LF_TreeWoodClass.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The row for a class and size, or null (a size with no row is not felled).
	LF_TreeYieldRow YieldOf(LF_TreeWoodClass woodClass, int size)
	{
		foreach (LF_TreeYieldRow yield : m_aYields)
		{
			if (yield.m_eClass == woodClass && yield.m_iSize == size)
				return yield;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int RegrowSecondsOf(LF_TreeWoodClass woodClass)
	{
		if (woodClass == LF_TreeWoodClass.HARDWOOD)
			return m_iHardwoodRegrowS;

		return m_iSoftwoodRegrowS;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TreeFellingConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TreeFellingConfig";
	}

	override void Apply()
	{
		LF_TreeFellingConfig.ResetStatics();
	}
}
