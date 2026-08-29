//------------------------------------------------------------------------------------------------
//! Tipos de licencias/habilidades disponibles - Sincronizadas con trabajos
enum EL_ELicenseType
{
	// INICIAL - Gratuito
	UNEMPLOYED = 0,
	
	// TRABAJO: RECOLECTOR (FARMER) - Frutos y Agua
	FARMER_TOMATO = 1,      // Inicial - Recolección de tomates (gratis)
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
class EL_LicenseConfig
{
	[Attribute(defvalue: "0", UIWidgets.ComboBox, "License type", "", ParamEnumArray.FromEnum(EL_ELicenseType))]
	EL_ELicenseType m_eLicenseType;
	
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
class EL_LicenseManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_LicenseManagerComponent : ScriptComponent
{
	[RplProp()]
	protected ref array<EL_ELicenseType> m_aUnlockedLicenses = new array<EL_ELicenseType>();
	
	protected ref ScriptInvoker m_OnLicenseUnlocked;
	protected ref map<EL_ELicenseType, ref EL_LicenseConfig> m_mLicenseConfigs;
	
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
		
		Print(string.Format("[EL_LicenseManagerComponent] Player has %1 licenses unlocked", 
			m_aUnlockedLicenses.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void InitializeLicenseConfigs()
	{
		m_mLicenseConfigs = new map<EL_ELicenseType, ref EL_LicenseConfig>();
		
		// ============= LICENCIAS INICIALES (GRATUITAS) =============
		EL_LicenseConfig unemployed = new EL_LicenseConfig();
		unemployed.m_eLicenseType = EL_ELicenseType.UNEMPLOYED;
		unemployed.m_sLicenseName = "Desempleado";
		unemployed.m_sDescription = "Sin trabajo activo";
		unemployed.m_iSkillPointCost = 0;
		unemployed.m_iMinLevelRequired = 1;
		unemployed.m_bIsInitialLicense = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.UNEMPLOYED, unemployed);
		
		// ============= TRABAJO: RECOLECTOR (FARMER) =============
		EL_LicenseConfig farmerTomato = new EL_LicenseConfig();
		farmerTomato.m_eLicenseType = EL_ELicenseType.FARMER_TOMATO;
		farmerTomato.m_sLicenseName = "Recolector de Tomates";
		farmerTomato.m_sDescription = "Licencia inicial para RECOLECTOR. Permite recolectar tomates.";
		farmerTomato.m_iSkillPointCost = 0;
		farmerTomato.m_iMinLevelRequired = 1;
		farmerTomato.m_bIsInitialLicense = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.FARMER_TOMATO, farmerTomato);
		
		EL_LicenseConfig farmerApple = new EL_LicenseConfig();
		farmerApple.m_eLicenseType = EL_ELicenseType.FARMER_APPLE;
		farmerApple.m_sLicenseName = "Hortelano Junior";
		farmerApple.m_sDescription = "Permite recolectar manzanas de huertos. Requiere trabajo RECOLECTOR.";
		farmerApple.m_iSkillPointCost = 1;
		farmerApple.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(EL_ELicenseType.FARMER_APPLE, farmerApple);
		
		EL_LicenseConfig farmerPlum = new EL_LicenseConfig();
		farmerPlum.m_eLicenseType = EL_ELicenseType.FARMER_PLUM;
		farmerPlum.m_sLicenseName = "Recolector de Ciruelas";
		farmerPlum.m_sDescription = "Permite recolectar ciruelas de árboles. Requiere trabajo RECOLECTOR.";
		farmerPlum.m_iSkillPointCost = 2;
		farmerPlum.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.FARMER_PLUM, farmerPlum);
		
		EL_LicenseConfig farmerWater = new EL_LicenseConfig();
		farmerWater.m_eLicenseType = EL_ELicenseType.FARMER_WATER;
		farmerWater.m_sLicenseName = "Recolector de Agua";
		farmerWater.m_sDescription = "Permite recolectar agua purificada. Requiere trabajo RECOLECTOR.";
		farmerWater.m_iSkillPointCost = 1;
		farmerWater.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(EL_ELicenseType.FARMER_WATER, farmerWater);
		
		EL_LicenseConfig farmerSpeed = new EL_LicenseConfig();
		farmerSpeed.m_eLicenseType = EL_ELicenseType.FARMER_SPEED;
		farmerSpeed.m_sLicenseName = "Manos Ágiles";
		farmerSpeed.m_sDescription = "x2 velocidad al recolectar frutas y agua. Requiere trabajo RECOLECTOR.";
		farmerSpeed.m_iSkillPointCost = 3;
		farmerSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.FARMER_SPEED, farmerSpeed);
		
		// ============= TRABAJO: MINERO (MINER) =============
		EL_LicenseConfig minerCoal = new EL_LicenseConfig();
		minerCoal.m_eLicenseType = EL_ELicenseType.MINER_COAL;
		minerCoal.m_sLicenseName = "Minero de Carbón";
		minerCoal.m_sDescription = "Permite extraer carbón con pico. Requiere trabajo MINERO.";
		minerCoal.m_iSkillPointCost = 1;
		minerCoal.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(EL_ELicenseType.MINER_COAL, minerCoal);
		
		EL_LicenseConfig minerCopper = new EL_LicenseConfig();
		minerCopper.m_eLicenseType = EL_ELicenseType.MINER_COPPER;
		minerCopper.m_sLicenseName = "Minero de Cobre";
		minerCopper.m_sDescription = "Permite extraer cobre con pico. Se puede fundir en lingotes. Requiere trabajo MINERO.";
		minerCopper.m_iSkillPointCost = 2;
		minerCopper.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.MINER_COPPER, minerCopper);
		
		EL_LicenseConfig minerIron = new EL_LicenseConfig();
		minerIron.m_eLicenseType = EL_ELicenseType.MINER_IRON;
		minerIron.m_sLicenseName = "Minero de Hierro";
		minerIron.m_sDescription = "Permite extraer hierro con pico. Metal industrial. Requiere trabajo MINERO.";
		minerIron.m_iSkillPointCost = 2;
		minerIron.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.MINER_IRON, minerIron);
		
		EL_LicenseConfig minerGold = new EL_LicenseConfig();
		minerGold.m_eLicenseType = EL_ELicenseType.MINER_GOLD;
		minerGold.m_sLicenseName = "Minero de Oro";
		minerGold.m_sDescription = "Permite extraer oro con pico. Se puede fundir en lingotes de alto valor. Requiere trabajo MINERO.";
		minerGold.m_iSkillPointCost = 3;
		minerGold.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(EL_ELicenseType.MINER_GOLD, minerGold);
		
		EL_LicenseConfig minerSpeed = new EL_LicenseConfig();
		minerSpeed.m_eLicenseType = EL_ELicenseType.MINER_SPEED;
		minerSpeed.m_sLicenseName = "Pico Veloz";
		minerSpeed.m_sDescription = "x2 velocidad al minar cualquier recurso. Requiere trabajo MINERO.";
		minerSpeed.m_iSkillPointCost = 3;
		minerSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.MINER_SPEED, minerSpeed);
		
		// ============= TRABAJO: LEÑADOR (LUMBERJACK) =============
		EL_LicenseConfig lumberjackBasic = new EL_LicenseConfig();
		lumberjackBasic.m_eLicenseType = EL_ELicenseType.LUMBERJACK_BASIC;
		lumberjackBasic.m_sLicenseName = "Leñador Básico";
		lumberjackBasic.m_sDescription = "Permite cortar árboles con hacha. Requiere trabajo LEÑADOR.";
		lumberjackBasic.m_iSkillPointCost = 1;
		lumberjackBasic.m_iMinLevelRequired = 2;
		m_mLicenseConfigs.Set(EL_ELicenseType.LUMBERJACK_BASIC, lumberjackBasic);
		
		EL_LicenseConfig lumberjackProcess = new EL_LicenseConfig();
		lumberjackProcess.m_eLicenseType = EL_ELicenseType.LUMBERJACK_PROCESS;
		lumberjackProcess.m_sLicenseName = "Procesador de Madera";
		lumberjackProcess.m_sDescription = "Permite procesar troncos en tablones en aserradero. Requiere trabajo LEÑADOR.";
		lumberjackProcess.m_iSkillPointCost = 2;
		lumberjackProcess.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.LUMBERJACK_PROCESS, lumberjackProcess);
		
		EL_LicenseConfig lumberjackSpeed = new EL_LicenseConfig();
		lumberjackSpeed.m_eLicenseType = EL_ELicenseType.LUMBERJACK_SPEED;
		lumberjackSpeed.m_sLicenseName = "Hacha Veloz";
		lumberjackSpeed.m_sDescription = "x2 velocidad al cortar árboles. Requiere trabajo LEÑADOR.";
		lumberjackSpeed.m_iSkillPointCost = 3;
		lumberjackSpeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.LUMBERJACK_SPEED, lumberjackSpeed);
		
		// ============= TRABAJO: EMS (MEDIC) =============
		EL_LicenseConfig medicAccess = new EL_LicenseConfig();
		medicAccess.m_eLicenseType = EL_ELicenseType.MEDIC_ACCESS;
		medicAccess.m_sLicenseName = "Credencial EMS";
		medicAccess.m_sDescription = "Acreditación oficial de EMS. Permite entrar de servicio.";
		medicAccess.m_iSkillPointCost = 0;
		medicAccess.m_iMinLevelRequired = 1;
		medicAccess.m_bRequiresWhitelist = true; // Assuming EMS also needs whitelist or at least license
		m_mLicenseConfigs.Set(EL_ELicenseType.MEDIC_ACCESS, medicAccess);

		EL_LicenseConfig medicBasic = new EL_LicenseConfig();
		medicBasic.m_eLicenseType = EL_ELicenseType.MEDIC_BASIC;
		medicBasic.m_sLicenseName = "Paramédico Básico";
		medicBasic.m_sDescription = "Permite curar jugadores heridos (+50 HP). Requiere trabajo EMS.";
		medicBasic.m_iSkillPointCost = 2;
		medicBasic.m_iMinLevelRequired = 3;
		m_mLicenseConfigs.Set(EL_ELicenseType.MEDIC_BASIC, medicBasic);
		
		EL_LicenseConfig medicAdvanced = new EL_LicenseConfig();
		medicAdvanced.m_eLicenseType = EL_ELicenseType.MEDIC_ADVANCED;
		medicAdvanced.m_sLicenseName = "Paramédico Avanzado";
		medicAdvanced.m_sDescription = "Curación mejorada (+100 HP). Requiere trabajo EMS.";
		medicAdvanced.m_iSkillPointCost = 3;
		medicAdvanced.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(EL_ELicenseType.MEDIC_ADVANCED, medicAdvanced);
		
		EL_LicenseConfig medicRevive = new EL_LicenseConfig();
		medicRevive.m_eLicenseType = EL_ELicenseType.MEDIC_REVIVE;
		medicRevive.m_sLicenseName = "Especialista en Reanimación";
		medicRevive.m_sDescription = "Permite revivir jugadores inconscientes. Requiere trabajo EMS.";
		medicRevive.m_iSkillPointCost = 4;
		medicRevive.m_iMinLevelRequired = 8;
		m_mLicenseConfigs.Set(EL_ELicenseType.MEDIC_REVIVE, medicRevive);
		
		// ============= TRABAJO: POLICÍA (POLICE) - Requiere Whitelist =============
		EL_LicenseConfig policeAccess = new EL_LicenseConfig();
		policeAccess.m_eLicenseType = EL_ELicenseType.POLICE_ACCESS;
		policeAccess.m_sLicenseName = "Placa Policial";
		policeAccess.m_sDescription = "Acreditación oficial de policía. Permite entrar de servicio.";
		policeAccess.m_iSkillPointCost = 0;
		policeAccess.m_iMinLevelRequired = 1;
		policeAccess.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.POLICE_ACCESS, policeAccess);

		EL_LicenseConfig policeArrest = new EL_LicenseConfig();
		policeArrest.m_eLicenseType = EL_ELicenseType.POLICE_ARREST;
		policeArrest.m_sLicenseName = "Oficial de Arresto";
		policeArrest.m_sDescription = "Permite arrestar criminales. Requiere trabajo POLICÍA y whitelist.";
		policeArrest.m_iSkillPointCost = 2;
		policeArrest.m_iMinLevelRequired = 5;
		policeArrest.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.POLICE_ARREST, policeArrest);
		
		EL_LicenseConfig policeFine = new EL_LicenseConfig();
		policeFine.m_eLicenseType = EL_ELicenseType.POLICE_FINE;
		policeFine.m_sLicenseName = "Oficial de Multas";
		policeFine.m_sDescription = "Permite multar infractores. Requiere trabajo POLICÍA y whitelist.";
		policeFine.m_iSkillPointCost = 2;
		policeFine.m_iMinLevelRequired = 5;
		policeFine.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.POLICE_FINE, policeFine);
		
		EL_LicenseConfig policeSearch = new EL_LicenseConfig();
		policeSearch.m_eLicenseType = EL_ELicenseType.POLICE_SEARCH;
		policeSearch.m_sLicenseName = "Oficial de Registro";
		policeSearch.m_sDescription = "Permite registrar jugadores sospechosos. Requiere trabajo POLICÍA y whitelist.";
		policeSearch.m_iSkillPointCost = 3;
		policeSearch.m_iMinLevelRequired = 7;
		policeSearch.m_bRequiresWhitelist = true;
		m_mLicenseConfigs.Set(EL_ELicenseType.POLICE_SEARCH, policeSearch);
		
		// ============= RAMA CRIMINAL - Actividades Ilegales =============
		EL_LicenseConfig criminalWeed = new EL_LicenseConfig();
		criminalWeed.m_eLicenseType = EL_ELicenseType.CRIMINAL_WEED;
		criminalWeed.m_sLicenseName = "Cultivador de Marihuana";
		criminalWeed.m_sDescription = "Permite cultivar y cosechar marihuana. Alto riesgo, alta recompensa. Ilegal.";
		criminalWeed.m_iSkillPointCost = 3;
		criminalWeed.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_WEED, criminalWeed);
		
		EL_LicenseConfig criminalRobbery = new EL_LicenseConfig();
		criminalRobbery.m_eLicenseType = EL_ELicenseType.CRIMINAL_ROBBERY;
		criminalRobbery.m_sLicenseName = "Ladrón Profesional";
		criminalRobbery.m_sDescription = "Permite robar tiendas, bancos y otras locaciones. Policía puede arrestar.";
		criminalRobbery.m_iSkillPointCost = 3;
		criminalRobbery.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_ROBBERY, criminalRobbery);
		
		EL_LicenseConfig criminalBlackmarket = new EL_LicenseConfig();
		criminalBlackmarket.m_eLicenseType = EL_ELicenseType.CRIMINAL_BLACKMARKET;
		criminalBlackmarket.m_sLicenseName = "Contacto del Mercado Negro";
		criminalBlackmarket.m_sDescription = "Acceso al mercado negro. Permite comprar armas, vehículos y ropa ilegal.";
		criminalBlackmarket.m_iSkillPointCost = 2;
		criminalBlackmarket.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_BLACKMARKET, criminalBlackmarket);
		
		EL_LicenseConfig criminalWeapons = new EL_LicenseConfig();
		criminalWeapons.m_eLicenseType = EL_ELicenseType.CRIMINAL_WEAPONS;
		criminalWeapons.m_sLicenseName = "Traficante de Armas";
		criminalWeapons.m_sDescription = "Permite comprar armas ilegales en el mercado negro. Requiere Contacto del Mercado Negro.";
		criminalWeapons.m_iSkillPointCost = 4;
		criminalWeapons.m_iMinLevelRequired = 6;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_WEAPONS, criminalWeapons);
		
		EL_LicenseConfig criminalVehicles = new EL_LicenseConfig();
		criminalVehicles.m_eLicenseType = EL_ELicenseType.CRIMINAL_VEHICLES;
		criminalVehicles.m_sLicenseName = "Contrabandista de Vehículos";
		criminalVehicles.m_sDescription = "Permite comprar vehículos modificados/robados. Requiere Contacto del Mercado Negro.";
		criminalVehicles.m_iSkillPointCost = 3;
		criminalVehicles.m_iMinLevelRequired = 5;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_VEHICLES, criminalVehicles);
		
		EL_LicenseConfig criminalClothes = new EL_LicenseConfig();
		criminalClothes.m_eLicenseType = EL_ELicenseType.CRIMINAL_CLOTHES;
		criminalClothes.m_sLicenseName = "Cliente VIP Textil";
		criminalClothes.m_sDescription = "Permite comprar ropa exclusiva del mercado negro. Requiere Contacto del Mercado Negro.";
		criminalClothes.m_iSkillPointCost = 2;
		criminalClothes.m_iMinLevelRequired = 4;
		m_mLicenseConfigs.Set(EL_ELicenseType.CRIMINAL_CLOTHES, criminalClothes);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void GiveInitialLicenses()
	{
		// Dar licencias iniciales gratuitas
		UnlockLicense(EL_ELicenseType.UNEMPLOYED, true);
		UnlockLicense(EL_ELicenseType.FARMER_TOMATO, true);
		
		Print("[EL_LicenseManagerComponent] Initial licenses granted", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si el jugador puede desbloquear una licencia
	bool CanUnlockLicense(EL_ELicenseType licenseType)
	{
		// Ya desbloqueada
		if (HasLicense(licenseType))
			return false;
		
		EL_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
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
			
			// Verificar whitelist para trabajos policiales
			if (licenseType == EL_ELicenseType.POLICE_ARREST || 
			    licenseType == EL_ELicenseType.POLICE_FINE || 
			    licenseType == EL_ELicenseType.POLICE_SEARCH)
			{
				if (!EL_WhitelistManager.IsWhitelistedForJob(playerUID, EL_EJobType.POLICE))
					return false;
			}
		}
		
		// Verificar nivel mínimo
		IEntity owner = GetOwner();
		if (owner)
		{
			EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
			if (levelComp && levelComp.GetPlayerLevel() < config.m_iMinLevelRequired)
				return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si el jugador tiene puntos suficientes para comprar
	bool CanAffordLicense(EL_ELicenseType licenseType)
	{
		EL_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
			return false;
		
		IEntity owner = GetOwner();
		if (!owner)
			return false;
		
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
		if (!levelComp)
			return false;
		
		return levelComp.GetSkillPoints() >= config.m_iSkillPointCost;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Comprar/desbloquear una licencia
	bool PurchaseLicense(EL_ELicenseType licenseType)
	{
		if (!CanUnlockLicense(licenseType))
		{
			EL_Debug.Log("License", string.Format("purchase rejected (requirements not met): %1", typename.EnumToString(EL_ELicenseType, licenseType)));
			return false;
		}

		if (!CanAffordLicense(licenseType))
		{
			EL_Debug.Log("License", string.Format("purchase rejected (cannot afford): %1", typename.EnumToString(EL_ELicenseType, licenseType)));
			return false;
		}
		
		EL_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
			return false;
		
		// Descontar puntos de habilidad
		IEntity owner = GetOwner();
		if (owner)
		{
			EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
			if (levelComp)
			{
				levelComp.SpendSkillPoints(config.m_iSkillPointCost);
			}
		}
		
		// Desbloquear licencia
		return UnlockLicense(licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Desbloquear una licencia
	bool UnlockLicense(EL_ELicenseType licenseType, bool isFree = false)
	{
if (!Replication.IsServer())
			return false;

		// Ya tiene esta licencia
		if (HasLicense(licenseType))
		{
			EL_Debug.Log("License", string.Format("unlock rejected (already owned): %1", typename.EnumToString(EL_ELicenseType, licenseType)));
			return false;
		}
		
		EL_LicenseConfig config = m_mLicenseConfigs.Get(licenseType);
		if (!config)
		{
			EL_Debug.Log("License", string.Format("unlock rejected (no config): %1", typename.EnumToString(EL_ELicenseType, licenseType)));
			return false;
		}
		
		// Si no es gratis, verificar requisitos y gastar SP
		if (!isFree)
		{
			IEntity owner = GetOwner();
			if (!owner)
				return false;
			
			// Verificar nivel
			EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
			if (!levelComp)
			{
				Print("[EL_LicenseManagerComponent] Player level component not found!", LogLevel.ERROR);
				return false;
			}
			
			if (levelComp.GetLevel() < config.m_iMinLevelRequired)
			{
				Print(string.Format("[EL_LicenseManagerComponent] Player level %1 is below required level %2", 
					levelComp.GetLevel(), config.m_iMinLevelRequired), LogLevel.WARNING);
				return false;
			}
			
			// Verificar whitelist si es necesario
			if (config.m_bRequiresWhitelist)
			{
				string playerUID = EL_Utils.GetPlayerUID(owner);
				if (!EL_WhitelistManager.IsWhitelistedForJob(playerUID, EL_EJobType.POLICE))
				{
					Print("[EL_LicenseManagerComponent] Player not whitelisted for this license", LogLevel.WARNING);
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
		
		Print(string.Format("[EL_LicenseManagerComponent] License unlocked: %1", config.m_sLicenseName), LogLevel.NORMAL);
		
		// Notificar
		if (m_OnLicenseUnlocked)
			m_OnLicenseUnlocked.Invoke(licenseType);
		
		// Mostrar notificación al jugador
		if (!isFree)
		{
			string message = string.Format("¡Licencia desbloqueada!\n%1\n\n%2", 
				config.m_sLicenseName, config.m_sDescription);
			SCR_HintManagerComponent.ShowCustomHint(message, "NUEVA PROFESIÓN", 5.0);
		}
		
		Replication.BumpMe();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Verificar si tiene una licencia
	bool HasLicense(EL_ELicenseType licenseType)
	{
		return m_aUnlockedLicenses.Contains(licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener todas las licencias desbloqueadas
	array<EL_ELicenseType> GetUnlockedLicenses()
	{
		return m_aUnlockedLicenses;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Establecer licencias desbloqueadas (para persistencia)
	void SetUnlockedLicenses(array<EL_ELicenseType> licenses)
	{
		m_aUnlockedLicenses.Clear();
		foreach (EL_ELicenseType license : licenses)
		{
			m_aUnlockedLicenses.Insert(license);
		}
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener configuración de una licencia
	EL_LicenseConfig GetLicenseConfig(EL_ELicenseType licenseType)
	{
		return m_mLicenseConfigs.Get(licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener todas las configuraciones de licencias
	map<EL_ELicenseType, ref EL_LicenseConfig> GetAllLicenseConfigs()
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
