//------------------------------------------------------------------------------------------------
//! The wire of the personal file (LF_PlayerMenuSheet). The F2 key (gamepad: hold RB + Y) opens the
//! sheet and asks the server once; the server answers this controller's own player only, with
//! LF_PlayerMenuServer.Build of GetPlayerId(), never of a player the client names. One ask a
//! second from this client, and the server's rate limit behind it. No RplProp, no BumpMe.
modded class SCR_PlayerController
{
	static const string LF_PLAYER_MENU_ACTION = "LF_PlayerMenu";
	//! Client: an ask closer than this to the last one is not sent (the sheet keeps the last answer). The
	//! server counts the ask in the COMMS bucket (3 per 5 s), so one ask per window leaves the phone and
	//! the board two.
	static const int LF_PLAYER_MENU_ASK_MS = 5000; // lf-fixed: matches LF_RateLimit.COMMS_WINDOW_MS, not a server tunable

	protected bool m_bLF_PlayerMenuListening;
	protected int m_iLF_PlayerMenuAskedAt = -100000;

	//------------------------------------------------------------------------------------------------
	//! The listener lives on the process-global InputManager, so it leaves with this controller (#2545).
	void ~SCR_PlayerController()
	{
		if (!m_bLF_PlayerMenuListening || !GetGame())
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener(LF_PLAYER_MENU_ACTION, EActionTrigger.DOWN, LF_OnPlayerMenuKey);

		m_bLF_PlayerMenuListening = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterPlayerMenuListener();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RegisterPlayerMenuListener()
	{
		if (m_bLF_PlayerMenuListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener(LF_PLAYER_MENU_ACTION, EActionTrigger.DOWN, LF_OnPlayerMenuKey);
		m_bLF_PlayerMenuListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPlayerMenuKey()
	{
		LF_PlayerMenuSheet.Toggle();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for this player's file. \return false when throttled.
	bool LF_AskPlayerMenu()
	{
		int now = System.GetTickCount();
		if (now - m_iLF_PlayerMenuAskedAt < LF_PLAYER_MENU_ASK_MS)
		{
			LF_Debug.Info(LF_PlayerMenuServer.FEATURE, "file ask skipped: asked under 5 s ago, the last answer stays");
			return false;
		}

		m_iLF_PlayerMenuAskedAt = now;
		Rpc(RpcAsk_LF_PlayerMenu);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: no argument on purpose; the file is always the sender's own.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlayerMenu()
	{
		int playerId = GetPlayerId();
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "file ask refused: the controller has no player");
			return;
		}

		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.COMMS, "player menu", false))
			return;

		string wire = LF_PlayerMenuServer.Build(playerId).Encode();
		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, string.Format("file sent to player %1: %2 chars", playerId, wire.Length()));
		Rpc(RpcDo_LF_PlayerMenu, wire);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PlayerMenu(string wire)
	{
		LF_PlayerMenuSheet.OnData(wire);
	}
}
