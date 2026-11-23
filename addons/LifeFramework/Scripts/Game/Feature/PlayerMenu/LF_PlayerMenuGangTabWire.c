// LF_PlayerMenuGangTabWire.c: the Gang tab's wire (LF_PlayerMenuGangTab). The sheet asks when the tab
// opens; the server answers the asking controller's own player only, with LF_PlayerMenuGangTab.ServerBuild
// of GetPlayerId(), never of a player or gang the client names. A player in no gang gets an empty
// answer. One ask a second from the client, and the COMMS bucket behind it. Read only: the answer
// changes nothing, no RplProp, no BumpMe.

//------------------------------------------------------------------------------------------------
//! The client's copy of the last answer, so a reopen prints at once.
class LF_PlayerMenuGangView
{
	protected static ref LF_PlayerMenuGangData s_Data;
	protected static string s_sWire;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Data = null;
		s_sWire = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerMenuGangData Data()
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
		s_Data = LF_PlayerMenuGangData.Decode(wire);
		LF_Debug.Info(LF_PlayerMenuGangTab.FEATURE, "gang tab answer: " + wire.Length().ToString() + " chars");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlayerMenuGangView's statics (LF_StaticReset finds it by reflection).
class LF_PlayerMenuGangView_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlayerMenuGangView";
	}

	override void Apply()
	{
		LF_PlayerMenuGangView.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	static const int LF_PLAYER_MENU_GANG_ASK_MS = 1000; // lf-fixed: client throttle under LF_RateLimit's COMMS bucket

	protected int m_iLF_PlayerMenuGangAskedAt = -100000;

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for this player's crew facts. \return false when throttled.
	bool LF_AskPlayerMenuGang()
	{
		int now = System.GetTickCount();
		if (now - m_iLF_PlayerMenuGangAskedAt < LF_PLAYER_MENU_GANG_ASK_MS)
			return false;

		m_iLF_PlayerMenuGangAskedAt = now;
		Rpc(RpcAsk_LF_PlayerMenuGang);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: no argument on purpose; the crew is always the sender's own.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlayerMenuGang()
	{
		int playerId = GetPlayerId();
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_PlayerMenuGangTab.FEATURE, "gang tab ask refused: the controller has no player");
			return;
		}

		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.COMMS, "player menu gang", false))
			return;

		string wire;
		LF_PlayerMenuGangData data = LF_PlayerMenuGangTab.ServerBuild(playerId);
		if (data)
			wire = data.Encode();

		LF_Debug.Info(LF_PlayerMenuGangTab.FEATURE, string.Format("gang tab sent to player %1: %2 chars", playerId, wire.Length()));
		Rpc(RpcDo_LF_PlayerMenuGang, wire);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PlayerMenuGang(string wire)
	{
		LF_PlayerMenuSheet.OnGangData(wire);
	}
}
