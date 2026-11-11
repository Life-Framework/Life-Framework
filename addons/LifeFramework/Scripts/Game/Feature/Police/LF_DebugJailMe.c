//! Playtest "Send me to jail" board outside the jail (LF_SpawnLogic.m_bDebugJailMe, on in
//! GameMode_Roleplay_Debug only). A solo tester jails themselves through the real sentence path
//! (LF_Jail.Imprison) for a short time. The server decides; the client only offers the action.
class LF_DebugJailMe
{
	static const int SENTENCE_SECONDS = 60;

	//------------------------------------------------------------------------------------------------
	//! True when the running game mode switched the playtest jail board on.
	static bool IsEnabled()
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (!spawnLogic)
			return false;

		return spawnLogic.IsDebugJailMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: jails the player for SENTENCE_SECONDS. Off in any mode that did not enable it.
	//! \return False when disabled, the uid is empty, or the jail refuses (no cell, already serving).
	static bool Perform(bool enabled, string playerUid, IEntity body)
	{
		if (!enabled)
		{
			LF_Debug.Warn("Jail", "send-me-to-jail refused: not enabled in this game mode");
			return false;
		}

		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Jail", "send-me-to-jail refused: no account id");
			return false;
		}

		bool jailed = LF_Jail.Imprison(playerUid, SENTENCE_SECONDS, 0, body);
		if (jailed)
			LF_Debug.Info("Jail", string.Format("send-me-to-jail: %1 jailed for %2 s", playerUid, SENTENCE_SECONDS));
		return jailed;
	}
}

//! "Send me to jail" on the playtest board by the jail entrance.
class LF_DebugJailMeAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user != null && LF_DebugJailMe.IsEnabled();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_DebugJailMe.IsEnabled())
			return false;

		// The cell registers on the server only; a client leaves this check to the server.
		if (Replication.IsServer() && !LF_Jail.HasCell())
		{
			SetCannotPerformReason("#EL-Jail_NoCell");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		LF_DebugJailMe.Perform(LF_DebugJailMe.IsEnabled(), LF_Utils.GetPlayerUid(pUserEntity), pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-DebugJailMe_Action";
		return true;
	}
}
