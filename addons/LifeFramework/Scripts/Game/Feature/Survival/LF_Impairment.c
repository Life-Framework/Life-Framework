//! How an impairment level falls after a dose (#1919).
enum LF_EImpairmentCurve
{
	LINEAR,		//!< straight down to zero over the dose's duration
	EXPONENTIAL	//!< halves every fifth of the duration: a sharp drop, then a long tail
}

//------------------------------------------------------------------------------------------------
//! One character's impairment: a level from 0 to 100 and how it decays. Pure logic, no world.
class LF_ImpairmentState : Managed
{
	//! Below this the level counts as gone.
	static const float CLEAR_BELOW = 0.5;

	protected float m_fLevel;
	protected LF_EImpairmentCurve m_eCurve;
	protected float m_fLinearRate;		//!< points per second, LINEAR
	protected float m_fHalfLifeSeconds;	//!< EXPONENTIAL

	//------------------------------------------------------------------------------------------------
	//! Adds a dose. The level rises by strength but never past cap (a level already above the cap
	//! from a stronger drink stays). The new dose sets the curve, and the whole level runs out over
	//! its duration.
	void Apply(float strength, float durationSeconds, LF_EImpairmentCurve curve, float cap)
	{
		if (strength <= 0)
			return;

		float capped = Math.Clamp(cap, 0, 100);
		if (m_fLevel < capped)
			m_fLevel = Math.Min(m_fLevel + strength, capped);

		float duration = Math.Max(durationSeconds, 1);
		m_eCurve = curve;
		m_fLinearRate = m_fLevel / duration;
		m_fHalfLifeSeconds = duration / 5;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the level on by deltaSeconds of decay.
	void Advance(float deltaSeconds)
	{
		if (deltaSeconds <= 0 || m_fLevel <= 0)
			return;

		if (m_eCurve == LF_EImpairmentCurve.EXPONENTIAL)
			m_fLevel = m_fLevel * Math.Pow(0.5, deltaSeconds / Math.Max(m_fHalfLifeSeconds, 0.1));
		else
			m_fLevel = m_fLevel - m_fLinearRate * deltaSeconds;

		if (m_fLevel < CLEAR_BELOW)
			m_fLevel = 0;
	}

	//------------------------------------------------------------------------------------------------
	float GetLevel()
	{
		return m_fLevel;
	}
}

//------------------------------------------------------------------------------------------------
//! Server registry of impairment per character id. In memory only: a restart clears every level
//! (persisting is not built; the default is off, docs/features/character-survival.md). Death
//! clears a character's level. The owning client reads a whole-point level from
//! LF_CharacterSurvivalComponent.GetReplicatedImpairment().
class LF_Impairment
{
	static const string LOG = "Impairment";

	protected static ref map<string, ref LF_ImpairmentState> s_mStates = new map<string, ref LF_ImpairmentState>();

	//------------------------------------------------------------------------------------------------
	//! Server: adds a dose to a character. Returns the new level.
	static float Apply(string characterId, float strength, float durationSeconds, LF_EImpairmentCurve curve, float cap)
	{
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(LOG, "dose with no character id ignored");
			return 0;
		}

		LF_ImpairmentState state = s_mStates.Get(characterId);
		if (!state)
		{
			state = new LF_ImpairmentState();
			s_mStates.Set(characterId, state);
		}

		float before = state.GetLevel();
		state.Apply(strength, durationSeconds, curve, cap);
		LF_Debug.Info(LOG, string.Format("%1 dose %2 over %3 s (%4, cap %5): level %6 -> %7",
			characterId, strength, durationSeconds, typename.EnumToString(LF_EImpairmentCurve, curve), cap, before, state.GetLevel()));
		return state.GetLevel();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: decays one character by deltaSeconds; drops the entry once it reaches zero.
	static void Advance(string characterId, float deltaSeconds)
	{
		LF_ImpairmentState state = s_mStates.Get(characterId);
		if (!state)
			return;

		state.Advance(deltaSeconds);
		if (state.GetLevel() <= 0)
		{
			s_mStates.Remove(characterId);
			LF_Debug.Info(LOG, string.Format("%1 sober again", characterId));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 0 when the character carries no impairment.
	static float GetLevel(string characterId)
	{
		LF_ImpairmentState state = s_mStates.Get(characterId);
		if (!state)
			return 0;

		return state.GetLevel();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes a character's impairment (death, body deleted).
	static void Clear(string characterId, string reason)
	{
		if (!s_mStates.Contains(characterId))
			return;

		s_mStates.Remove(characterId);
		LF_Debug.Info(LOG, string.Format("%1 cleared (%2)", characterId, reason));
	}

	//------------------------------------------------------------------------------------------------
	//! True when the owner should hear about a new whole-point level: a change of 5 points, or the
	//! level starting or ending. Keeps a 1 s decay from bumping replication every tick.
	static bool NeedsSync(int sentLevel, float level)
	{
		int whole = Math.Round(level);
		if (whole == sentLevel)
			return false;

		if (whole == 0 || sentLevel == 0)
			return true;

		return Math.AbsInt(whole - sentLevel) >= 5;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and so a restart): every level goes.
	static void ResetStatics()
	{
		s_mStates.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! The character side: the survival tick decays the level, death clears it, and the owner gets a
//! whole-point level as an owner-only RplProp bumped on a significant change (NeedsSync).
modded class LF_CharacterSurvivalComponent
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iReplicatedImpairment;

	protected float m_fImpairmentLastTickMs;

	//! Set once this body's death has cleared its level. The character id outlives the body, so a
	//! corpse must not touch the registry again: a respawned body may already be drinking.
	protected bool m_bImpairmentBodyDead;

	//------------------------------------------------------------------------------------------------
	override void Tick()
	{
		super.Tick();
		TickImpairment();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: decays this character's level by the time since the last tick; a dead body sobers
	//! at once.
	protected void TickImpairment()
	{
		if (!Replication.IsServer() || m_sCharacterId.IsEmpty() || m_bImpairmentBodyDead)
			return;

		if (IsOwnerDead())
		{
			m_bImpairmentBodyDead = true;
			LF_Impairment.Clear(m_sCharacterId, "death");
			if (m_iReplicatedImpairment != 0)
			{
				m_iReplicatedImpairment = 0;
				Replication.BumpMe();
			}
			return;
		}

		float nowMs = GetGame().GetWorld().GetWorldTime();
		float deltaSeconds = ClampTickDelta((nowMs - m_fImpairmentLastTickMs) / 1000, LF_SurvivalConfig.Get().m_fMaxTickDeltaSeconds);
		m_fImpairmentLastTickMs = nowMs;

		if (LF_Impairment.GetLevel(m_sCharacterId) <= 0 && m_iReplicatedImpairment == 0)
			return;

		LF_Impairment.Advance(m_sCharacterId, deltaSeconds);
		SyncImpairment();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOwnerDead()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return true;

		DamageManagerComponent damage = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return true;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(owner.FindComponent(CharacterControllerComponent));
		return controller && controller.GetLifeState() == ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: adds a dose to this character and tells the owner. Used by LF_ConsumableEffectImpair.
	void ApplyImpairment(float strength, float durationSeconds, LF_EImpairmentCurve curve, float cap)
	{
		if (!Replication.IsServer() || m_bImpairmentBodyDead)
			return;

		if (m_sCharacterId.IsEmpty())
		{
			LF_Debug.Warn(LF_Impairment.LOG, "dose on a character with no survival id ignored");
			return;
		}

		if (LF_Impairment.GetLevel(m_sCharacterId) <= 0)
			m_fImpairmentLastTickMs = GetGame().GetWorld().GetWorldTime();

		LF_Impairment.Apply(m_sCharacterId, strength, durationSeconds, curve, cap);
		SyncImpairment();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pushes the whole-point level to the owner when it changed enough to matter.
	protected void SyncImpairment()
	{
		float level = LF_Impairment.GetLevel(m_sCharacterId);
		if (!LF_Impairment.NeedsSync(m_iReplicatedImpairment, level))
			return;

		m_iReplicatedImpairment = Math.Round(level);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Owner and server: whole-point impairment (0-100), within 5 points of the server level.
	int GetReplicatedImpairment()
	{
		return m_iReplicatedImpairment;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer() && !m_sCharacterId.IsEmpty() && !m_bImpairmentBodyDead)
			LF_Impairment.Clear(m_sCharacterId, "body deleted");

		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_Impairment");
		if (apply)
			LF_Impairment.ResetStatics();
	}
}
