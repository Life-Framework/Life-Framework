//------------------------------------------------------------------------------------------------
//! How needs act on the character's real health (the damage manager's default "Health" hit zone),
//! server side. Low needs pull health down toward LF_SurvivalConfig.m_fHealthCapScaled and never
//! below it, so needs never kill and never worsen an injury. Once needs are met, survival gives
//! back only what it took (the deficit), and never pulls down health that vanilla healing raised.
//! All values are fractions of max health (0..1).
class LF_SurvivalHealth
{
	//------------------------------------------------------------------------------------------------
	//! \return Health after deltaTime of low needs: drained toward the cap, never below it, and
	//!         unchanged when already at or below the cap or when deltaTime <= 0.
	static float ComputeCappedHealth(float currentScaled, float capScaled, float drainPerSecond, float deltaTime)
	{
		if (deltaTime <= 0 || currentScaled <= capScaled)
			return currentScaled;

		return Math.Max(currentScaled - drainPerSecond * deltaTime, capScaled);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Health to give back after deltaTime of met needs: the restore rate, at most the
	//!         deficit, never past full health.
	static float ComputeRestore(float currentScaled, float deficitScaled, float regenPerSecond, float deltaTime)
	{
		if (deltaTime <= 0 || deficitScaled <= 0 || regenPerSecond <= 0)
			return 0;

		float amount = Math.Min(regenPerSecond * deltaTime, deficitScaled);
		return Math.Max(Math.Min(amount, 1 - currentScaled), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: heals the default hit zone by amountScaled of max health, capped at full.
	static void Restore(notnull SCR_CharacterDamageManagerComponent damageManager, float amountScaled)
	{
		HitZone health = damageManager.GetDefaultHitZone();
		if (!health || amountScaled <= 0 || health.GetDamageState() == EDamageState.DESTROYED)
			return;

		health.SetHealthScaled(Math.Min(health.GetHealthScaled() + amountScaled, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: applies deltaTime of needs to real health.
	//! \return The new deficit (what needs have taken and not given back).
	static float Apply(notnull SCR_CharacterDamageManagerComponent damageManager, bool needsLow, float deltaTime, notnull LF_SurvivalConfig config, float deficitScaled)
	{
		HitZone health = damageManager.GetDefaultHitZone();
		if (!health || deltaTime <= 0)
			return deficitScaled;

		// A dead body is the death flow's business; a fresh body starts clean.
		if (health.GetDamageState() == EDamageState.DESTROYED)
			return 0;

		float current = health.GetHealthScaled();
		if (needsLow)
		{
			float capped = ComputeCappedHealth(current, config.m_fHealthCapScaled, config.m_fHealthDrainScaledPerSecond, deltaTime);
			if (capped < current)
			{
				health.SetHealthScaled(capped);
				deficitScaled += current - capped;
			}

			return deficitScaled;
		}

		// Vanilla healing may already have filled the gap; forget the part it covered.
		deficitScaled = Math.Min(deficitScaled, 1 - current);
		float restore = ComputeRestore(current, deficitScaled, config.m_fHealthRestoreScaledPerSecond, deltaTime);
		if (restore > 0)
			health.SetHealthScaled(current + restore);

		return Math.Max(deficitScaled - restore, 0);
	}
}
