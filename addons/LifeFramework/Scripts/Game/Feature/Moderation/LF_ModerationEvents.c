//------------------------------------------------------------------------------------------------
//! Moderation events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! One sampled stretch of player-on-player damage. LF_IncidentDamage folds every hit between a
//! victim and an attacker into at most one event per sample window; m_fDamage is the total in it.
class LF_OnPlayerDamaged : LF_Event
{
	string m_sVictimUid;
	string m_sAttackerUid;
	float m_fDamage;
	vector m_vPosition;
	string m_sWeapon;

	static LF_OnPlayerDamaged Create(string victimUid, string attackerUid, float damage, vector position, string weapon)
	{
		LF_OnPlayerDamaged e = new LF_OnPlayerDamaged();
		e.m_sVictimUid = victimUid;
		e.m_sAttackerUid = attackerUid;
		e.m_fDamage = damage;
		e.m_vPosition = position;
		e.m_sWeapon = weapon;
		return e;
	}
}

//! A zone was added, changed, or removed (LF_Zones). m_sRule is "safe", "lawless" or "normal".
class LF_OnZoneChanged : LF_Event
{
	string m_sZone;
	string m_sRule;
	bool m_bRemoved;

	static LF_OnZoneChanged Create(string zone, string rule, bool removed)
	{
		LF_OnZoneChanged e = new LF_OnZoneChanged();
		e.m_sZone = zone;
		e.m_sRule = rule;
		e.m_bRemoved = removed;
		return e;
	}
}

//! A player held another up: weapon in hand, hands-up call, in range (LF_HoldUp). Intent only; no
//! rule changes because of it. The incident log records it (kind "holdup").
class LF_OnHoldUp : LF_Event
{
	string m_sActorUid;
	string m_sTargetUid;
	//! Where the actor stood.
	vector m_vPosition;

	static LF_OnHoldUp Create(string actorUid, string targetUid, vector position)
	{
		LF_OnHoldUp e = new LF_OnHoldUp();
		e.m_sActorUid = actorUid;
		e.m_sTargetUid = targetUid;
		e.m_vPosition = position;
		return e;
	}
}

//! An administrator did something: a Game Master editor action, #kick, #ban, !wadd, !handled
//! (LF_AdminAction). The incident log records it (kind "admin") so moderators are accountable.
class LF_OnAdminAction : LF_Event
{
	string m_sAdminUid;
	//! The player acted on; empty when the action has none.
	string m_sTargetUid;
	string m_sAction;
	vector m_vPosition;

	static LF_OnAdminAction Create(string adminUid, string targetUid, string action, vector position)
	{
		LF_OnAdminAction e = new LF_OnAdminAction();
		e.m_sAdminUid = adminUid;
		e.m_sTargetUid = targetUid;
		e.m_sAction = action;
		e.m_vPosition = position;
		return e;
	}
}

//! A player kept a raised weapon on another player for the aim window (LF_AimIntent). Intent
//! only; no rule changes because of it. The incident log records it (kind "aim").
class LF_OnAimedAt : LF_Event
{
	string m_sActorUid;
	string m_sTargetUid;
	//! Where the actor stood.
	vector m_vPosition;

	static LF_OnAimedAt Create(string actorUid, string targetUid, vector position)
	{
		LF_OnAimedAt e = new LF_OnAimedAt();
		e.m_sActorUid = actorUid;
		e.m_sTargetUid = targetUid;
		e.m_vPosition = position;
		return e;
	}
}
