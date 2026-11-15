//------------------------------------------------------------------------------------------------
//! Session events on the server bus (docs/architecture/events.md). Fields are plain values: ids,
//! uids and flags, never IEntity or EntityID. Published on the server only; nothing replicates.
//! A new event of this feature is appended to this file, not to a central list.

//! A player spawned (first join, respawn or restart load). Published by LF_Session.OnPlayerSpawned
//! from the game mode's spawn hook, after the core spawn work ran. M4 features greet or tell a
//! player something by subscribing, without editing the game mode.
class LF_OnPlayerSpawnedEvent : LF_Event
{
	//! Server-side player id; never sent to clients.
	int m_iPlayerId;
	//! Account key (LF_Utils.GetAccountUID); empty when the identity has not arrived yet.
	string m_sAccountUid;
	//! True the first time this uid spawns since the world (session) started, false on later spawns.
	bool m_bFirstThisSession;

	static LF_OnPlayerSpawnedEvent Create(int playerId, string accountUid, bool firstThisSession)
	{
		LF_OnPlayerSpawnedEvent e = new LF_OnPlayerSpawnedEvent();
		e.m_iPlayerId = playerId;
		e.m_sAccountUid = accountUid;
		e.m_bFirstThisSession = firstThisSession;
		return e;
	}
}
