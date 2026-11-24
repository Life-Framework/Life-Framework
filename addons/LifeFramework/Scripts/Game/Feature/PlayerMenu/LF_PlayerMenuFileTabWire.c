// LF_PlayerMenuFileTabWire.c: the My file tab's wire (LF_PlayerMenuFileTab). The sheet asks when the tab
// opens; the server answers the asking controller's own player only, with LF_PlayerMenuFileTab.ServerBuild
// of GetPlayerId(), never of a player the client names. One ask a second from the client, and the COMMS
// bucket behind it. Read only: no RplProp, no BumpMe, nothing saved.

//------------------------------------------------------------------------------------------------
//! The client's copy of the last answer, so a reopen prints at once.
class LF_PlayerMenuFileView
{
	protected static ref LF_PlayerMenuFileData s_Data;
	protected static string s_sWire;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Data = null;
		s_sWire = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerMenuFileData Data()
	{
		return s_Data;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer. \return True when it changed what the tab prints.
	static bool Apply(string wire)
	{
		if (wire == s_sWire)
			return false;

		s_sWire = wire;
		s_Data = LF_PlayerMenuFileData.Decode(wire);
		LF_Debug.Info(LF_PlayerMenuFileTab.FEATURE, "file tab answer: " + wire.Length().ToString() + " chars");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlayerMenuFileView's statics (LF_StaticReset finds it by reflection).
class LF_PlayerMenuFileView_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlayerMenuFileView";
	}

	override void Apply()
	{
		LF_PlayerMenuFileView.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	static const int LF_PLAYER_MENU_FILE_ASK_MS = 1000; // lf-fixed: client throttle under LF_RateLimit's COMMS bucket

	protected int m_iLF_PlayerMenuFileAskedAt = -100000;

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for this player's file. \return false when throttled.
	bool LF_AskPlayerMenuFile()
	{
		int now = System.GetTickCount();
		if (now - m_iLF_PlayerMenuFileAskedAt < LF_PLAYER_MENU_FILE_ASK_MS)
			return false;

		m_iLF_PlayerMenuFileAskedAt = now;
		Rpc(RpcAsk_LF_PlayerMenuFile);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: no argument on purpose; the file is always the sender's own.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlayerMenuFile()
	{
		int playerId = GetPlayerId();
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_PlayerMenuFileTab.FEATURE, "file tab ask refused: the controller has no player");
			return;
		}

		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.COMMS, "player menu file", false))
			return;

		string wire = LF_PlayerMenuFileTab.ServerBuild(playerId).Encode();
		LF_Debug.Info(LF_PlayerMenuFileTab.FEATURE, string.Format("file tab sent to player %1: %2 chars", playerId, wire.Length()));
		Rpc(RpcDo_LF_PlayerMenuFile, wire);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PlayerMenuFile(string wire)
	{
		LF_PlayerMenuSheet.OnFileData(wire);
	}
}
