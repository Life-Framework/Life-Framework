//------------------------------------------------------------------------------------------------
//! One victim/attacker pair's sampler state.
class LF_IncidentDamageWindow : Managed
{
	float m_fLastPublish = -1000000;
	float m_fPending;
	//! Who and where the held damage belongs to, so it can be published without a later hit (#2414).
	string m_sVictimUid;
	string m_sAttackerUid;
	vector m_vPosition;
	string m_sWeapon;
}

//------------------------------------------------------------------------------------------------
//! Damage sampler for the incident log. Every hit between two players reaches Offer; at most one
//! LF_OnPlayerDamaged leaves per victim/attacker pair per sample window, carrying the damage
//! gathered since the last one. A fight is a handful of records, never one per bullet.
//! Damage held back inside a window is not lost to the log (#2414): a 1 s tick publishes it once
//! its window has ended, and a death or a downing publishes the victim's held damage first.
class LF_IncidentDamage
{
	static const int MAX_PAIRS = 256;
	static const int TICK_MS = 1000; // lf-fixed: flush granularity, not tuning

	protected static ref map<string, ref LF_IncidentDamageWindow> s_mWindows;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTick();
		s_mWindows = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts the 1 s flush tick. Safe to call twice.
	static void StartTick()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static void StopTick()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsTicking()
	{
		return s_bTicking;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld())
			return;

		FlushDue(GetGame().GetWorld().GetWorldTime() / 1000, LF_IncidentConfig.DamageSampleSeconds());
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
			// The table cannot grow without bound: start over past the cap, without dropping what is held.
			if (s_mWindows.Count() >= MAX_PAIRS)
			{
				FlushWhere(string.Empty, nowSeconds);
				s_mWindows.Clear();
			}

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
	//! One hit, the part that needs no entity: samples it and publishes what leaves now. The window
	//! keeps who, where and with what, so the damage held back can still be published later.
	//! \return The damage published by this call (0 when held back).
	static float Hit(string pairKey, string victimUid, string attackerUid, float damage, vector position, string weapon, float nowSeconds, float windowSeconds)
	{
		float released = Offer(pairKey, damage, nowSeconds, windowSeconds);
		if (!s_mWindows)
			return 0;

		LF_IncidentDamageWindow window = s_mWindows.Get(pairKey);
		if (!window)
			return 0;

		window.m_sVictimUid = victimUid;
		window.m_sAttackerUid = attackerUid;
		window.m_vPosition = position;
		window.m_sWeapon = weapon;
		if (released > 0)
			LF_EventBus.Publish(LF_OnPlayerDamaged.Create(victimUid, attackerUid, released, position, weapon));

		return released;
	}

	//------------------------------------------------------------------------------------------------
	//! Publishes the damage held in every window whose sample window has ended.
	//! \return How many windows were flushed.
	static int FlushDue(float nowSeconds, float windowSeconds)
	{
		if (!s_mWindows)
			return 0;

		array<ref LF_IncidentDamageWindow> due = {};
		foreach (string pairKey, LF_IncidentDamageWindow window : s_mWindows)
		{
			if (window && window.m_fPending > 0 && nowSeconds - window.m_fLastPublish >= windowSeconds)
				due.Insert(window);
		}

		foreach (LF_IncidentDamageWindow dueWindow : due)
			Publish(dueWindow, nowSeconds);

		return due.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Publishes at once the damage held for a victim (a death or a downing is about to be logged,
	//! so the hits that led to it come first). Empty uid: every window.
	//! \return How many windows were flushed.
	static int FlushWhere(string victimUid, float nowSeconds)
	{
		if (!s_mWindows)
			return 0;

		array<ref LF_IncidentDamageWindow> held = {};
		foreach (string pairKey, LF_IncidentDamageWindow window : s_mWindows)
		{
			if (window && window.m_fPending > 0 && (victimUid.IsEmpty() || window.m_sVictimUid == victimUid))
				held.Insert(window);
		}

		foreach (LF_IncidentDamageWindow heldWindow : held)
			Publish(heldWindow, nowSeconds);

		return held.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the victim's held damage reaches the log now. Called before their death or downing.
	static void FlushVictim(string victimUid)
	{
		if (victimUid.IsEmpty() || !GetGame() || !GetGame().GetWorld())
			return;

		FlushWhere(victimUid, GetGame().GetWorld().GetWorldTime() / 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Publish(notnull LF_IncidentDamageWindow window, float nowSeconds)
	{
		float released = window.m_fPending;
		window.m_fPending = 0;
		window.m_fLastPublish = nowSeconds;
		if (released <= 0)
			return;

		LF_Debug.Info(LF_IncidentLog.FEATURE, string.Format("flushed %1 damage held for %2 from %3", Math.Round(released), window.m_sVictimUid, window.m_sAttackerUid));
		LF_EventBus.Publish(LF_OnPlayerDamaged.Create(window.m_sVictimUid, window.m_sAttackerUid, released, window.m_vPosition, window.m_sWeapon));
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
		Hit(string.Format("%1>%2", attackerId, victimId), LF_Utils.GetPlayerUID(victimId), LF_Utils.GetPlayerUID(attackerId), damageContext.damageValue, victim.GetOrigin(), LF_IncidentLog.WeaponOf(instigator.GetInstigatorEntity()), now, LF_IncidentConfig.DamageSampleSeconds());
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
