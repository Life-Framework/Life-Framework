//------------------------------------------------------------------------------------------------
//! EL_DevGate — the one server-side gate for developer tools that mutate player state: the debug
//! menu (cash, faction, XP, job, wanted, survival) and the agent-harness RPCs (stand, pose, use).
//!
//! A request is allowed only when the running game mode opts in (EL_GameModeRoleplay
//! m_bDevToolsEnabled, set on GameMode_Roleplay_Debug.et for the DebugWorld) or the requesting
//! player holds EPlayerRole.ADMINISTRATOR. The production GameMode_Roleplay.et leaves the opt-in
//! off, so on a live server only admins pass. Client-side checks that use this are cosmetic;
//! the server-side RPC handlers are the authority.
class EL_DevGate
{
	//------------------------------------------------------------------------------------------------
	//! The gate decision. Pure, so it is LOGIC-tier tested (EL_Test_DebugMenuGate).
	//! \param devToolsEnabled The game mode opts every player in (development worlds).
	//! \param isAdmin The requesting player holds EPlayerRole.ADMINISTRATOR.
	//! \return true when dev tools may run for this player.
	static bool IsAllowed(bool devToolsEnabled, bool isAdmin)
	{
		return devToolsEnabled || isAdmin;
	}

	//------------------------------------------------------------------------------------------------
	//! The agent harness opens its own gate (stand, pose, use; never the debug menu) in a build
	//! compiled with -scrDefine LF_HARNESS (agent-box/session.mjs up --mission playtest). The define
	//! changes the script checksum, so only clients started with the same define can join such a
	//! server: a real player's client is refused at the door ("remote script source code checksum
	//! does not match"). tools\cli host start never passes it, so the hosted gate stays closed.
	//! \return true only in a build compiled with -scrDefine LF_HARNESS.
	static bool HarnessBuild()
	{
#ifdef LF_HARNESS
		return true;
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side check for a harness RPC on the sender's own character (stand, pose, use).
	//! \param character The character the RPC arrived on.
	//! \param request Short request name for the log line.
	static bool CheckHarnessRequest(IEntity character, string request)
	{
		if (HarnessBuild())
		{
			EL_Debug.Info("AgentHarness", request + " allowed: LF_HARNESS build");
			return true;
		}

		return CheckCharacterRequest(character, "AgentHarness", request);
	}

	//------------------------------------------------------------------------------------------------
	//! Looks up the gate inputs for a player.
	//! \param playerId The requesting player; 0 (unknown) is never an administrator.
	static bool IsAllowedForPlayer(int playerId)
	{
		bool devToolsEnabled = false;
		EL_GameModeRoleplay gameMode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			devToolsEnabled = gameMode.AreDevToolsEnabled();

		bool isAdmin = false;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerId > 0)
			isAdmin = playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR);

		return IsAllowed(devToolsEnabled, isAdmin);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side check for an RPC hosted on the sender's owned character. A client can only RPC
	//! on entities it owns, so the player controlling the character is the sender. Logs the
	//! refusal through EL_Debug so a refused request is never silent.
	//! \param character The character the RPC arrived on.
	//! \param feature EL_Debug feature tag (e.g. "DebugMenu", "AgentHarness").
	//! \param request Short request name for the log line.
	//! \return true when the request may proceed.
	static bool CheckCharacterRequest(IEntity character, string feature, string request)
	{
		int playerId = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && character)
			playerId = playerManager.GetPlayerIdFromControlledEntity(character);

		if (IsAllowedForPlayer(playerId))
			return true;

		EL_Debug.Warn(feature, string.Format("%1 refused: player %2 is not an administrator and dev tools are disabled in this game mode", request, playerId));
		return false;
	}
}
