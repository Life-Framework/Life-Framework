//------------------------------------------------------------------------------------------------
//! Player-targeted toast delivery. The controller is the one replicated entity every connected
//! player owns for the whole session (alive, dead, or still in the faction menu), so an Owner
//! RPC on it reaches exactly that client and nobody else.
modded class SCR_PlayerController
{
	//! The last toast message this client received. The agent harness reports it with `state`,
	//! so a multi-client run can prove an alert reached the right player.
	protected static string s_sLF_LastToast;

	//------------------------------------------------------------------------------------------------
	static string LF_GetLastToast()
	{
		return s_sLF_LastToast;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show a toast to this controller's player. The listen host's own controller is
	//! local, so it calls the handler directly (an RPC is never executed on the sender).
	void LF_DeliverNotification(string title, string message, float duration, LF_ENotificationType type, string messageArg)
	{
		if (GetGame().GetPlayerController() == this)
		{
			RpcDo_LF_ShowNotification(title, message, duration, type, messageArg);
			return;
		}

		Rpc(RpcDo_LF_ShowNotification, title, message, duration, type, messageArg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowNotification(string title, string message, float duration, LF_ENotificationType type, string messageArg)
	{
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			LF_Debug.Error("Notify", "toast dropped: no LF_NotificationManagerComponent on this client");
			return;
		}

		s_sLF_LastToast = message;
		LF_Debug.Log("Notify", string.Format("toast received: %1", message));
		manager.ShowNotificationToast(title, message, duration, type, messageArg);
	}
}
