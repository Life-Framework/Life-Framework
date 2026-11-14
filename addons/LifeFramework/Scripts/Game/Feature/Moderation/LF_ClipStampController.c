//------------------------------------------------------------------------------------------------
//! The wire of the clip stamp (#1173). The player's key toggles the stamp on the client. Showing
//! it asks the server once (and again every few minutes while it is up); the server answers the
//! asker only, with its real clock, the server name, the tail of the player's id and the owner's
//! allow flag. Never per frame, no RplProp, no BumpMe.
modded class SCR_PlayerController
{
	protected bool m_bLF_ClipStampListening;

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterClipStampListener();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: listen for the key once. No local input on a dedicated server.
	protected void LF_RegisterClipStampListener()
	{
		if (m_bLF_ClipStampListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_ClipStamp", EActionTrigger.DOWN, LF_OnClipStampKey);
		m_bLF_ClipStampListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnClipStampKey()
	{
		LF_ToggleClipStamp();
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry (also the harness): flip the stamp; showing it asks the server for the time.
	void LF_ToggleClipStamp()
	{
		if (!LF_ClipStamp.IsAllowed())
		{
			LF_Debug.Warn(LF_ClipStamp.FEATURE, "clip stamp refused: the owner turned it off");
			LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
			if (manager)
				manager.ShowNotificationToast("#LF-ClipStamp_Action", "#LF-ClipStamp_Denied", 4, LF_ENotificationType.INFO);

			return;
		}

		if (LF_ClipStamp.Toggle())
		{
			LF_AskClipStamp();
			if (LF_ClipStamp.IsSynced())
				LF_ClipStampHud.Refresh();
		}
		else
			LF_ClipStampHud.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for its clock.
	void LF_AskClipStamp()
	{
		if (Replication.IsServer())
		{
			LF_AnswerClipStamp();
			return;
		}

		Rpc(RpcAsk_LF_ClipStamp);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ClipStamp()
	{
		LF_AnswerClipStamp();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: build the answer for this player.
	protected void LF_AnswerClipStamp()
	{
		string server = LF_ClipStamp.ServerName(LF_IncidentConfig.ClipStampServerName());
		string suffix = LF_ClipStamp.IdTail(LF_Utils.GetPlayerUID(GetPlayerId()));
		bool allowed = LF_IncidentConfig.ClipStampAllowed();
		int now = System.GetUnixTime();
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_ClipStamp(now, server, suffix, allowed);
		else
			Rpc(RpcDo_LF_ClipStamp, now, server, suffix, allowed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ClipStamp(int serverUnix, string server, string suffix, bool allowed)
	{
		LF_ClipStamp.Sync(serverUnix, server, suffix, allowed, System.GetTickCount());
		LF_ClipStampHud.Refresh();
		if (!allowed)
			LF_Debug.Warn(LF_ClipStamp.FEATURE, "clip stamp hidden: the owner turned it off");
	}
}
