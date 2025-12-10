//------------------------------------------------------------------------------------------------
//! Job types available in the game
enum EL_EJobType
{
	UNEMPLOYED = 0,
	FARMER = 1,      // Recolector - Frutos y agua
	MINER = 2,       // Minero - Pico y minerales
	LUMBERJACK = 3,  // Leñador - Hacha y madera
	MEDIC = 4,       // EMS - Paramédico
	// Whitelist-only jobs
	POLICE = 100     // Policía - Requiere whitelist
}

//------------------------------------------------------------------------------------------------
//! Job configuration class
[BaseContainerProps()]
class EL_JobConfig
{
	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Job type", "", ParamEnumArray.FromEnum(EL_EJobType))]
	EL_EJobType m_eJobType;
	
	[Attribute(defvalue: "Unemployed", UIWidgets.EditBox, "Job name")]
	string m_sJobName;
	
	[Attribute(defvalue: "No job description", UIWidgets.EditBox, "Job description")]
	string m_sDescription;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Icon for HUD/Menu", "edds")]
	ResourceName m_sIcon;
	
	[Attribute(defvalue: "100", UIWidgets.Slider, "Base salary per paycheck", "0 10000 10")]
	int m_iBaseSalary;
	
	[Attribute(defvalue: "300", UIWidgets.Slider, "Time between paychecks in seconds", "60 3600 10")]
	float m_fPaycheckInterval;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Uniform/clothing prefabs for this job", "et")]
	ref array<ResourceName> m_aUniformPrefabs;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Tool/equipment prefabs for this job", "et")]
	ref array<ResourceName> m_aEquipmentPrefabs;
	
	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Can this job gather resources")]
	bool m_bCanGatherResources;
	
	[Attribute(defvalue: "1.0", UIWidgets.Slider, "Resource gathering speed multiplier", "0.1 5.0 0.1")]
	float m_fGatheringSpeedMultiplier;
	
	[Attribute(defvalue: "0", UIWidgets.ComboBox, "Required license to access this job", "", ParamEnumArray.FromEnum(EL_ELicenseType))]
	EL_ELicenseType m_eRequiredLicense;
	
	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Is this job available from the start (no license required)")]
	bool m_bIsInitialJob;
}
