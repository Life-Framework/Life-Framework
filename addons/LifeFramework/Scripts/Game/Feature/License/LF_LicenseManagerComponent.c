//------------------------------------------------------------------------------------------------
//! Tipos de licencias/habilidades disponibles - Sincronizadas con trabajos
enum LF_ELicenseType
{
	// INICIAL - Gratuito
	UNEMPLOYED = 0,
	
	// TRABAJO: RECOLECTOR (FARMER) - Frutos y Agua
	FARMER_FIELD = 1,       // Inicial - peón de campo (gratis); era FARMER_FIELD, el valor 1 se conserva por los guardados
	FARMER_APPLE = 2,       // Manzanas de huertos
	FARMER_PLUM = 3,        // Ciruelas de árboles
	FARMER_WATER = 4,       // Agua purificada
	FARMER_SPEED = 5,       // x2 velocidad recolección frutas
	
	// TRABAJO: MINERO (MINER) - Minerales con Pico
	MINER_COAL = 10,        // Carbón (IronNugget.et)
	MINER_COPPER = 11,      // Cobre
	MINER_IRON = 12,        // Hierro (IronOre.et)
	MINER_GOLD = 13,        // Oro
	MINER_SPEED = 14,       // x2 velocidad minería
	
	// TRABAJO: LEÑADOR (LUMBERJACK) - Madera con Hacha
	LUMBERJACK_BASIC = 20,  // Cortar árboles básicos
	LUMBERJACK_PROCESS = 21,// Procesar troncos en tablones
	LUMBERJACK_SPEED = 22,  // x2 velocidad corte
	
	// TRABAJO: EMS (MEDIC) - Curación
	MEDIC_ACCESS = 29,      // Permiso de acceso al trabajo (Credencial EMS)
	MEDIC_BASIC = 30,       // Curación básica
	MEDIC_ADVANCED = 31,    // Curación avanzada (+HP)
	MEDIC_REVIVE = 32,      // Revivir jugadores
	
	// TRABAJO: POLICÍA (POLICE) - Requiere Whitelist
	POLICE_ACCESS = 99,     // Permiso de acceso al trabajo (Placa)
	POLICE_ARREST = 100,    // Arrestar criminales
	POLICE_FINE = 101,      // Multar infractores
	POLICE_SEARCH = 102,    // Registrar jugadores
	
	// RAMA CRIMINAL - Actividades Ilegales (Sin trabajo, solo licencias)
	CRIMINAL_WEED = 200,        // Cultivar marihuana
	CRIMINAL_ROBBERY = 201,     // Robar locaciones
	CRIMINAL_BLACKMARKET = 202, // Acceso mercado negro
	CRIMINAL_WEAPONS = 203,     // Comprar armas ilegales
	CRIMINAL_VEHICLES = 204,    // Comprar vehículos ilegales
	CRIMINAL_CLOTHES = 205      // Comprar ropa del mercado negro
}

//------------------------------------------------------------------------------------------------
//! Configuración de una licencia
[BaseContainerProps()]
class LF_LicenseConfig
{
	[Attribute(defvalue: "0", UIWidgets.ComboBox, "License type", "", ParamEnumArray.FromEnum(LF_ELicenseType))]
	LF_ELicenseType m_eLicenseType;
	
	[Attribute(defvalue: "Unknown License", UIWidgets.EditBox, "License name")]
	string m_sLicenseName;
	
	[Attribute(defvalue: "No description", UIWidgets.EditBox, "License description")]
	string m_sDescription;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, "Skill points cost")]
	int m_iSkillPointCost;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, "Minimum level required")]
	int m_iMinLevelRequired;
	
	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Is this an initial license (free at start)")]
	bool m_bIsInitialLicense;
	
	[Attribute(defvalue: "0", UIWidgets.CheckBox, "Requires whitelist")]
	bool m_bRequiresWhitelist;
}

//------------------------------------------------------------------------------------------------
//! Componente que gestiona las licencias del jugador
[ComponentEditorProps(category: "EveronLife/Feature/License", description: "Manages player's unlocked licenses/professions")]
class LF_LicenseManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! License set <-> int bitmask for replication. The enum values are sparse (0..205), so each
//! type gets a bit by its position in ORDER. Append new types at the end; never reorder, or a
//! client decodes a different license. 31 bits are available (bit 31 is the sign).
class LF_LicenseBits
{
	protected static ref array<LF_ELicenseType> s_aOrder;

	//------------------------------------------------------------------------------------------------
	static array<LF_ELicenseType> Order()
	{
		if (!s_aOrder)
		{
			array<LF_ELicenseType> order = {
				LF_ELicenseType.UNEMPLOYED,
				LF_ELicenseType.FARMER_FIELD, LF_ELicenseType.FARMER_APPLE, LF_ELicenseType.FARMER_PLUM, LF_ELicenseType.FARMER_WATER, LF_ELicenseType.FARMER_SPEED,
				LF_ELicenseType.MINER_COAL, LF_ELicenseType.MINER_COPPER, LF_ELicenseType.MINER_IRON, LF_ELicenseType.MINER_GOLD, LF_ELicenseType.MINER_SPEED,
				LF_ELicenseType.LUMBERJACK_BASIC, LF_ELicenseType.LUMBERJACK_PROCESS, LF_ELicenseType.LUMBERJACK_SPEED,
				LF_ELicenseType.MEDIC_ACCESS, LF_ELicenseType.MEDIC_BASIC, LF_ELicenseType.MEDIC_ADVANCED, LF_ELicenseType.MEDIC_REVIVE,
				LF_ELicenseType.POLICE_ACCESS, LF_ELicenseType.POLICE_ARREST, LF_ELicenseType.POLICE_FINE, LF_ELicenseType.POLICE_SEARCH,
				LF_ELicenseType.CRIMINAL_WEED, LF_ELicenseType.CRIMINAL_ROBBERY, LF_ELicenseType.CRIMINAL_BLACKMARKET, LF_ELicenseType.CRIMINAL_WEAPONS,
				LF_ELicenseType.CRIMINAL_VEHICLES, LF_ELicenseType.CRIMINAL_CLOTHES
			};
			s_aOrder = order;
		}

		return s_aOrder;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Bit index for a license type, or -1 when it has none (logged).
	static int BitOf(LF_ELicenseType licenseType)
	{
		int index = Order().Find(licenseType);
		if (index < 0 || index > 30)
		{
			LF_Debug.Error("License", string.Format("license %1 has no replication bit", licenseType));
			return -1;
		}

		return index;
	}

	//------------------------------------------------------------------------------------------------
	static int Encode(notnull array<LF_ELicenseType> licenses)
	{
		int bits = 0;
		foreach (LF_ELicenseType licenseType : licenses)
		{
			int bit = BitOf(licenseType);
			if (bit >= 0)
				bits = bits | (1 << bit);
		}

		return bits;
	}

	//------------------------------------------------------------------------------------------------
	static bool Has(int bits, LF_ELicenseType licenseType)
	{
		int bit = BitOf(licenseType);
		if (bit < 0)
			return false;

		return (bits & (1 << bit)) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills outLicenses in ORDER with every type whose bit is set.
	static void Decode(int bits, notnull array<LF_ELicenseType> outLicenses)
	{
		outLicenses.Clear();
		array<LF_ELicenseType> order = Order();
		int count = order.Count();
		if (count > 31)
			count = 31;

		for (int i = 0; i < count; i++)
		{
			if ((bits & (1 << i)) != 0)
				outLicenses.Insert(order.Get(i));
		}
	}
}

//------------------------------------------------------------------------------------------------
class LF_LicenseManagerComponent : ScriptComponent
{
	//! Server-side list (authoritative). Not replicated: RplProp carries simple types only.
	protected ref array<LF_ELicenseType> m_aUnlockedLicenses = new array<LF_ELicenseType>();

	//! Replicated copy of m_aUnlockedLicenses as an LF_LicenseBits mask. Written on the server
	//! whenever the list changes; a client decodes it on read, so a late joiner needs no callback.
	[RplProp()]
	protected int m_iUnlockedBits;

	protected ref ScriptInvoker m_OnLicenseUnlocked;
	protected ref map<LF_ELicenseType, ref LF_LicenseConfig> m_mLicenseConfigs;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
		
		SetEventMask(owner, EntityEvent.INIT);
		InitializeLicenseConfigs();
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!Replication.IsServer())
			return;
		
		// Dar licencias iniciales si no tiene ninguna
		if (m_aUnlockedLicenses.IsEmpty())
		{
			GiveInitialLicenses();
		}
		
		LF_Debug.Log("License", string.Format("Player has %1 licenses unlocked", 
			m_aUnlockedLicenses.Count()));
	}
	
	//------------------------------------------------------------------------------------------------
	protected void InitializeLicenseConfigs()
	{
		m_mLicenseConfigs = new map<LF_ELicenseType, ref LF_LicenseConfig>();
		
		// ============= LICENCIAS INICIALES (GRATUITAS) =============
		LF_LicenseConfig unemployed = new LF_LicenseConfig();
		unemployed.m_eLicenseType = LF_ELicenseType.UNEMPLOYED;
		unemployed.m_sLicenseName = "#LF-License_Unemployed_Name";
		unemployed.m_sDescription = "#LF-License_Unemployed_Desc";
		unemployed.m_iSkillPointCost = 0;
		unemployed.m_iMinLevelRequired = 1;
		unemployed.m_bIsInitialLicense = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.UNEMPLOYED, unemployed);
		
		// ============= TRABAJO: RECOLECTOR (FARMER) =============
		LF_LicenseConfig farmerField = new LF_LicenseConfig();
		farmerField.m_eLicenseType = LF_ELicenseType.FARMER_FIELD;
		farmerField.m_sLicenseName = "#LF-License_FarmerField_Name";
		farmerField.m_sDescription = "#LF-License_FarmerField_Desc";
		farmerField.m_iSkillPointCost = 0;
		farmerField.m_iMinLevelRequired = 1;
		farmerField.m_bIsInitialLicense = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.FARMER_FIELD, farmerField);
		
		LF_LicenseConfig farmerApple = new LF_LicenseConfig();
		farmerApple.m_eLicenseType = LF_ELicenseType.FARMER_APPLE;
		farmerApple.m_sLicenseName = "#LF-License_FarmerApple_Name";
		farmerApple.m_sDescription = "#LF-License_FarmerApple_Desc";
		farmerApple.m_iSkillPointCost = 1;
		farmerApple.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(LF_ELicenseType.FARMER_APPLE, farmerApple);
		
		LF_LicenseConfig farmerPlum = new LF_LicenseConfig();
		farmerPlum.m_eLicenseType = LF_ELicenseType.FARMER_PLUM;
		farmerPlum.m_sLicenseName = "#LF-License_FarmerPlum_Name";
		farmerPlum.m_sDescription = "#LF-License_FarmerPlum_Desc";
		farmerPlum.m_iSkillPointCost = 2;
		farmerPlum.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.FARMER_PLUM, farmerPlum);
		
		LF_LicenseConfig farmerWater = new LF_LicenseConfig();
		farmerWater.m_eLicenseType = LF_ELicenseType.FARMER_WATER;
		farmerWater.m_sLicenseName = "#LF-License_FarmerWater_Name";
		farmerWater.m_sDescription = "#LF-License_FarmerWater_Desc";
		farmerWater.m_iSkillPointCost = 1;
		farmerWater.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(LF_ELicenseType.FARMER_WATER, farmerWater);
		
		LF_LicenseConfig farmerSpeed = new LF_LicenseConfig();
		farmerSpeed.m_eLicenseType = LF_ELicenseType.FARMER_SPEED;
		farmerSpeed.m_sLicenseName = "#LF-License_FarmerSpeed_Name";
		farmerSpeed.m_sDescription = "#LF-License_FarmerSpeed_Desc";
		farmerSpeed.m_iSkillPointCost = 3;
		farmerSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.FARMER_SPEED, farmerSpeed);
		
		// ============= TRABAJO: MINERO (MINER) =============
		LF_LicenseConfig minerCoal = new LF_LicenseConfig();
		minerCoal.m_eLicenseType = LF_ELicenseType.MINER_COAL;
		minerCoal.m_sLicenseName = "#LF-License_MinerCoal_Name";
		minerCoal.m_sDescription = "#LF-License_MinerCoal_Desc";
		minerCoal.m_iSkillPointCost = 1;
		minerCoal.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(LF_ELicenseType.MINER_COAL, minerCoal);
		
		LF_LicenseConfig minerCopper = new LF_LicenseConfig();
		minerCopper.m_eLicenseType = LF_ELicenseType.MINER_COPPER;
		minerCopper.m_sLicenseName = "#LF-License_MinerCopper_Name";
		minerCopper.m_sDescription = "#LF-License_MinerCopper_Desc";
		minerCopper.m_iSkillPointCost = 2;
		minerCopper.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.MINER_COPPER, minerCopper);
		
		LF_LicenseConfig minerIron = new LF_LicenseConfig();
		minerIron.m_eLicenseType = LF_ELicenseType.MINER_IRON;
		minerIron.m_sLicenseName = "#LF-License_MinerIron_Name";
		minerIron.m_sDescription = "#LF-License_MinerIron_Desc";
		minerIron.m_iSkillPointCost = 2;
		minerIron.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.MINER_IRON, minerIron);
		
		LF_LicenseConfig minerGold = new LF_LicenseConfig();
		minerGold.m_eLicenseType = LF_ELicenseType.MINER_GOLD;
		minerGold.m_sLicenseName = "#LF-License_MinerGold_Name";
		minerGold.m_sDescription = "#LF-License_MinerGold_Desc";
		minerGold.m_iSkillPointCost = 3;
		minerGold.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(LF_ELicenseType.MINER_GOLD, minerGold);
		
		LF_LicenseConfig minerSpeed = new LF_LicenseConfig();
		minerSpeed.m_eLicenseType = LF_ELicenseType.MINER_SPEED;
		minerSpeed.m_sLicenseName = "#LF-License_MinerSpeed_Name";
		minerSpeed.m_sDescription = "#LF-License_MinerSpeed_Desc";
		minerSpeed.m_iSkillPointCost = 3;
		minerSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.MINER_SPEED, minerSpeed);
		
		// ============= TRABAJO: LEÑADOR (LUMBERJACK) =============
		LF_LicenseConfig lumberjackBasic = new LF_LicenseConfig();
		lumberjackBasic.m_eLicenseType = LF_ELicenseType.LUMBERJACK_BASIC;
		lumberjackBasic.m_sLicenseName = "#LF-License_LumberjackBasic_Name";
		lumberjackBasic.m_sDescription = "#LF-License_LumberjackBasic_Desc";
		lumberjackBasic.m_iSkillPointCost = 1;
		lumberjackBasic.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(LF_ELicenseType.LUMBERJACK_BASIC, lumberjackBasic);
		
		LF_LicenseConfig lumberjackProcess = new LF_LicenseConfig();
		lumberjackProcess.m_eLicenseType = LF_ELicenseType.LUMBERJACK_PROCESS;
		lumberjackProcess.m_sLicenseName = "#LF-License_LumberjackProcess_Name";
		lumberjackProcess.m_sDescription = "#LF-License_LumberjackProcess_Desc";
		lumberjackProcess.m_iSkillPointCost = 2;
		lumberjackProcess.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.LUMBERJACK_PROCESS, lumberjackProcess);
		
		LF_LicenseConfig lumberjackSpeed = new LF_LicenseConfig();
		lumberjackSpeed.m_eLicenseType = LF_ELicenseType.LUMBERJACK_SPEED;
		lumberjackSpeed.m_sLicenseName = "#LF-License_LumberjackSpeed_Name";
		lumberjackSpeed.m_sDescription = "#LF-License_LumberjackSpeed_Desc";
		lumberjackSpeed.m_iSkillPointCost = 3;
		lumberjackSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.LUMBERJACK_SPEED, lumberjackSpeed);
		
		// ============= TRABAJO: EMS (MEDIC) =============
		LF_LicenseConfig medicAccess = new LF_LicenseConfig();
		medicAccess.m_eLicenseType = LF_ELicenseType.MEDIC_ACCESS;
		medicAccess.m_sLicenseName = "#LF-License_MedicAccess_Name";
		medicAccess.m_sDescription = "#LF-License_MedicAccess_Desc";
		medicAccess.m_iSkillPointCost = 0;
		medicAccess.m_iMinLevelRequired = 1;
		medicAccess.m_bRequiresWhitelist = true; // Assuming EMS also needs whitelist or at least license
		m_mLicenseConfigs.Set(LF_ELicenseType.MEDIC_ACCESS, medicAccess);

		LF_LicenseConfig medicBasic = new LF_LicenseConfig();
		medicBasic.m_eLicenseType = LF_ELicenseType.MEDIC_BASIC;
		medicBasic.m_sLicenseName = "#LF-License_MedicBasic_Name";
		medicBasic.m_sDescription = "#LF-License_MedicBasic_Desc";
		medicBasic.m_iSkillPointCost = 2;
		medicBasic.m_iMinLevelRequired = 3;
		m_mLicenseConfigs.Set(LF_ELicenseType.MEDIC_BASIC, medicBasic);
		
		LF_LicenseConfig medicAdvanced = new LF_LicenseConfig();
		medicAdvanced.m_eLicenseType = LF_ELicenseType.MEDIC_ADVANCED;
		medicAdvanced.m_sLicenseName = "#LF-License_MedicAdvanced_Name";
		medicAdvanced.m_sDescription = "#LF-License_MedicAdvanced_Desc";
		medicAdvanced.m_iSkillPointCost = 3;
		medicAdvanced.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(LF_ELicenseType.MEDIC_ADVANCED, medicAdvanced);
		
		LF_LicenseConfig medicRevive = new LF_LicenseConfig();
		medicRevive.m_eLicenseType = LF_ELicenseType.MEDIC_REVIVE;
		medicRevive.m_sLicenseName = "#LF-License_MedicRevive_Name";
		medicRevive.m_sDescription = "#LF-License_MedicRevive_Desc";
		medicRevive.m_iSkillPointCost = 4;
		medicRevive.m_iMinLevelRequired = 8;
		m_mLicenseConfigs.Set(LF_ELicenseType.MEDIC_REVIVE, medicRevive);
		
		// ============= TRABAJO: POLICÍA (POLICE) - Requiere Whitelist =============
		LF_LicenseConfig policeAccess = new LF_LicenseConfig();
		policeAccess.m_eLicenseType = LF_ELicenseType.POLICE_ACCESS;
		policeAccess.m_sLicenseName = "#LF-License_PoliceAccess_Name";
		policeAccess.m_sDescription = "#LF-License_PoliceAccess_Desc";
		policeAccess.m_iSkillPointCost = 0;
		policeAccess.m_iMinLevelRequired = 1;
		policeAccess.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.POLICE_ACCESS, policeAccess);

		LF_LicenseConfig policeArrest = new LF_LicenseConfig();
		policeArrest.m_eLicenseType = LF_ELicenseType.POLICE_ARREST;
		policeArrest.m_sLicenseName = "#LF-License_PoliceArrest_Name";
		policeArrest.m_sDescription = "#LF-License_PoliceArrest_Desc";
		policeArrest.m_iSkillPointCost = 2;
		policeArrest.m_iMinLevelRequired = 5;
		policeArrest.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.POLICE_ARREST, policeArrest);
		
		LF_LicenseConfig policeFine = new LF_LicenseConfig();
		policeFine.m_eLicenseType = LF_ELicenseType.POLICE_FINE;
		policeFine.m_sLicenseName = "#LF-License_PoliceFine_Name";
		policeFine.m_sDescription = "#LF-License_PoliceFine_Desc";
		policeFine.m_iSkillPointCost = 2;
		policeFine.m_iMinLevelRequired = 5;
		policeFine.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.POLICE_FINE, policeFine);
		
		LF_LicenseConfig policeSearch = new LF_LicenseConfig();
		policeSearch.m_eLicenseType = LF_ELicenseType.POLICE_SEARCH;
		policeSearch.m_sLicenseName = "#LF-License_PoliceSearch_Name";
		policeSearch.m_sDescription = "#LF-License_PoliceSearch_Desc";
		policeSearch.m_iSkillPointCost = 3;
		policeSearch.m_iMinLevelRequired = 7;
		policeSearch.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(LF_ELicenseType.POLICE_SEARCH, policeSearch);
		
		// ============= RAMA CRIMINAL - Actividades Ilegales =============
		LF_LicenseConfig criminalWeed = new LF_LicenseConfig();
		criminalWeed.m_eLicenseType = LF_ELicenseType.CRIMINAL_WEED;
		criminalWeed.m_sLicenseName = "#LF-License_CriminalWeed_Name";
		criminalWeed.m_sDescription = "#LF-License_CriminalWeed_Desc";
		criminalWeed.m_iSkillPointCost = 3;
		criminalWeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_WEED, criminalWeed);
		
		LF_LicenseConfig criminalRobbery = new LF_LicenseConfig();
		criminalRobbery.m_eLicenseType = LF_ELicenseType.CRIMINAL_ROBBERY;
		criminalRobbery.m_sLicenseName = "#LF-License_CriminalRobbery_Name";
		criminalRobbery.m_sDescription = "#LF-License_CriminalRobbery_Desc";
		criminalRobbery.m_iSkillPointCost = 3;
		criminalRobbery.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_ROBBERY, criminalRobbery);
		
		LF_LicenseConfig criminalBlackmarket = new LF_LicenseConfig();
		criminalBlackmarket.m_eLicenseType = LF_ELicenseType.CRIMINAL_BLACKMARKET;
		criminalBlackmarket.m_sLicenseName = "#LF-License_CriminalBlackmarket_Name";
		criminalBlackmarket.m_sDescription = "#LF-License_CriminalBlackmarket_Desc";
		criminalBlackmarket.m_iSkillPointCost = 2;
		criminalBlackmarket.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_BLACKMARKET, criminalBlackmarket);
		
		LF_LicenseConfig criminalWeapons = new LF_LicenseConfig();
		criminalWeapons.m_eLicenseType = LF_ELicenseType.CRIMINAL_WEAPONS;
		criminalWeapons.m_sLicenseName = "#LF-License_CriminalWeapons_Name";
		criminalWeapons.m_sDescription = "#LF-License_CriminalWeapons_Desc";
		criminalWeapons.m_iSkillPointCost = 4;
		criminalWeapons.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_WEAPONS, criminalWeapons);
		
		LF_LicenseConfig criminalVehicles = new LF_LicenseConfig();
		criminalVehicles.m_eLicenseType = LF_ELicenseType.CRIMINAL_VEHICLES;
		criminalVehicles.m_sLicenseName = "#LF-License_CriminalVehicles_Name";
		criminalVehicles.m_sDescription = "#LF-License_CriminalVehicles_Desc";
		criminalVehicles.m_iSkillPointCost = 3;
		criminalVehicles.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_VEHICLES, criminalVehicles);
		
		LF_LicenseConfig criminalClothes = new LF_LicenseConfig();
		criminalClothes.m_eLicenseType = LF_ELicenseType.CRIMINAL_CLOTHES;
		criminalClothes.m_sLicenseName = "#LF-License_CriminalClothes_Name";
		criminalClothes.m_sDescription = "#LF-License_CriminalClothes_Desc";
		criminalClothes.m_iSkillPointCost = 2;
		criminalClothes.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(LF_ELicenseType.CRIMINAL_CLOTHES, criminalClothes);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void GiveInitialLicenses()
	{
		// Dar licencias iniciales gratuitas
		UnlockLicense(LF_ELicenseType.UNEMPLOYED, true);
		UnlockLicense(LF_ELicenseType.FARMER_FIELD, true);
		
		LF_Debug.Log("License", "Initial licenses granted");
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si el jugador puede desbloquear una licencia
	bool CanUnlockLicense(LF_ELicenseType licenseType)
	{
		// Ya desbloqueada
		if (HasLicense(licenseType))
			return false;
		
		LF_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
			return false;
		
		// Verificar whitelist si es necesaria
		if (config.m_bRequiresWhitelist)
		{
			IEntity owner = GetOwner();
			if (!owner) return false;
			
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
			if (playerId <= 0) return false;
			
			string playerUID = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			if (!playerUID) return false;

			LF_EJobType whitelistJob = GetLicenseWhitelistJob(licenseType);
			if (whitelistJob != LF_EJobType.UNEMPLOYED && !LF_WhitelistManager.IsWhitelistedForJob(playerUID, whitelistJob))
				return false;
		}
		
		// Verificar nivel mínimo
		IEntity owner = GetOwner();
		if (owner)
		{
			LF_PlayerLevelComponent levelComp = LF_Component<LF_PlayerLevelComponent>.Find(owner);
			if (levelComp && levelComp.GetPlayerLevel() < config.m_iMinLevelRequired)
				return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si el jugador tiene puntos suficientes para comprar
	bool CanAffordLicense(LF_ELicenseType licenseType)
	{
		LF_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
			return false;
		
		IEntity owner = GetOwner();
		if (!owner)
			return false;
		
		LF_PlayerLevelComponent levelComp = LF_Component<LF_PlayerLevelComponent>.Find(owner);
		if (!levelComp)
			return false;
		
		return levelComp.GetSkillPoints() >= config.m_iSkillPointCost;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Comprar/desbloquear una licencia
	bool PurchaseLicense(LF_ELicenseType licenseType)
	{
		if (!CanUnlockLicense(licenseType))
		{
			LF_Debug.Log("License", string.Format("purchase rejected (requirements not met): %1", typename.EnumToString(LF_ELicenseType, licenseType)));
			return false;
		}

		if (!CanAffordLicense(licenseType))
		{
			LF_Debug.Log("License", string.Format("purchase rejected (cannot afford): %1", typename.EnumToString(LF_ELicenseType, licenseType)));
			return false;
		}

		// UnlockLicense spends the skill points itself after all checks pass;
		// spending here as well would charge the player twice.
		return UnlockLicense(licenseType);
	}

	//------------------------------------------------------------------------------------------------
	//! Job whitelist a license requires, or UNEMPLOYED when it has none.
	static LF_EJobType GetLicenseWhitelistJob(LF_ELicenseType licenseType)
	{
		if (licenseType == LF_ELicenseType.POLICE_ACCESS ||
			licenseType == LF_ELicenseType.POLICE_ARREST ||
			licenseType == LF_ELicenseType.POLICE_FINE ||
			licenseType == LF_ELicenseType.POLICE_SEARCH)
			return LF_EJobType.POLICE;

		if (licenseType == LF_ELicenseType.MEDIC_ACCESS)
			return LF_EJobType.MEDIC;

		return LF_EJobType.UNEMPLOYED;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Desbloquear una licencia
	bool UnlockLicense(LF_ELicenseType licenseType, bool isFree = false)
	{
if (!Replication.IsServer())
			return false;

		// Ya tiene esta licencia
		if (HasLicense(licenseType))
		{
			LF_Debug.Log("License", string.Format("unlock rejected (already owned): %1", typename.EnumToString(LF_ELicenseType, licenseType)));
			return false;
		}
		
		LF_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
		{
			LF_Debug.Log("License", string.Format("unlock rejected (no config): %1", typename.EnumToString(LF_ELicenseType, licenseType)));
			return false;
		}
		
		// Si no es gratis, verificar requisitos y gastar SP
		if (!isFree)
		{
			IEntity owner = GetOwner();
			if (!owner)
				return false;
			
			// Verificar nivel
			LF_PlayerLevelComponent levelComp = LF_Component<LF_PlayerLevelComponent>.Find(owner);
			if (!levelComp)
			{
				LF_Debug.Error("License", "Player level component not found!");
				return false;
			}
			
			if (levelComp.GetLevel() < config.m_iMinLevelRequired)
			{
				LF_Debug.Warn("License", string.Format("Player level %1 is below required level %2", 
					levelComp.GetLevel(), config.m_iMinLevelRequired));
				return false;
			}
			
			// Verificar whitelist si es necesario
			if (config.m_bRequiresWhitelist)
			{
				string playerUID = LF_Utils.GetPlayerUID(owner);
				LF_EJobType whitelistJob = GetLicenseWhitelistJob(licenseType);
				if (whitelistJob == LF_EJobType.UNEMPLOYED)
				{
					LF_Debug.Error("License", string.Format("License %1 requires a whitelist but maps to no job", typename.EnumToString(LF_ELicenseType, licenseType)));
					return false;
				}

				if (!LF_WhitelistManager.IsWhitelistedForJob(playerUID, whitelistJob))
				{
					LF_Debug.Warn("License", "Player not whitelisted for this license");
					return false;
				}
			}
			
			// Gastar skill points
			if (!levelComp.SpendSkillPoints(config.m_iSkillPointCost, config.m_sLicenseName))
			{
				return false;
			}
		}
		
		// Desbloquear licencia
		m_aUnlockedLicenses.Insert(licenseType);
		m_iUnlockedBits = LF_LicenseBits.Encode(m_aUnlockedLicenses);

		// Saved with the account's progression (version 7). The free starter grant at spawn init
		// runs before the body has a player, so only real unlocks land here.
		string ownerUid = LF_Utils.GetPlayerUID(GetOwner());
		if (!ownerUid.IsEmpty())
			LF_Progression.NoteLicenses(ownerUid, m_aUnlockedLicenses);

		LF_Debug.Log("License", string.Format("License unlocked: %1", config.m_sLicenseName));
		
		// Notificar
		if (m_OnLicenseUnlocked)
			m_OnLicenseUnlocked.Invoke(licenseType);
		
		// Mostrar notificación al jugador
		if (!isFree)
		{
			string message = WidgetManager.Translate("#LF-License_Granted", 
				WidgetManager.Translate(config.m_sLicenseName), WidgetManager.Translate(config.m_sDescription));
			SCR_HintManagerComponent.ShowCustomHint(message, WidgetManager.Translate("#LF-License_Granted_Title"), 5.0);
		}
		
		Replication.BumpMe();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si tiene una licencia
	bool HasLicense(LF_ELicenseType licenseType)
	{
		if (!Replication.IsServer())
			return LF_LicenseBits.Has(m_iUnlockedBits, licenseType);

		return m_aUnlockedLicenses.Contains(licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener todas las licencias desbloqueadas
	array<LF_ELicenseType> GetUnlockedLicenses()
	{
		// A client has only the replicated mask; rebuild the list from it on read.
		if (!Replication.IsServer())
			LF_LicenseBits.Decode(m_iUnlockedBits, m_aUnlockedLicenses);

		return m_aUnlockedLicenses;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Establecer licencias desbloqueadas (para persistencia)
	void SetUnlockedLicenses(array<LF_ELicenseType> licenses)
	{
		m_aUnlockedLicenses.Clear();
		foreach (LF_ELicenseType license : licenses)
		{
			m_aUnlockedLicenses.Insert(license);
		}
		m_iUnlockedBits = LF_LicenseBits.Encode(m_aUnlockedLicenses);
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener configuración de una licencia
	LF_LicenseConfig GetLicenseConfig(LF_ELicenseType licenseType)
	{
		return m_mLicenseConfigs.Get(licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener todas las configuraciones de licencias
	map<LF_ELicenseType, ref LF_LicenseConfig> GetAllLicenseConfigs()
	{
		return m_mLicenseConfigs;
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnLicenseUnlocked()
	{
		if (!m_OnLicenseUnlocked)
			m_OnLicenseUnlocked = new ScriptInvoker();
		return m_OnLicenseUnlocked;
	}
}
