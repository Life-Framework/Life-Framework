//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Jobs", description: "Manages player's job, level and work statistics")]
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
	
	protected ref ScriptInvoker m_OnJobChanged;
	protected ref ScriptInvoker m_OnLevelUp;
	protected ref ScriptInvoker m_OnJobExperienceGained;
	
	protected ref EL_JobConfig m_CurrentJobConfig;
	protected ref map<EL_EJobType, ref EL_JobConfig> m_mJobConfigs;

	//! World time (seconds) of the last accepted job switch. Negative = never.
	protected float m_fLastJobSwitchAt = -1;

	//! Server-side Fruit Catcher round ledger. The client never holds the truth.
	protected ref LF_FruitCatcherRound m_FruitRound = new LF_FruitCatcherRound();

	//! Client copy of the round id the server issued, sent back with the claim.
	protected int m_iClientFruitRoundId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
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
		EL_JobConfig unemployedConfig = new EL_JobConfig();
		unemployedConfig.m_eJobType = EL_EJobType.UNEMPLOYED;
		unemployedConfig.m_sJobName = "#LF-Job_Name_Unemployed";
		m_mJobConfigs.Set(EL_EJobType.UNEMPLOYED, unemployedConfig);
		
		EL_JobConfig farmerConfig = new EL_JobConfig();
		farmerConfig.m_eJobType = EL_EJobType.FARMER;
		farmerConfig.m_sJobName = "#LF-Job_Name_Farmer";
		farmerConfig.m_bCanGatherResources = true;
		farmerConfig.m_fGatheringSpeedMultiplier = 1.0;
		farmerConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds";
		m_mJobConfigs.Set(EL_EJobType.FARMER, farmerConfig);
		
		EL_JobConfig minerConfig = new EL_JobConfig();
		minerConfig.m_eJobType = EL_EJobType.MINER;
		minerConfig.m_sJobName = "#LF-Job_Name_Miner";
		minerConfig.m_bCanGatherResources = true;
		minerConfig.m_fGatheringSpeedMultiplier = 1.0;
		minerConfig.m_aEquipmentPrefabs = {"{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et"};
		minerConfig.m_sIcon = "{49C8B8FB3D34F654}Images/minero.edds";
		m_mJobConfigs.Set(EL_EJobType.MINER, minerConfig);
		
		EL_JobConfig lumberjackConfig = new EL_JobConfig();
		lumberjackConfig.m_eJobType = EL_EJobType.LUMBERJACK;
		lumberjackConfig.m_sJobName = "#LF-Job_Name_Lumberjack";
		lumberjackConfig.m_bCanGatherResources = true;
		lumberjackConfig.m_fGatheringSpeedMultiplier = 1.0;
		lumberjackConfig.m_aEquipmentPrefabs = {"{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et"};
		lumberjackConfig.m_sIcon = "{49C8B8FB3D34F654}Images/minero.edds"; // TODO: Update icon
		m_mJobConfigs.Set(EL_EJobType.LUMBERJACK, lumberjackConfig);
		
		EL_JobConfig medicConfig = new EL_JobConfig();
		medicConfig.m_eJobType = EL_EJobType.MEDIC;
		medicConfig.m_sJobName = "#LF-Job_Name_Medic";
		medicConfig.m_sDescription = "#LF-Job_Desc_Medic";
		medicConfig.m_bCanGatherResources = false;
		medicConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds"; // Same icon as Police for now
		m_mJobConfigs.Set(EL_EJobType.MEDIC, medicConfig);
		
		EL_JobConfig policeConfig = new EL_JobConfig();
		policeConfig.m_eJobType = EL_EJobType.POLICE;
		policeConfig.m_sJobName = "#LF-Job_Name_Police";
		policeConfig.m_sDescription = "#LF-Job_Desc_Police";
		policeConfig.m_bCanGatherResources = false;
		policeConfig.m_sIcon = "{6E1B308726CC6EF8}Images/recolector.edds"; // TODO: Update icon
		m_mJobConfigs.Set(EL_EJobType.POLICE, policeConfig);

		EL_JobConfig courierConfig = new EL_JobConfig();
		courierConfig.m_eJobType = EL_EJobType.PARCEL;
		courierConfig.m_sJobName = "#EL-Job_Name_Courier";
		courierConfig.m_sDescription = "#EL-Job_SignUp_Courier";
		courierConfig.m_bCanGatherResources = false;
		m_mJobConfigs.Set(EL_EJobType.PARCEL, courierConfig);

		EL_JobConfig fuelConfig = new EL_JobConfig();
		fuelConfig.m_eJobType = EL_EJobType.FUEL_DELIVERY;
		fuelConfig.m_sJobName = "#EL-Job_Name_Fuel";
		fuelConfig.m_sDescription = "#EL-Job_SignUp_Fuel";
		fuelConfig.m_bCanGatherResources = false;
		m_mJobConfigs.Set(EL_EJobType.FUEL_DELIVERY, fuelConfig);

		EL_JobConfig taxiConfig = new EL_JobConfig();
		taxiConfig.m_eJobType = EL_EJobType.TAXI;
		taxiConfig.m_sJobName = "#EL-Job_Name_Taxi";
		taxiConfig.m_sDescription = "#EL-Job_SignUp_Taxi";
		taxiConfig.m_bCanGatherResources = false;
		m_mJobConfigs.Set(EL_EJobType.TAXI, taxiConfig);
		
		m_CurrentJobConfig = m_mJobConfigs.Get(m_eCurrentJob);
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
					WidgetManager.Translate("#LF-Job_NeedPoliceBadge"),
					WidgetManager.Translate("#LF-Job_AccessDenied"),
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
					WidgetManager.Translate("#LF-Job_NeedMedicCard"),
					WidgetManager.Translate("#LF-Job_AccessDenied"),
					5.0
				);
				return false;
			}
		}

		// Courier, fuel delivery, and taxi are taken at the workplace. The
		// civilian career level is the gate, the same way a license gates
		// police and medic. An empty account id is a broken caller.
		if (LF_CivilianJob.IsDeliveryJob(jobType))
		{
			IEntity careerOwner = GetOwner();
			string careerUid = "";
			if (careerOwner)
				careerUid = EL_Utils.GetPlayerUID(careerOwner);

			if (careerUid.IsEmpty())
			{
				EL_Debug.Error("Jobs", "signup refused: player has no account id");
				return false;
			}

			if (!LF_CivilianJob.CanSignUp(careerUid, jobType))
			{
				EL_Debug.Log("Jobs", string.Format("signup rejected: %1 is below the level for %2", careerUid, typename.EnumToString(EL_EJobType, jobType)));
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
					WidgetManager.Translate("#LF-Job_NotWhitelisted"),
					WidgetManager.Translate("#LF-Job_AccessDenied")
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
		m_fLastJobSwitchAt = LF_JobGuards.Now();
		
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
		string jobName = WidgetManager.Translate(GetJobName());
		string message;
		string title;
		
		if (jobType == EL_EJobType.UNEMPLOYED)
		{
			message = WidgetManager.Translate("#LF-Job_OffDuty");
			title = WidgetManager.Translate("#LF-Job_OffDuty_Title");
		}
		else if (jobType == EL_EJobType.POLICE)
		{
			message = WidgetManager.Translate("#LF-Job_OnDutyPolice", jobName);
			title = WidgetManager.Translate("#LF-Job_OnDutyPolice_Title");
		}
		else if (jobType == EL_EJobType.MEDIC)
		{
			message = WidgetManager.Translate("#LF-Job_OnDutyMedic", jobName);
			title = WidgetManager.Translate("#LF-Job_OnDutyMedic_Title");
		}
		else
		{
			message = WidgetManager.Translate("#LF-Job_HiredHidden", jobName);
			title = WidgetManager.Translate("#LF-Job_Hired_Title");
		}
		
		SCR_HintManagerComponent.ShowCustomHint(message, title, 4.0);
		
		// Give job equipment
		GiveJobEquipment();

		if (LF_CivilianJob.IsDeliveryJob(jobType))
			PullCareer();
		
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void AskSetJob(EL_EJobType jobType)
	{
		if (Replication.IsServer())
		{
			RequestJobFromClient(jobType);
			return;
		}

		Rpc(RpcAsk_SetJob, jobType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetJob(EL_EJobType jobType)
	{
		RequestJobFromClient(jobType);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side gate for a job switch a client asked for. A client can name any job, so the
	//! switch needs the job's workplace within LF_JobGuards.WORKPLACE_RADIUS and the switch
	//! cooldown elapsed. Going off duty (UNEMPLOYED) needs no workplace. Workplace actions call
	//! SetJob directly; the action range is their proximity check.
	//! \return True when the job was set.
	bool RequestJobFromClient(EL_EJobType jobType)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return false;

		IEntity owner = GetOwner();
		if (!owner)
		{
			EL_Debug.Error("Jobs", "job request refused: no owner");
			return false;
		}

		float now = LF_JobGuards.Now();
		if (!LF_JobGuards.SwitchCooldownElapsed(m_fLastJobSwitchAt, now))
		{
			EL_Debug.Warn("Jobs", string.Format("job request refused: switch cooldown (%1 s) not elapsed for %2", LF_JobGuards.SWITCH_COOLDOWN_SECONDS, typename.EnumToString(EL_EJobType, jobType)));
			return false;
		}

		if (jobType != EL_EJobType.UNEMPLOYED && !LF_JobGuards.IsNearWorkplace(owner.GetOrigin(), jobType))
		{
			EL_Debug.Warn("Jobs", string.Format("job request refused: no %1 workplace within %2 m", typename.EnumToString(EL_EJobType, jobType), LF_JobGuards.WORKPLACE_RADIUS));
			return false;
		}

		// Security check: Only allow switching to POLICE if they have the license
		if (jobType == EL_EJobType.POLICE)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.POLICE_ACCESS))
			{
				EL_Debug.Warn("Jobs", "job request refused: POLICE needs the POLICE_ACCESS license");
				return false;
			}
		}

		// Security check: Only allow switching to MEDIC if they have the license
		if (jobType == EL_EJobType.MEDIC)
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(GetOwner());
			if (!licenseManager || !licenseManager.HasLicense(EL_ELicenseType.MEDIC_ACCESS))
			{
				EL_Debug.Warn("Jobs", "job request refused: MEDIC needs the MEDIC_ACCESS license");
				return false;
			}
		}

		return SetJob(jobType);
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
			return "#LF-Job_Name_Unknown";
		
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
	//! Add job experience. A completed delivery run calls this. The civilian
	//! career (courier, fuel, taxi) levels on LF_CivilianJob; this copies that
	//! result onto the player so the workplace gate and the HUD agree.
	void AddExperience(float amount)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return;

		IEntity owner = GetOwner();
		if (!owner)
		{
			EL_Debug.Error("Jobs", "AddExperience refused: no owner");
			return;
		}

		string uid = EL_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "AddExperience refused: player has no account id");
			return;
		}

		LF_CivilianJob.AddExperience(uid, amount);
		PullCareer();
	}

	//------------------------------------------------------------------------------------------------
	//! Copy the civilian career level and XP onto this component.
	void PullCareer()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		string uid = EL_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
			return;

		if (!m_mJobLevels)
			m_mJobLevels = new map<EL_EJobType, int>();
		if (!m_mJobExperience)
			m_mJobExperience = new map<EL_EJobType, float>();

		int levelBefore = m_iJobLevel;
		m_iJobLevel = LF_CivilianJob.GetLevel(uid);
		m_fJobExperience = LF_CivilianJob.GetExperience(uid);

		if (m_eCurrentJob != EL_EJobType.UNEMPLOYED)
		{
			m_mJobLevels.Set(m_eCurrentJob, m_iJobLevel);
			m_mJobExperience.Set(m_eCurrentJob, m_fJobExperience);
		}

		if (m_iJobLevel > levelBefore)
			OnLevelUp();

		if (m_OnJobExperienceGained)
			m_OnJobExperienceGained.Invoke(m_fJobExperience);

		Replication.BumpMe();
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
	//! Give equipment for current job
	protected void GiveJobEquipment()
	{
		if (!m_CurrentJobConfig || !m_CurrentJobConfig.m_aEquipmentPrefabs)
			return;
		
		if (m_CurrentJobConfig.m_aEquipmentPrefabs.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		InventoryStorageManagerComponent inventory = EL_Component<InventoryStorageManagerComponent>.Find(owner);
		if (!inventory)
			return;

		// One kit per job per account per session. Without this, switching jobs back and
		// forth (or dying and switching again) mints a fresh tool every time.
		string uid = EL_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "equipment refused: player has no account id");
			return;
		}

		if (!LF_JobGuards.ClaimEquipmentGrant(uid, m_eCurrentJob))
		{
			EL_Debug.Warn("Jobs", string.Format("equipment refused: %1 already received the %2 kit this session", uid, typename.EnumToString(EL_EJobType, m_eCurrentJob)));
			return;
		}

		// Spawn and give equipment items
		foreach (ResourceName prefab : m_CurrentJobConfig.m_aEquipmentPrefabs)
		{
			if (prefab.IsEmpty())
				continue;
			
			// The helper logs EL_Debug.Error and returns null when the prefab does not resolve.
			IEntity equipment = EL_Utils.SpawnEntityPrefab(prefab, owner.GetOrigin());
			if (!equipment)
			{
				EL_Debug.Error("Jobs", string.Format("job equipment skipped: %1 did not spawn", prefab));
				continue;
			}

			if (!inventory.TryInsertItem(equipment))
				EL_Debug.Warn("Jobs", string.Format("job equipment %1 did not fit the inventory; left at the player's feet", prefab));
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
	protected void OnLevelUp()
	{
		if (m_OnLevelUp)
			m_OnLevelUp.Invoke(m_iJobLevel);

		// Hidden progression (docs/decisions.md "Progression is pluggable"): no level or percent
		// text. The dropoff tells the player when new work opens (LF_CivilianJob.NewlyOpened).
		EL_Debug.Log("Jobs", string.Format("job level %1 pulled onto the player component", m_iJobLevel));
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
	//! Ask the server to open a Fruit Catcher round. The minigame calls this when the round
	//! starts; the server records the round id and start time and sends the id back.
	void AskStartFruitCatcherRound(EHarvestJobType jobType)
	{
		if (Replication.IsServer())
		{
			StartFruitCatcherRound(jobType);
			return;
		}

		Rpc(RpcAsk_StartFruitCatcherRound, jobType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartFruitCatcherRound(EHarvestJobType jobType)
	{
		StartFruitCatcherRound(jobType);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens a round for the caller. Requires the harvest job (FARMER).
	//! \return The new round id, or 0 when refused.
	int StartFruitCatcherRound(EHarvestJobType jobType)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		if (m_eCurrentJob != EL_EJobType.FARMER)
		{
			EL_Debug.Warn("Jobs", string.Format("fruit round refused: caller holds %1, not FARMER", typename.EnumToString(EL_EJobType, m_eCurrentJob)));
			return 0;
		}

		if (LF_FruitCatcherRound.FruitPrefab(jobType).IsEmpty())
		{
			EL_Debug.Warn("Jobs", string.Format("fruit round refused: unknown fruit type %1", jobType));
			return 0;
		}

		int roundId = m_FruitRound.Start(jobType, LF_JobGuards.Now());
		m_iClientFruitRoundId = roundId;
		EL_Debug.Log("Jobs", string.Format("fruit round %1 started (%2)", roundId, typename.EnumToString(EHarvestJobType, jobType)));
		Rpc(RpcDo_FruitCatcherRoundStarted, roundId);
		return roundId;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_FruitCatcherRoundStarted(int roundId)
	{
		m_iClientFruitRoundId = roundId;
	}

	//------------------------------------------------------------------------------------------------
	//! Round id the server issued for this player's open round (0 = none).
	int GetFruitCatcherRoundId()
	{
		return m_iClientFruitRoundId;
	}

	//------------------------------------------------------------------------------------------------
	//! Ask server to claim fruit catcher rewards for the round the server opened.
	void AskClaimFruitCatcherReward(int score, EHarvestJobType jobType)
	{
		int roundId = m_iClientFruitRoundId;
		m_iClientFruitRoundId = 0;

		if (Replication.IsServer())
		{
			ClaimFruitCatcherReward(roundId, score, jobType);
			return;
		}

		Rpc(RpcAsk_ClaimFruitCatcherReward, roundId, score, jobType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ClaimFruitCatcherReward(int roundId, int score, EHarvestJobType jobType)
	{
		ClaimFruitCatcherReward(roundId, score, jobType);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pays one Fruit Catcher round. Refused unless the caller holds FARMER, the round
	//! id matches the open round for that fruit, and the round ran at least
	//! LF_FruitCatcherRound.MIN_ROUND_SECONDS. A paid round is closed; a second claim is refused.
	//! \return The number of fruit awarded (0 when refused).
	int ClaimFruitCatcherReward(int roundId, int score, EHarvestJobType jobType)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		if (m_eCurrentJob != EL_EJobType.FARMER)
		{
			EL_Debug.Warn("Jobs", string.Format("fruit claim refused: caller holds %1, not FARMER", typename.EnumToString(EL_EJobType, m_eCurrentJob)));
			return 0;
		}

		string reason = m_FruitRound.Claim(roundId, jobType, LF_JobGuards.Now());
		if (!reason.IsEmpty())
		{
			EL_Debug.Warn("Jobs", string.Format("fruit claim refused: round %1 %2", roundId, reason));
			return 0;
		}

		// Server-side clamp: a forged score can never exceed one round's max
		int rewardCount = EL_GetFruitCatcherRewardCount(score);
		if (rewardCount <= 0) return 0;


		ResourceName fruitPrefab = LF_FruitCatcherRound.FruitPrefab(jobType);
		string fruitName;
		
		switch (jobType)
		{
			case EHarvestJobType.APPLES:
				fruitName = "#LF-Fruit_Apples";
				break;
			case EHarvestJobType.TOMATOES:
				fruitName = "#LF-Fruit_Tomatoes";
				break;
			case EHarvestJobType.PLUMS:
				fruitName = "#LF-Fruit_Plums";
				break;
			case EHarvestJobType.BERRIES:
				fruitName = "#LF-Fruit_Berries";
				break;
		}
		
		// Unknown fruit type gets nothing, not even XP
		if (!fruitPrefab) return 0;

		IEntity owner = GetOwner();
		if (!owner)
			return 0;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(owner.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
		{
			EL_Debug.Error("Jobs", "fruit claim refused: no inventory found on player");
			return 0;
		}

		// Give XP (e.g. 5 XP per fruit)
		float xpAmount = rewardCount * 5.0;
		
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
		if (levelComp)
		{
			levelComp.AddExperience(xpAmount, "fruit picking");
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
				EL_Debug.Error("Jobs", string.Format("fruit claim: failed to spawn fruit %1", i));
				continue;
			}
			
			// Try to insert using TryInsertItem (will go to any available storage)
			if (inventory.TryInsertItem(fruitEntity))
				actuallyAdded++;
			else
				EL_Debug.Warn("Jobs", string.Format("fruit claim: inventory full, fruit %1 dropped near player", i));
		}
		
		EL_Debug.Log("Jobs", string.Format("fruit round %1 paid %2/%3 %4", roundId, actuallyAdded, rewardCount, fruitName));
		
		// Notify
		string message = WidgetManager.Translate("#LF-Job_FruitHarvest", actuallyAdded, WidgetManager.Translate(fruitName));
		SCR_HintManagerComponent.ShowCustomHint(message, WidgetManager.Translate("#LF-Job_Work_Title"), 5.0);
		return rewardCount;
	}
}
