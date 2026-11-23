//------------------------------------------------------------------------------------------------
//! The welcome guide's wire. The client asks once per connection, when it first controls a body,
//! whether the guide is pending for its character; the server also pushes "show" at a first spawn.
//! Both answers go to the owner only. The key (LF_WelcomeGuide) opens the sheet locally at any time.
//! "Don't show again" and "Set waypoint" are asks the server checks against the player's own
//! character. Nothing is replicated as state; no RplProp, no BumpMe.
modded class SCR_PlayerController
{
	protected bool m_bLF_WelcomeGuideListening;
	protected bool m_bLF_WelcomeGuideAsked;

	//------------------------------------------------------------------------------------------------
	//! The listener lives on the process-global InputManager, so it leaves with this controller (#2545).
	void ~SCR_PlayerController()
	{
		if (!m_bLF_WelcomeGuideListening || !GetGame())
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener(LF_WelcomeGuide.INPUT_ACTION, EActionTrigger.DOWN, LF_OnWelcomeGuideKey);

		m_bLF_WelcomeGuideListening = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (GetGame().GetPlayerController() != this)
			return;

		LF_RegisterWelcomeGuideListener();
		if (to && !m_bLF_WelcomeGuideAsked)
		{
			m_bLF_WelcomeGuideAsked = true;
			Rpc(RpcAsk_LF_WelcomeGuide);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RegisterWelcomeGuideListener()
	{
		if (m_bLF_WelcomeGuideListening)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener(LF_WelcomeGuide.INPUT_ACTION, EActionTrigger.DOWN, LF_OnWelcomeGuideKey);
		m_bLF_WelcomeGuideListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnWelcomeGuideKey()
	{
		LF_WelcomeGuideSheet.Toggle();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WelcomeGuide()
	{
		int playerId = GetPlayerId();
		string characterId = LF_WelcomeGuide.CharacterIdOf(playerId);
		bool registered = LF_QuestManager.GetInstance() && LF_QuestManager.GetInstance().HasEarnedIdCard(LF_Utils.GetPlayerUID(playerId));
		if (!LF_WelcomeGuide.ResolveAutoShow(characterId, registered))
		{
			LF_Debug.Info(LF_WelcomeGuide.FEATURE, "player " + playerId + " joined; the guide is not pending for " + characterId);
			return;
		}

		LF_Debug.Info(LF_WelcomeGuide.FEATURE, "player " + playerId + " joined with the guide still pending for " + characterId);
		LF_SendWelcomeGuideShow();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell this player's client to open the guide by itself. On a listen host's own controller
	//! the engine runs the Owner handler at once inside Rpc().
	void LF_SendWelcomeGuideShow()
	{
		Rpc(RpcDo_LF_WelcomeGuideShow);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_WelcomeGuideShow()
	{
		LF_WelcomeGuideSheet.AutoOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: "Don't show again".
	void LF_AskWelcomeGuideDismiss()
	{
		Rpc(RpcAsk_LF_WelcomeGuideDismiss);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WelcomeGuideDismiss()
	{
		int playerId = GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.QUEST, "welcome-guide-dismiss", false))
			return;

		LF_WelcomeGuide.Dismiss(LF_WelcomeGuide.CharacterIdOf(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: "Set waypoint".
	void LF_AskWelcomeGuideWaypoint()
	{
		Rpc(RpcAsk_LF_WelcomeGuideWaypoint);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WelcomeGuideWaypoint()
	{
		int playerId = GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.QUEST, "welcome-guide-waypoint", false))
		{
			Rpc(RpcDo_LF_WelcomeGuideWaypoint, LF_EWelcomeWaypoint.REFUSED, string.Empty);
			return;
		}

		string labelKey;
		int result = LF_WelcomeGuide.Waypoint(playerId, labelKey);
		Rpc(RpcDo_LF_WelcomeGuideWaypoint, result, labelKey);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_WelcomeGuideWaypoint(int result, string labelKey)
	{
		LF_WelcomeGuideSheet.OnWaypointAnswer(result, labelKey);
	}
}
