//------------------------------------------------------------------------------------------------
//! Server-validated plot purchase (#2649). The player controller is the entity the client owns for
//! the whole session, so the request rides it. The request carries no plot id: the server picks the
//! plot from the caller's own character position (LF_PlotComponent.Buy).
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy the plot whose sign the local character stands at.
	void LF_RequestBuyProperty()
	{
		Rpc(RpcAsk_LF_BuyProperty);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BuyProperty()
	{
		int playerId = GetPlayerId();
		IEntity buyer = GetControlledEntity();
		if (LF_CuffEnforcement.Refuse(buyer, "plot purchase"))
			return;

		string plotKey;
		LF_EPlotBuy result = LF_PlotComponent.Buy(buyer, LF_Utils.GetPlayerUID(playerId), plotKey);

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
