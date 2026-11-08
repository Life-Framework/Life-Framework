//------------------------------------------------------------------------------------------------
//! Player-targeted toast delivery. The controller is the one replicated entity every connected
//! player owns for the whole session (alive, dead, or still in the faction menu), so an Owner
//! RPC on it reaches exactly that client and nobody else.
modded class SCR_PlayerController
{
	//! The last toast message this client received. The agent harness reports it with `state`,
	//! so a multi-client run can prove an alert reached the right player.
	protected static string s_sEL_LastToast;

	//------------------------------------------------------------------------------------------------
	static string EL_GetLastToast()
	{
		return s_sEL_LastToast;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show a toast to this controller's player. The listen host's own controller is
	//! local, so it calls the handler directly (an RPC is never executed on the sender).
	void EL_DeliverNotification(string title, string message, float duration, EL_ENotificationType type, string messageArg)
	{
		if (GetGame().GetPlayerController() == this)
		{
			RpcDo_EL_ShowNotification(title, message, duration, type, messageArg);
			return;
		}

		Rpc(RpcDo_EL_ShowNotification, title, message, duration, type, messageArg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_ShowNotification(string title, string message, float duration, EL_ENotificationType type, string messageArg)
	{
		EL_NotificationManagerComponent manager = EL_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			EL_Debug.Error("Notify", "toast dropped: no EL_NotificationManagerComponent on this client");
			return;
		}

		s_sEL_LastToast = message;
		EL_Debug.Log("Notify", string.Format("toast received: %1", message));
		manager.ShowNotificationToast(title, message, duration, type, messageArg);
	}
}
