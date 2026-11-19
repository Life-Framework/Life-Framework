//------------------------------------------------------------------------------------------------
//! Server-validated house purchase (#3207). The request rides the player controller, as the plot purchase
//! does (LF_PropertyPlayerController). It names the house id of the sign used; the server sells it only
//! when a live sign with that id is within reach of the caller's character (LF_HouseSaleComponent.Buy),
//! which also refuses a cuffed, downed or dead buyer. The client never sends a price or an owner.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy the house of the sign the local character used.
	void LF_RequestBuyHouse(string houseId)
	{
		Rpc(RpcAsk_LF_BuyHouse, houseId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BuyHouse(string askedHouseId)
	{
		int playerId = GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.PURCHASE, "house purchase"))
			return;

		string houseId;
		LF_EHouseBuy result = LF_HouseSaleComponent.Buy(GetControlledEntity(), LF_Utils.GetPlayerUID(playerId), houseId, askedHouseId);
		string message = LF_HouseSaleRequest.MessageKey(result);
		LF_ENotificationType type = LF_ENotificationType.WARNING;
		if (result == LF_EHouseBuy.BOUGHT)
			type = LF_ENotificationType.INFO;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Property_Title", message, 6.0, type, houseId);
	}
}

//------------------------------------------------------------------------------------------------
//! The words of a house-purchase answer.
class LF_HouseSaleRequest
{
	//------------------------------------------------------------------------------------------------
	//! \return The string key of the toast that answers a purchase.
	static string MessageKey(LF_EHouseBuy result)
	{
		switch (result)
		{
			case LF_EHouseBuy.BOUGHT:
				return "#LF-Property_HouseBought";
			case LF_EHouseBuy.REFUSED_NO_CASH:
				return "#LF-Property_HouseNoCash";
			case LF_EHouseBuy.REFUSED_OWNED:
				return "#LF-Property_HouseOwned";
			case LF_EHouseBuy.REFUSED_NOT_ABLE:
				return "#LF-Interact_NotAlive";
		}

		return "#LF-Property_HouseNotHere";
	}
}
