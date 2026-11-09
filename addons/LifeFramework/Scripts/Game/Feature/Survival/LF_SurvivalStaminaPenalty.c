//------------------------------------------------------------------------------------------------
//! Low hunger or thirst makes exertion drain stamina faster (LF_SurvivalConfig.m_fStaminaDrainMultiplier),
//! so a hungry player sprints for less time and pants sooner: the cue is in the body, not a HUD.
//! Mirrors the base game's hydration penalty in SCR_CharacterStaminaComponent.OnStaminaDrain and
//! runs wherever the stamina simulation runs (server and owner); the multiplier comes from the
//! survival component's owner-only replicated low-needs flag.
modded class SCR_CharacterStaminaComponent
{
	protected bool m_bLF_ApplyingSurvivalPenalty;

	//------------------------------------------------------------------------------------------------
	override void OnStaminaDrain(float pDrain)
	{
		super.OnStaminaDrain(pDrain);

		if (pDrain >= 0 || m_bLF_ApplyingSurvivalPenalty)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(owner.FindComponent(LF_CharacterSurvivalComponent));
		if (!survival)
			return;

		float multiplier = survival.GetStaminaDrainMultiplier();
		if (multiplier <= 1)
			return;

		// AddStamina re-enters OnStaminaDrain; guard ours and the base hydration penalty.
		m_bLF_ApplyingSurvivalPenalty = true;
		bool wasApplyingHydration = m_bApplyingHydrationPenalty;
		m_bApplyingHydrationPenalty = true;
		AddStamina(pDrain * (multiplier - 1));
		m_bApplyingHydrationPenalty = wasApplyingHydration;
		m_bLF_ApplyingSurvivalPenalty = false;
	}
}
