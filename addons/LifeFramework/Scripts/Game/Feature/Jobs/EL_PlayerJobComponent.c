//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Jobs", description: "Manages player's job, salary and work statistics")]
class EL_PlayerJobComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_PlayerJobComponent : ScriptComponent
{
	[RplProp(onRplName: "OnJobChanged")]
	protected EL_EJobType m_eCurrentJob;
	
	[RplProp()]
	protected int m_iJobLevel;
	
	[RplProp()]
	protected float m_fJobExperience;
	
	// Mapa de niveles por trabajo (para persistencia completa)
	protected ref map<EL_EJobType, int> m_mJobLevels;
	protected ref map<EL_EJobType, float> m_mJobExperience;
	
	protected float m_fTimeSinceLastPaycheck;
	protected float m_fTotalWorkTime;
	protected int m_iTotalEarnings;
	
	protected ref ScriptInvoker m_OnJobChanged;
	protected ref ScriptInvoker m_OnPaycheckReceived;
	protected ref ScriptInvoker m_OnLevelUp;
	protected ref ScriptInvoker m_OnJobExperienceGained;
	
	protected ref EL_JobConfig m_CurrentJobConfig;
	protected ref map<EL_EJobType, ref EL_JobConfig> m_mJobConfigs;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
		m_eCurrentJob = EL_EJobType.UNEMPLOYED;
		m_iJobLevel = 1;
		m_fJobExperience = 0;
		
		// Inicializar mapas de niveles/XP por trabajo
		m_mJobLevels = new map<EL_EJobType, int>();
		m_mJobExperience = new map<EL_EJobType, float>();
		
		InitializeJobConfigs();
	}
	
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected void InitializeJobConfigs()
	{
		m_mJobConfigs = new map<EL_EJobType, ref EL_JobConfig>();
		
		// Initialize default job configs
		// CIVILIANS (Base 700)
		EL_JobConfig unemployedConfig = new EL_JobConfig();
		unemployedConfig.m_eJobType = EL_EJobType.UNEMPLOYED;
		unemployedConfig.m_sJobName = "Civil";
		unemployedConfig.m_iBaseSalary = 700;
		unemployedConfig.m_fPaycheckInterval = 300; // 5 minutes
		m_mJobConfigs.Set(EL_EJobType.UNEMPLOYED, unemployedConfig);
		
		EL_JobConfig farmerConfig = new EL_JobConfig();
		farmerConfig.m_eJobType = EL_EJobType.FARMER;
		farmerConfig.m_sJobName = "Civil (Granjero)";
		farmerConfig.m_iBaseSalary = 700;
		farmerConfig.m_fPaycheckInterval = 300;
		farmerConfig.m_bCanGatherResources = true;
		farmerConfig.m_fGatheringSpeedMultiplier = 1.0;
		farmerConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds";
		m_mJobConfigs.Set(EL_EJobType.FARMER, farmerConfig);
		
		EL_JobConfig minerConfig = new EL_JobConfig();
		minerConfig.m_eJobType = EL_EJobType.MINER;
		minerConfig.m_sJobName = "Civil (Minero)";
		minerConfig.m_iBaseSalary = 700;
		minerConfig.m_fPaycheckInterval = 300;
		minerConfig.m_bCanGatherResources = true;
		minerConfig.m_fGatheringSpeedMultiplier = 1.0;
		minerConfig.m_aEquipmentPrefabs = {"{E2A803C6E2B7B3E6}Prefabs/Tools/Axe/Pico.et"};
		minerConfig.m_sIcon = "{49C8B8FB3D34F654}Images/minero.edds";
		m_mJobConfigs.Set(EL_EJobType.MINER, minerConfig);
		
		EL_JobConfig lumberjackConfig = new EL_JobConfig();
		lumberjackConfig.m_eJobType = EL_EJobType.LUMBERJACK;
		lumberjackConfig.m_sJobName = "Civil (Leñador)";
		lumberjackConfig.m_iBaseSalary = 700;
		lumberjackConfig.m_fPaycheckInterval = 300;
		lumberjackConfig.m_bCanGatherResources = true;
		lumberjackConfig.m_fGatheringSpeedMultiplier = 1.0;
		lumberjackConfig.m_aEquipmentPrefabs = {"{92FAC15304387B6F}Prefabs/Tools/Axe/Axe.et"};
		lumberjackConfig.m_sIcon = "{49C8B8FB3D34F654}Images/minero.edds"; // TODO: Update icon
		m_mJobConfigs.Set(EL_EJobType.LUMBERJACK, lumberjackConfig);
		
		// PUBLIC SERVANTS (Base 1000)
		EL_JobConfig medicConfig = new EL_JobConfig();
		medicConfig.m_eJobType = EL_EJobType.MEDIC;
		medicConfig.m_sJobName = "EMS";
		medicConfig.m_sDescription = "Paramédico de emergencias. Cura jugadores y gana recompensas.";
		medicConfig.m_iBaseSalary = 1000;
		medicConfig.m_fPaycheckInterval = 300;
		medicConfig.m_bCanGatherResources = false;
		medicConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds"; // Same icon as Police for now
		m_mJobConfigs.Set(EL_EJobType.MEDIC, medicConfig);
		
		EL_JobConfig policeConfig = new EL_JobConfig();
		policeConfig.m_eJobType = EL_EJobType.POLICE;
		policeConfig.m_sJobName = "Policía";
		policeConfig.m_sDescription = "Oficial de policía. Requiere whitelist de Discord.";
		policeConfig.m_iBaseSalary = 1000;
		policeConfig.m_fPaycheckInterval = 300;
		policeConfig.m_bCanGatherResources = false;
		policeConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds"; // TODO: Update icon
		m_mJobConfigs.Set(EL_EJobType.POLICE, policeConfig);
		
		m_CurrentJobConfig = m_mJobConfigs.Get(m_eCurrentJob);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Only update on server
		if (!Replication.IsServer() && Replication.IsRunning())
			return;
		
		if (!m_CurrentJobConfig)
			return;
		
		m_fTimeSinceLastPaycheck += timeSlice;
		m_fTotalWorkTime += timeSlice;
		
		// Check for paycheck
		if (m_fTimeSinceLastPaycheck >= m_CurrentJobConfig.m_fPaycheckInterval)
		{
			ProcessPaycheck();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set player's job
	bool SetJob(EL_EJobType jobType)
	{
		if (m_eCurrentJob == jobType)
			return true;
		
		// Special check for POLICE job - requires POLICE_ACCESS license
		if (jobType == EL_EJobType.POLICE)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.POLICE_ACCESS))
			{
				SCR_HintManagerComponent.ShowCustomHint(
					"Necesitas la Placa Policial para entrar de servicio.\n\nObtén la licencia en la comisaría.",
					"Acceso Denegado",
					5.0
				);
				return false;
			}
		}
		
		// Special check for MEDIC job - requires MEDIC_ACCESS license
		if (jobType == EL_EJobType.MEDIC)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.MEDIC_ACCESS))
			{
				SCR_HintManagerComponent.ShowCustomHint(
					"Necesitas la Credencial EMS para entrar de servicio.\n\nObtén la licencia en el hospital.",
					"Acceso Denegado",
					5.0
				);
				return false;
			}
		}
		
		// Check whitelist for restricted jobs. POLICE is the only restricted job and
		// requires both the whitelist and the POLICE_ACCESS license; the license
		// grant is itself whitelist-gated, so this is defense in depth.
		if (EL_WhitelistManager.IsJobRestricted(jobType))
		{
			IEntity owner = GetOwner();
			if (!owner)
				return false;
			
			string playerUID = EL_Utils.GetPlayerUID(owner);
			if (!EL_WhitelistManager.IsWhitelistedForJob(playerUID, jobType))
			{
				SCR_HintManagerComponent.ShowCustomHint(
					"You are not whitelisted for this job. Contact an administrator.",
					"Access Denied"
				);
				return false;
			}
		}
		
		// Guardar progreso del trabajo actual antes de cambiar
		if (m_eCurrentJob != EL_EJobType.UNEMPLOYED)
		{
			m_mJobLevels.Set(m_eCurrentJob, m_iJobLevel);
			m_mJobExperience.Set(m_eCurrentJob, m_fJobExperience);
		}
		
		// Cambiar al nuevo trabajo
		m_eCurrentJob = jobType;
		m_fTimeSinceLastPaycheck = 0;
		
		// Cargar progreso del nuevo trabajo (si existe)
		if (m_mJobLevels.Contains(jobType))
		{
			m_iJobLevel = m_mJobLevels.Get(jobType);
			m_fJobExperience = m_mJobExperience.Get(jobType);
		}
		else
		{
			// Primer vez en este trabajo, empezar en nivel 1
			m_iJobLevel = 1;
			m_fJobExperience = 0;
			m_mJobLevels.Set(jobType, 1);
			m_mJobExperience.Set(jobType, 0);
		}
		
		m_CurrentJobConfig = m_mJobConfigs.Get(jobType);
		
		OnJobChanged();
		Replication.BumpMe();

		EL_Debug.Log("Jobs", string.Format("job set -> %1 (level %2)", typename.EnumToString(EL_EJobType, jobType), m_iJobLevel));
		
		// Show notification for job change
		string jobName = GetJobName();
		string message;
		string title;
		
		if (jobType == EL_EJobType.UNEMPLOYED)
		{
			message = "Has finalizado tu turno de servicio.";
			title = "FUERA DE SERVICIO";
		}
		else if (jobType == EL_EJobType.POLICE)
		{
			message = string.Format("¡Estás de servicio como %1!\n\n10-4, oficial en patrulla.", jobName);
			title = "POLICÍA EN SERVICIO";
		}
		else if (jobType == EL_EJobType.MEDIC)
		{
			message = string.Format("¡Estás de servicio como %1!\n\nUnidad médica operativa.", jobName);
			title = "EMS EN SERVICIO";
		}
		else
		{
			message = string.Format("Ahora trabajas como: %1\nNivel: %2 | XP: %.0f", jobName, m_iJobLevel, m_fJobExperience);
			title = "TRABAJO CAMBIADO";
		}
		
		SCR_HintManagerComponent.ShowCustomHint(message, title, 4.0);
		
		// Give job equipment
		GiveJobEquipment();
		
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void AskSetJob(EL_EJobType jobType)
	{
		Rpc(RpcAsk_SetJob, jobType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetJob(EL_EJobType jobType)
	{
		// Security check: Only allow switching to POLICE if they have the license
		if (jobType == EL_EJobType.POLICE)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.POLICE_ACCESS))
			{
				EL_Debug.Warn("Jobs", "security: POLICE job request denied (no POLICE_ACCESS license)");
				return;
			}
		}
		
		// Security check: Only allow switching to MEDIC if they have the license
		if (jobType == EL_EJobType.MEDIC)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.MEDIC_ACCESS))
			{
				EL_Debug.Warn("Jobs", "security: MEDIC job request denied (no MEDIC_ACCESS license)");
				return;
			}
		}
		
		SetJob(jobType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current job type
	EL_EJobType GetJob()
	{
		return m_eCurrentJob;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get job name
	string GetJobName()
	{
		if (!m_CurrentJobConfig)
			return "Unknown";
		
		return m_CurrentJobConfig.m_sJobName;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current job config
	EL_JobConfig GetCurrentJobConfig()
	{
		return m_CurrentJobConfig;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current job level
	int GetJobLevel()
	{
		return m_iJobLevel;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current job experience
	float GetJobExperience()
	{
		return m_fJobExperience;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add job experience (used when performing job actions)
	void AddExperience(float amount)
	{
		// Job-level XP is deprecated by design: progression runs through
		// EL_PlayerLevelComponent (paycheck already grants +5 player XP). The
		// per-job level/XP maps have no SaveData pair, so wiring this would
		// resurrect an unpersisted progression path whose level-up threshold
		// does not exist in EL_JobConfig.
		return;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get gathering speed multiplier for current job
	float GetGatheringSpeedMultiplier()
	{
		// DEPRECATED: Gathering speed is now handled by player level
		return 1.0;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if current job can gather resources
	bool CanGatherResources()
	{
		if (!m_CurrentJobConfig)
			return false;
		
		return m_CurrentJobConfig.m_bCanGatherResources;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Process paycheck for player
	protected void ProcessPaycheck()
	{
		m_fTimeSinceLastPaycheck = 0;
		
		if (!m_CurrentJobConfig)
			return;
		
		// Calculate salary with PLAYER LEVEL bonus
		int salary = m_CurrentJobConfig.m_iBaseSalary;
		
		// Get player level
		int playerLevel = 1;
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(GetOwner());
		if (levelComp)
		{
			playerLevel = levelComp.GetPlayerLevel();
		}
		
		// Bonus: +5% per player level
		float levelBonus = 1.0 + ((playerLevel - 1) * 0.05);
		int totalPay = Math.Round(salary * levelBonus);
		
		m_iTotalEarnings += totalPay;

		// Cash is the only payout. The bank balance moves exclusively through the
		// ATM (Feature/ATM): a paycheck landing straight in the bank would bypass
		// the physical-money boundary. Inventory-full just shortchanges the
		// payout, it never mints.
		IEntity owner = GetOwner();
		if (owner)
		{
			int paid = EL_MoneyUtils.GiveCash(owner, totalPay);
			if (paid != totalPay)
				EL_Debug.Warn("Jobs", string.Format("paycheck payout short: paid %1 of %2 (player %3)", paid, totalPay, owner));

			// Give Paycheck XP (+5)
			if (levelComp)
			{
				levelComp.AddExperience(5.0, "Paycheck");
			}
		}
		
		OnPaycheckReceived(totalPay);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Give equipment for current job
	protected void GiveJobEquipment()
	{
		if (!m_CurrentJobConfig || !m_CurrentJobConfig.m_aEquipmentPrefabs)
			return;
		
		IEntity owner = GetOwner();
		if (!owner)
			return;
		
		InventoryStorageManagerComponent inventory = EL_Component<InventoryStorageManagerComponent>.Find(owner);
		if (!inventory)
			return;
		
		// Spawn and give equipment items
		foreach (ResourceName prefab : m_CurrentJobConfig.m_aEquipmentPrefabs)
		{
			if (prefab.IsEmpty())
				continue;
			
			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.TransformMode = ETransformMode.WORLD;
			owner.GetWorldTransform(spawnParams.Transform);
			
			IEntity equipment = GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, spawnParams);
			if (equipment)
			{
				inventory.TryInsertItem(equipment);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get map of all job levels (para persistencia)
	map<EL_EJobType, int> GetAllJobLevels()
	{
		// Guardar el nivel actual primero
		if (m_eCurrentJob != EL_EJobType.UNEMPLOYED)
			m_mJobLevels.Set(m_eCurrentJob, m_iJobLevel);
		
		return m_mJobLevels;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get map of all job experience (para persistencia)
	map<EL_EJobType, float> GetAllJobExperience()
	{
		// Guardar la XP actual primero
		if (m_eCurrentJob != EL_EJobType.UNEMPLOYED)
			m_mJobExperience.Set(m_eCurrentJob, m_fJobExperience);
		
		return m_mJobExperience;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set all job levels (para persistencia)
	void SetAllJobLevels(map<EL_EJobType, int> levels)
	{
		m_mJobLevels = levels;
		
		// Si el trabajo actual está en el mapa, actualizar
		if (m_mJobLevels.Contains(m_eCurrentJob))
			m_iJobLevel = m_mJobLevels.Get(m_eCurrentJob);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set all job experience (para persistencia)
	void SetAllJobExperience(map<EL_EJobType, float> experience)
	{
		m_mJobExperience = experience;
		
		// Si el trabajo actual está en el mapa, actualizar
		if (m_mJobExperience.Contains(m_eCurrentJob))
			m_fJobExperience = m_mJobExperience.Get(m_eCurrentJob);
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnJobChanged()
	{
		if (!m_OnJobChanged)
			m_OnJobChanged = new ScriptInvoker();
		
		return m_OnJobChanged;
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnPaycheckReceived()
	{
		if (!m_OnPaycheckReceived)
			m_OnPaycheckReceived = new ScriptInvoker();
		
		return m_OnPaycheckReceived;
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnLevelUp()
	{
		if (!m_OnLevelUp)
			m_OnLevelUp = new ScriptInvoker();
		
		return m_OnLevelUp;
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnJobExperienceGained()
	{
		if (!m_OnJobExperienceGained)
			m_OnJobExperienceGained = new ScriptInvoker();
		
		return m_OnJobExperienceGained;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnJobChanged()
	{
		if (m_OnJobChanged)
			m_OnJobChanged.Invoke(m_eCurrentJob);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnPaycheckReceived(int amount)
	{
		if (m_OnPaycheckReceived)
			m_OnPaycheckReceived.Invoke(amount);
		
		// Show notification indicating bank deposit
		string message = string.Format("Paycheck deposited to bank: $%1 (+5 XP)", amount);
		SCR_HintManagerComponent.ShowCustomHint(message, GetJobName(), 5.0);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnLevelUp()
	{
		if (m_OnLevelUp)
			m_OnLevelUp.Invoke(m_iJobLevel);
		
		// Calcular bonus de recolección
		float gatherBonus = (m_iJobLevel - 1) * 10.0; // 10% por nivel
		
		// Show notification con información del bonus
		string jobName = GetJobName();
		string message = string.Format("¡%1 NIVEL %2!\n\nBonus de recolección: +%.0f%%\nGanas más recursos al trabajar", 
			jobName, m_iJobLevel, gatherBonus);
		SCR_HintManagerComponent.ShowCustomHint(message, "¡SUBIDA DE NIVEL!", 6.0);
	}
	//------------------------------------------------------------------------------------------------
	//! Maximum score a single Fruit Catcher round can award. The minigame screen
	//! is not in this repo yet; this constant is the server-side round cap and
	//! must match the minigame's round length when it lands. The server clamps,
	//! so a mismatch shortchanges players but never mints.
	static const int EL_FRUIT_CATCHER_MAX_SCORE = 100;

	//------------------------------------------------------------------------------------------------
	//! Clamps a claimed Fruit Catcher score to the range a single round can award.
	static int EL_GetFruitCatcherRewardCount(int score)
	{
		if (score <= 0) return 0;
		if (score > EL_FRUIT_CATCHER_MAX_SCORE) return EL_FRUIT_CATCHER_MAX_SCORE;
		return score;
	}

	//------------------------------------------------------------------------------------------------
	//! Ask server to claim fruit catcher rewards
	void AskClaimFruitCatcherReward(int score, EHarvestJobType jobType)
	{
		Rpc(RpcAsk_ClaimFruitCatcherReward, score, jobType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ClaimFruitCatcherReward(int score, EHarvestJobType jobType)
	{
		// Server-side clamp: a forged score can never exceed one round's max
		int rewardCount = EL_GetFruitCatcherRewardCount(score);
		if (rewardCount <= 0) return;
		
		// Spawn Items
		ResourceName fruitPrefab;
		string fruitName;
		
		switch (jobType)
		{
			case EHarvestJobType.APPLES:
				fruitPrefab = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
				fruitName = "Manzanas";
				break;
			case EHarvestJobType.TOMATOES:
				fruitPrefab = "{0815D91FDF997A0A}Prefabs/Items/Food/Tomato.et";
				fruitName = "Tomates";
				break;
			case EHarvestJobType.PLUMS:
				fruitPrefab = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
				fruitName = "Ciruelas";
				break;
			case EHarvestJobType.BERRIES:
				fruitPrefab = "{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et";
				fruitName = "Bayas";
				break;
		}
		
		// Unknown fruit type gets nothing, not even XP
		if (!fruitPrefab) return;

		// Give XP (e.g. 5 XP per fruit)
		float xpAmount = rewardCount * 5.0;
		
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(GetOwner());
		if (levelComp)
		{
			levelComp.AddExperience(xpAmount, "Recolección de Frutas");
		}
		
		IEntity owner = GetOwner();
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(owner.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
		{
			Print("[EL_PlayerJobComponent] ERROR: No inventory found on player", LogLevel.ERROR);
			return;
		}
		
		int actuallyAdded = 0;
		
		// Use EXACT same pattern as trader system (EL_InventoryStorageManagerComponent.c line 24-27)
		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = owner.GetOrigin();
		
		for (int i = 0; i < rewardCount; i++)
		{
			// Spawn using SpawnEntityPrefabEx (same as trader system)
			IEntity fruitEntity = GetGame().SpawnEntityPrefabEx(fruitPrefab, false, null, spawnParams);
			if (!fruitEntity)
			{
				Print(string.Format("[EL_PlayerJobComponent] ERROR: Failed to spawn fruit %1", i), LogLevel.ERROR);
				continue;
			}
			
			// Try to insert using TryInsertItem (will go to any available storage)
			if (inventory.TryInsertItem(fruitEntity))
			{
				actuallyAdded++;
			}
			else
			{
				// If inventory full, item remains in world at spawn position
				Print(string.Format("[EL_PlayerJobComponent] Inventory full, fruit %1 dropped near player", i), LogLevel.WARNING);
			}
		}
		
		Print(string.Format("[EL_PlayerJobComponent] Added %1/%2 %3 to player inventory", 
			actuallyAdded, rewardCount, fruitName), LogLevel.NORMAL);
		
		// Notify
		string message = string.Format("¡Recibiste %1 %2 y %3 XP!", actuallyAdded, fruitName, xpAmount);
		SCR_HintManagerComponent.ShowCustomHint(message, "TRABAJO", 5.0);
	}
}
