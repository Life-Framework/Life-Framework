//------------------------------------------------------------------------------------------------
//! One victim/attacker pair's sampler state.
class LF_IncidentDamageWindow : Managed
{
	float m_fLastPublish = -1000000;
	float m_fPending;
}

//------------------------------------------------------------------------------------------------
//! Damage sampler for the incident log. Every hit between two players reaches Offer; at most one
//! LF_OnPlayerDamaged leaves per victim/attacker pair per sample window, carrying the damage
//! gathered since the last one. A fight is a handful of records, never one per bullet.
//! Damage gathered in a window that never closes is not lost to the log: the death record ends it.
class LF_IncidentDamage
{
	static const int MAX_PAIRS = 256;

	protected static ref map<string, ref LF_IncidentDamageWindow> s_mWindows;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mWindows = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure sampler step. \return The damage to publish now, or 0 to hold it back.
	static float Offer(string pairKey, float damage, float nowSeconds, float windowSeconds)
	{
		if (damage <= 0)
			return 0;

		if (!s_mWindows)
			s_mWindows = new map<string, ref LF_IncidentDamageWindow>();

		LF_IncidentDamageWindow window = s_mWindows.Get(pairKey);
		if (!window)
		{
			// The table cannot grow without bound: start over past the cap.
			if (s_mWindows.Count() >= MAX_PAIRS)
				s_mWindows.Clear();

			window = new LF_IncidentDamageWindow();
			s_mWindows.Set(pairKey, window);
		}

		window.m_fPending += damage;
		if (nowSeconds - window.m_fLastPublish < windowSeconds)
			return 0;

		float released = window.m_fPending;
		window.m_fPending = 0;
		window.m_fLastPublish = nowSeconds;
		return released;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one hit on a character. Player-on-player only; self damage, environment damage and
	//! NPCs are not incidents. Never changes the damage.
	static void OnHit(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsServer() || !victim)
			return;

		if (!LF_IncidentConfig.IsEnabled(LF_IncidentConfig.KIND_DAMAGE))
			return;

		Instigator instigator = damageContext.instigator;
		if (!instigator)
			return;

		int attackerId = instigator.GetInstigatorPlayerID();
		PlayerManager players = GetGame().GetPlayerManager();
		if (attackerId <= 0 || !players)
			return;

		int victimId = players.GetPlayerIdFromControlledEntity(victim);
		if (victimId <= 0 || victimId == attackerId)
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		float damage = Offer(string.Format("%1>%2", attackerId, victimId), damageContext.damageValue, now, LF_IncidentConfig.DamageSampleSeconds());
		if (damage <= 0)
			return;

		LF_EventBus.Publish(LF_OnPlayerDamaged.Create(LF_Utils.GetPlayerUID(victimId), LF_Utils.GetPlayerUID(attackerId), damage, victim.GetOrigin(), LF_IncidentLog.WeaponOf(instigator.GetInstigatorEntity())));
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		bool hijacked = super.HijackDamageHandling(damageContext);
		if (!hijacked)
			LF_IncidentDamage.OnHit(GetOwner(), damageContext);

		return hijacked;
	}
}
