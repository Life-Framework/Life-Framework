//------------------------------------------------------------------------------------------------
//! Server-validated plot purchase (#2649). The player controller is the entity the client owns for
//! the whole session, so the request rides it. The request names the plot key of the sign used; the
//! server sells it only when a live sign with that key is within reach of the caller (LF_PlotComponent.Buy).
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy the plot of the sign the local character used.
	void LF_RequestBuyProperty(string plotKey)
	{
		Rpc(RpcAsk_LF_BuyProperty, plotKey);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BuyProperty(string askedKey)
	{
		int playerId = GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.PURCHASE, "plot purchase"))
			return;

		IEntity buyer = GetControlledEntity();
		if (LF_CuffEnforcement.Refuse(buyer, "plot purchase"))
			return;

		// A dead or downed body still carries its cash; only a living, conscious buyer pays.
		if (!LF_Utils.IsAliveState(buyer))
		{
			LF_Debug.Warn(LF_PlotComponent.FEATURE, string.Format("plot buy refused for player %1: not alive and conscious", playerId));
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Property_Title", "#LF-Interact_NotAlive", 6.0, LF_ENotificationType.WARNING);
			return;
		}

		string plotKey;
		LF_EPlotBuy result = LF_PlotComponent.Buy(buyer, LF_Utils.GetPlayerUID(playerId), plotKey, askedKey);

		string message = "#LF-Property_Bought";
		LF_ENotificationType type = LF_ENotificationType.INFO;
		switch (result)
		{
			case LF_EPlotBuy.BOUGHT:
				break;
			case LF_EPlotBuy.REFUSED_NO_CASH:
				message = "#LF-Property_NoCash";
				type = LF_ENotificationType.WARNING;
				break;
			case LF_EPlotBuy.REFUSED_OWNED:
				message = "#LF-Property_Owned";
				type = LF_ENotificationType.WARNING;
				break;
			default:
				message = "#LF-Property_NotHere";
				type = LF_ENotificationType.WARNING;
				break;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Property_Title", message, 6.0, type, plotKey);
	}
}
