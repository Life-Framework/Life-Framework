//------------------------------------------------------------------------------------------------
//! Componente que gestiona el nivel global del jugador y skill points
//! Este sistema es independiente de los trabajos
[ComponentEditorProps(category: "EveronLife/Feature/Level", description: "Manages player's global level, experience and skill points")]
class EL_PlayerLevelComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_PlayerLevelComponent : ScriptComponent
{
	[RplProp(onRplName: "OnLevelChanged")]
	protected int m_iPlayerLevel = 1;
	
	[RplProp()]
	protected float m_fPlayerExperience = 0;
	
	[RplProp()]
	protected int m_iSkillPoints = 0; // SP disponibles para gastar
	
	[RplProp()]
	protected int m_iTotalSkillPointsEarned = 0; // Total de SP ganados en toda la partida
	
	protected ref ScriptInvoker m_OnLevelUp;
	protected ref ScriptInvoker m_OnExperienceGained;
	protected ref ScriptInvoker m_OnSkillPointsChanged;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!Replication.IsServer())
			return;
		
		Print(string.Format("[EL_PlayerLevelComponent] Initialized for player. Level: %1, XP: %2, SP: %3", 
			m_iPlayerLevel, m_fPlayerExperience, m_iSkillPoints), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Añadir experiencia al jugador
	void AddExperience(float amount, string source = "")
	{
		if (!Replication.IsServer())
			return;
		
		m_fPlayerExperience += amount;

		EL_Debug.Log("Level", string.Format("+%1 xp from '%2' (total %3)", amount, source, m_fPlayerExperience));

		// Invocar evento
		if (m_OnExperienceGained)
			m_OnExperienceGained.Invoke(amount, source);

		// Comprobar si sube de nivel
		CheckLevelUp();

		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Comprobar si el jugador debe subir de nivel
	protected void CheckLevelUp()
	{
		float xpRequired = GetExperienceForNextLevel();
		
		while (m_fPlayerExperience >= xpRequired)
		{
			m_fPlayerExperience -= xpRequired;
			m_iPlayerLevel++;
			m_iSkillPoints++;
			m_iTotalSkillPointsEarned++;

			EL_Debug.Log("Level", string.Format("LEVEL UP -> level %1 (+1 sp, total %2)", m_iPlayerLevel, m_iSkillPoints));
			
			// Notificar al jugador
			OnLevelUp();
			
			// Comprobar si puede subir otro nivel
			xpRequired = GetExperienceForNextLevel();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calcular XP necesaria para siguiente nivel
	float GetExperienceForNextLevel()
	{
		// Fórmula: 100 XP por nivel
		// Nivel 1->2: 100 XP
		// Nivel 2->3: 200 XP
		// Nivel 3->4: 300 XP
		return m_iPlayerLevel * 100.0;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gastar skill points
	bool SpendSkillPoints(int amount, string reason = "")
	{
		if (!Replication.IsServer())
			return false;
		
		if (m_iSkillPoints < amount)
		{
			Print(string.Format("[EL_PlayerLevelComponent] Cannot spend %1 SP, only has %2", 
				amount, m_iSkillPoints), LogLevel.WARNING);
			return false;
		}
		
		m_iSkillPoints -= amount;
		
		Print(string.Format("[EL_PlayerLevelComponent] Spent %1 SP for '%2'. Remaining: %3", 
			amount, reason, m_iSkillPoints), LogLevel.NORMAL);
		
		if (m_OnSkillPointsChanged)
			m_OnSkillPointsChanged.Invoke(m_iSkillPoints);
		
		Replication.BumpMe();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Añadir skill points (por admin o eventos especiales)
	void AddSkillPoints(int amount, string reason = "")
	{
		if (!Replication.IsServer())
			return;
		
		m_iSkillPoints += amount;
		
		Print(string.Format("[EL_PlayerLevelComponent] Added %1 SP for '%2'. Total: %3", 
			amount, reason, m_iSkillPoints), LogLevel.NORMAL);
		
		if (m_OnSkillPointsChanged)
			m_OnSkillPointsChanged.Invoke(m_iSkillPoints);
		
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------
	
	int GetLevel()
	{
		return m_iPlayerLevel;
	}
	
	int GetPlayerLevel() // Alias
	{
		return m_iPlayerLevel;
	}
	
	float GetExperience()
	{
		return m_fPlayerExperience;
	}
	
	float GetPlayerExperience() // Alias
	{
		return m_fPlayerExperience;
	}
	
	int GetSkillPoints()
	{
		return m_iSkillPoints;
	}
	
	int GetTotalSkillPointsEarned()
	{
		return m_iTotalSkillPointsEarned;
	}
	
	//------------------------------------------------------------------------------------------------
	// Passive Bonuses (reemplaza sistema de trabajos)
	//------------------------------------------------------------------------------------------------
	
	//! Get gathering speed multiplier based on player level
	//! Returns: 1.0 at level 1, 1.1 at level 2, 1.2 at level 3, etc.
	float GetGatheringSpeedMultiplier()
	{
		// Base multiplier 1.0 + 10% per level above 1
		return 1.0 + (m_iPlayerLevel - 1) * 0.1;
	}
	
	//! Get gathering amount bonus multiplier based on player level
	//! Returns: 1.0 at level 1, 1.1 at level 2, etc. (+10% per level)
	float GetGatheringAmountBonus()
	{
		// +10% bonus per level
		return 1.0 + (m_iPlayerLevel - 1) * 0.1;
	}
	
	//! Get sale price bonus multiplier based on player level
	//! Returns: 1.0 at level 1, 1.05 at level 2, etc. (+5% per level)
	float GetSaleBonus()
	{
		// +5% bonus per level
		return 1.0 + (m_iPlayerLevel - 1) * 0.05;
	}
	
	//! Get formatted string of current bonuses for UI display
	string GetBonusesText()
	{
		int gatherPercent = Math.Round((GetGatheringAmountBonus() - 1.0) * 100);
		int salePercent = Math.Round((GetSaleBonus() - 1.0) * 100);
		
		if (gatherPercent == 0 && salePercent == 0)
			return "Sin bonos activos";
		
		return string.Format("+%1%% recolección | +%2%% venta", gatherPercent, salePercent);
	}
	
	//------------------------------------------------------------------------------------------------
	// Setters (para persistencia)
	//------------------------------------------------------------------------------------------------
	
	void SetLevel(int level)
	{
		Print(string.Format("[EL_PlayerLevelComponent] SetLevel called - Old: %1, New: %2", 
			m_iPlayerLevel, level), LogLevel.NORMAL);
		m_iPlayerLevel = level;
		Replication.BumpMe();
	}
	
	void SetExperience(float xp)
	{
		m_fPlayerExperience = xp;
		Replication.BumpMe();
	}
	
	void SetSkillPoints(int sp)
	{
		m_iSkillPoints = sp;
		Replication.BumpMe();
	}
	
	void SetTotalSkillPointsEarned(int total)
	{
		m_iTotalSkillPointsEarned = total;
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	// Script Invokers
	//------------------------------------------------------------------------------------------------
	
	ScriptInvoker GetOnLevelUp()
	{
		if (!m_OnLevelUp)
			m_OnLevelUp = new ScriptInvoker();
		return m_OnLevelUp;
	}
	
	ScriptInvoker GetOnExperienceGained()
	{
		if (!m_OnExperienceGained)
			m_OnExperienceGained = new ScriptInvoker();
		return m_OnExperienceGained;
	}
	
	ScriptInvoker GetOnSkillPointsChanged()
	{
		if (!m_OnSkillPointsChanged)
			m_OnSkillPointsChanged = new ScriptInvoker();
		return m_OnSkillPointsChanged;
	}
	
	//------------------------------------------------------------------------------------------------
	// Eventos de replicación
	//------------------------------------------------------------------------------------------------
	
	protected void OnLevelChanged()
	{
		Print(string.Format("[EL_PlayerLevelComponent] OnLevelChanged RPC callback - Current level: %1", 
			m_iPlayerLevel), LogLevel.NORMAL);
		
		if (m_OnLevelUp)
			m_OnLevelUp.Invoke(m_iPlayerLevel);
		
		// Mostrar notificación
		IEntity owner = GetOwner();
		if (!owner)
			return;
		
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController || playerController.GetControlledEntity() != owner)
			return;
		
		// Calcular bonos actuales
		int gatherPercent = Math.Round((GetGatheringAmountBonus() - 1.0) * 100);
		int salePercent = Math.Round((GetSaleBonus() - 1.0) * 100);
		
		string bonusText = "";
		if (gatherPercent > 0 || salePercent > 0)
		{
			bonusText = string.Format("\n\nBonos Activos:\n+%1%% Recolección | +%2%% Venta", gatherPercent, salePercent);
		}
		
		string message = string.Format("¡NIVEL %1 ALCANZADO!\n+1 Skill Point ganado%2", m_iPlayerLevel, bonusText);
		SCR_HintManagerComponent.ShowCustomHint(message, "LEVEL UP!", 8.0);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnLevelUp()
	{
		OnLevelChanged();
		Replication.BumpMe();
	}
}
