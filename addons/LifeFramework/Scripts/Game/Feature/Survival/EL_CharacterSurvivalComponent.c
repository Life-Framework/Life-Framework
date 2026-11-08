[ComponentEditorProps(category: "EveronLife/Survival", description: "Component for character survival stats")]
class EL_CharacterSurvivalComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Hunger and thirst for one character, and their effect on the character's real health.
//! Server-authoritative: the stats live and tick on the server only (a call-queue tick at
//! LF_SurvivalConfig.m_fTickIntervalSeconds). Low needs pull the damage manager's health down to
//! the config cap (LF_SurvivalHealth); met needs give back only what needs took.
//! The owner's client gets whole-point hunger and thirst plus the low-needs flag as owner-only
//! RplProps, bumped only when one of them changes, for the stamina penalty and the HUD gauges.
class EL_CharacterSurvivalComponent : ScriptComponent
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iReplicatedHunger = 100;

	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iReplicatedThirst = 100;

	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bReplicatedNeedsLow;

	protected ref EL_SurvivalStats m_SurvivalStats;
	protected string m_sCharacterId;
	protected float m_fLastTickTimeMs;

	//! Fraction of max health that low needs have taken from this body and not yet given back.
	//! Session state on purpose: a fresh body after a restart or respawn starts with full health.
	protected float m_fHealthDeficitScaled;

	//------------------------------------------------------------------------------------------------
	//! Server: loads (or creates and registers) the survival stats for a character and starts the
	//! survival tick.
	//! \param characterId Character persistence id (PersistenceIdUtils generated UUID).
	void Init(string characterId)
	{
		m_sCharacterId = characterId;

		EL_PersistenceComponent persistence = GetPersistenceComponent();
		if (persistence)
		{
			m_SurvivalStats = persistence.GetSurvivalStats(characterId);
			if (!m_SurvivalStats)
			{
				m_SurvivalStats = EL_SurvivalStats.Create(characterId);
				persistence.SetSurvivalStats(characterId, m_SurvivalStats);
			}
		}
		else
		{
			m_SurvivalStats = EL_SurvivalStats.Create(characterId);
		}

		m_fLastTickTimeMs = GetGame().GetWorld().GetWorldTime();
		SyncReplication();

		if (!Replication.IsServer())
		{
			EL_Debug.Warn("Survival", string.Format("survival init on a client for %1: no tick, the server owns the stats", characterId));
			return;
		}

		LF_SurvivalConfig config = LF_SurvivalConfig.Get();
		int intervalMs = Math.Max(config.m_fTickIntervalSeconds, 0.1) * 1000;
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().CallLater(Tick, intervalMs, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server tick: applies the elapsed time (clamped against hitches) and syncs the owner.
	protected void Tick()
	{
		if (!m_SurvivalStats)
			return;

		LF_SurvivalConfig config = LF_SurvivalConfig.Get();
		float nowMs = GetGame().GetWorld().GetWorldTime();
		float deltaSeconds = ClampTickDelta((nowMs - m_fLastTickTimeMs) / 1000, config.m_fMaxTickDeltaSeconds);
		m_fLastTickTimeMs = nowMs;

		bool wasLow = m_SurvivalStats.AreNeedsLow(config);
		m_SurvivalStats.UpdateStats(deltaSeconds, config);
		bool isLow = m_SurvivalStats.AreNeedsLow(config);
		ApplyNeedsToHealth(deltaSeconds);
		if (isLow != wasLow)
		{
			EL_Debug.Info("Survival", string.Format("%1 needs low=%2 (hunger %3, thirst %4, health deficit %5)",
				m_sCharacterId, isLow, m_SurvivalStats.GetHunger(), m_SurvivalStats.GetThirst(), m_fHealthDeficitScaled));
		}

		SyncReplication();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: applies deltaSeconds of needs to the character's real health. Public so a WORLD test
	//! can drive it without waiting on the tick.
	void ApplyNeedsToHealth(float deltaSeconds)
	{
		if (!m_SurvivalStats || !Replication.IsServer())
			return;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(GetOwner().FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damageManager)
		{
			EL_Debug.Error("Survival", string.Format("%1 has no character damage manager: needs do not touch health", m_sCharacterId));
			return;
		}

		LF_SurvivalConfig config = LF_SurvivalConfig.Get();
		m_fHealthDeficitScaled = LF_SurvivalHealth.Apply(damageManager, m_SurvivalStats.AreNeedsLow(config), deltaSeconds, config, m_fHealthDeficitScaled);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fraction of max health needs have taken and not given back.
	float GetHealthDeficitScaled()
	{
		return m_fHealthDeficitScaled;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The delta a tick may apply: never negative, never above maxSeconds.
	static float ClampTickDelta(float rawSeconds, float maxSeconds)
	{
		return Math.Clamp(rawSeconds, 0, Math.Max(maxSeconds, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Throttle for the owner sync: only a whole-point hunger or thirst change, or a low-needs flip,
	//! is worth a BumpMe.
	static bool NeedsSync(int sentHunger, int sentThirst, bool sentLow, float hunger, float thirst, bool low)
	{
		if (sentLow != low)
			return true;

		if (sentHunger != Math.Round(hunger))
			return true;

		return sentThirst != Math.Round(thirst);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pushes the owner-facing values when they changed enough to matter.
	void SyncReplication()
	{
		if (!m_SurvivalStats || !Replication.IsServer())
			return;

		float hunger = m_SurvivalStats.GetHunger();
		float thirst = m_SurvivalStats.GetThirst();
		bool low = m_SurvivalStats.AreNeedsLow();
		if (!NeedsSync(m_iReplicatedHunger, m_iReplicatedThirst, m_bReplicatedNeedsLow, hunger, thirst, low))
			return;

		m_iReplicatedHunger = Math.Round(hunger);
		m_iReplicatedThirst = Math.Round(thirst);
		m_bReplicatedNeedsLow = low;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the game-mode persistence component (EL_PersistenceComponent) attached to the
	//! game-mode entity.
	//! \return The persistence component, or null when no game mode carries one.
	protected EL_PersistenceComponent GetPersistenceComponent()
	{
		EL_GameModeRoleplay mode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
			return null;

		return EL_PersistenceComponent.Cast(mode.FindComponent(EL_PersistenceComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side stats; null on clients.
	EL_SurvivalStats GetSurvivalStats()
	{
		return m_SurvivalStats;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner and server: whole-point hunger (0-100).
	int GetReplicatedHunger()
	{
		return m_iReplicatedHunger;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner and server: whole-point thirst (0-100).
	int GetReplicatedThirst()
	{
		return m_iReplicatedThirst;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner and server: the stamina drain multiplier low needs impose (1 when needs are met).
	float GetStaminaDrainMultiplier()
	{
		if (!m_bReplicatedNeedsLow)
			return 1;

		return Math.Max(LF_SurvivalConfig.Get().m_fStaminaDrainMultiplier, 1);
	}

	//------------------------------------------------------------------------------------------------
	void SetSurvivalStats(EL_SurvivalStats stats)
	{
		m_SurvivalStats = stats;
		m_fLastTickTimeMs = GetGame().GetWorld().GetWorldTime();

		// Keep the persistence store in sync so a save picks up the live value.
		if (m_SurvivalStats)
		{
			EL_PersistenceComponent persistence = GetPersistenceComponent();
			if (persistence)
				persistence.SetSurvivalStats(m_sCharacterId, m_SurvivalStats);
		}

		SyncReplication();
	}

	//------------------------------------------------------------------------------------------------
	void Eat(float amount)
	{
		if (!m_SurvivalStats)
			return;

		m_SurvivalStats.Eat(amount);
		SyncReplication();
	}

	//------------------------------------------------------------------------------------------------
	void Drink(float amount)
	{
		if (!m_SurvivalStats)
			return;

		m_SurvivalStats.Drink(amount);
		SyncReplication();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: heals the character's real health by amount percent of max (consumable restore).
	void Heal(float amount)
	{
		if (amount <= 0 || !Replication.IsServer())
			return;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(GetOwner().FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damageManager)
		{
			EL_Debug.Error("Survival", string.Format("%1 heal skipped: no character damage manager", m_sCharacterId));
			return;
		}

		LF_SurvivalHealth.Restore(damageManager, amount / 100);
	}
};
