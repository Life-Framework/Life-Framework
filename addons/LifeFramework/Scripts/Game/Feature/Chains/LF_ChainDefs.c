//------------------------------------------------------------------------------------------------
//! Illicit chain config rows (docs/roadmap/scopes/crime/drug-chains.md). A chain is data in
//! Configs/Resources/ResourceChains.conf (LF_ResourceChainConfig.m_aChains); a pack adds one by
//! overriding that file, never with script. LF_ChainCatalog loads and validates the rows; a bad
//! chain disables itself and nothing else.
//------------------------------------------------------------------------------------------------

//! Equipment a step can demand: an item the worker carries (a coil, a press screw), bought from a
//! shop for m_iPrice. Carrying it is the unlock, so it persists with the inventory.
[BaseContainerProps()]
class LF_ChainEquipmentDef
{
	[Attribute("", UIWidgets.EditBox, "Equipment id, matched by LF_ChainStepDef.m_sEquipmentId")]
	string m_sEquipmentId;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item the worker must carry", "et")]
	ResourceName m_Prefab;

	[Attribute("0", UIWidgets.Auto, "Shop price of the item. Capital is one of the four difficulty levers")]
	int m_iPrice;
}

//------------------------------------------------------------------------------------------------
//! How loud a running site is. LF_ChainSite adds heat while people work and lets it fade; the
//! smell and smoke trace (#418) reads the number and the range.
[BaseContainerProps()]
class LF_ChainHeatDef
{
	[Attribute("1", UIWidgets.Auto, "Heat added per unit of hands-on work")]
	float m_fPerWork;

	[Attribute("0.5", UIWidgets.Auto, "Heat lost per second while nobody works")]
	float m_fDecayPerSecond;

	[Attribute("100", UIWidgets.Auto, "Heat never rises above this")]
	float m_fMax;

	[Attribute("40", UIWidgets.Auto, "Metres a site at full heat can be smelled from")]
	float m_fRangeM;
}

//------------------------------------------------------------------------------------------------
//! One step: work at a station, turning the inputs into the outputs once enough hands-on work
//! has been done. Steps run in the order listed.
[BaseContainerProps()]
class LF_ChainStepDef
{
	[Attribute("", UIWidgets.EditBox, "Step id, unique inside the chain")]
	string m_sStepId;

	[Attribute("", UIWidgets.EditBox, "Station type this step needs (barrel, still, press, ...), matched by LF_ChainStationComponent")]
	string m_sStationType;

	[Attribute("", UIWidgets.Object, "Inputs consumed when a batch starts")]
	ref array<ref LF_ProcessingInput> m_aInputs;

	[Attribute("", UIWidgets.Object, "Outputs produced when the batch is finished")]
	ref array<ref LF_ProcessingOutput> m_aOutputs;

	[Attribute("10", UIWidgets.Auto, "Hands-on work units one batch needs. One worker takes this many work ticks, N workers about 1/N of that time")]
	int m_iWork;

	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Licence the worker must hold. UNEMPLOYED (everyone has it) means no licence gate", "", ParamEnumArray.FromEnum(LF_ELicenseType))]
	LF_ELicenseType m_eLicense;

	[Attribute("0", UIWidgets.Auto, "Minimum player level. 0 means no level gate")]
	int m_iMinLevel;

	[Attribute("", UIWidgets.EditBox, "Equipment id from the chain's equipment list. Empty means none needed")]
	string m_sEquipmentId;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ChainDef
{
	[Attribute("", UIWidgets.EditBox, "Chain id, unique across the config")]
	string m_sChainId;

	[Attribute("1", UIWidgets.Auto, "Difficulty tier: 1 easy, 2 medium, 3 hard")]
	int m_iTier;

	[Attribute("", UIWidgets.EditBox, "Localization key for the chain name")]
	string m_sNameKey;

	[Attribute("", UIWidgets.Object, "Steps in order")]
	ref array<ref LF_ChainStepDef> m_aSteps;

	[Attribute("", UIWidgets.Object, "Equipment the steps can demand")]
	ref array<ref LF_ChainEquipmentDef> m_aEquipment;

	[Attribute("", UIWidgets.Object, "Heat settings. Missing means the chain makes no trace")]
	ref LF_ChainHeatDef m_Heat;

	//------------------------------------------------------------------------------------------------
	LF_ChainStepDef FindStep(string stepId)
	{
		if (!m_aSteps || stepId.IsEmpty())
			return null;

		foreach (LF_ChainStepDef step : m_aSteps)
		{
			if (step && step.m_sStepId == stepId)
				return step;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	LF_ChainEquipmentDef FindEquipment(string equipmentId)
	{
		if (!m_aEquipment || equipmentId.IsEmpty())
			return null;

		foreach (LF_ChainEquipmentDef equipment : m_aEquipment)
		{
			if (equipment && equipment.m_sEquipmentId == equipmentId)
				return equipment;
		}

		return null;
	}
}
