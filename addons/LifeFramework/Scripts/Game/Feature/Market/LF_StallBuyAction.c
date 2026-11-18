//------------------------------------------------------------------------------------------------
//! "Buy from the stall" on a rented market stall (#2926). It buys the cheapest listing nobody has
//! reserved. The client only decides whether to show the prompt (the stall is rented, replicated); the
//! server decides everything in `LF_StallTrade.Buy` (reach, clock, listing, cash, stock, room) and answers
//! with a toast to the buyer and one to the seller. Nothing is trusted from the client, not even which
//! listing: the listing list is server state and the card has no listing screen yet.
//------------------------------------------------------------------------------------------------
class LF_StallBuyAction : ScriptedUserAction
{
	protected LF_StallComponent m_Stall;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_Stall = LF_Component<LF_StallComponent>.Find(pOwnerEntity);
		if (!m_Stall)
			LF_Debug.Error(LF_StallConfig.FEATURE, "Buy action on an entity with no LF_StallComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Stall || !m_Stall.IsRented())
			return false;

		LF_StallConfig config = LF_StallConfig.Get();
		return config && config.m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Stall)
			return false;

		LF_StallConfig config = LF_StallConfig.Get();
		int day;
		int hour;
		if (!config || !LF_StallClock.Now(day, hour) || !LF_StallRules.IsMarketOpen(config, day, hour))
		{
			SetCannotPerformReason("#LF-Stall_Closed");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Stall)
			return false;

		outName = "#LF-Stall_Buy";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buys for the user and tells the buyer, and the seller when they are in the world.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !m_Stall || !pUserEntity)
			return;

		int paid;
		int sellerPay;
		LF_EStallResult rule;
		LF_EStallTrade result = LF_EStallTrade.BAD_INPUT;
		int listingId = LF_StallTrade.CheapestOpen(m_Stall.GetStall());
		if (listingId > 0)
			result = LF_StallTrade.Buy(m_Stall, pUserEntity, LF_Utils.GetPlayerUid(pUserEntity), listingId, paid, sellerPay, rule);

		string message = "#LF-Stall_Bought";
		string arg = LF_MoneyFormat.FormatMoney(paid);
		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (listingId <= 0)
		{
			message = "#LF-Stall_NothingListed";
			type = LF_ENotificationType.WARNING;
		}
		else if (result != LF_EStallTrade.OK)
		{
			type = LF_ENotificationType.WARNING;
			message = RefusalKey(result, rule);
		}

		int buyerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (buyerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(buyerId, "#LF-Stall_Title", message, 6.0, type, arg);

		if (result != LF_EStallTrade.OK)
			return;

		IEntity holder = LF_StallTrade.FindHolder(m_Stall.GetHolderKey());
		int sellerId;
		if (holder)
			sellerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(holder);

		if (sellerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(sellerId, "#LF-Stall_Title", "#LF-Stall_Sold", 6.0, LF_ENotificationType.INFO, LF_MoneyFormat.FormatMoney(sellerPay));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The string key that tells the buyer why nothing was bought.
	protected string RefusalKey(LF_EStallTrade result, LF_EStallResult rule)
	{
		switch (result)
		{
			case LF_EStallTrade.BUYER_FULL:
				return "#LF-Stall_BuyerFull";
			case LF_EStallTrade.NO_STOCK:
			case LF_EStallTrade.HOLDER_AWAY:
				return "#LF-Stall_Unavailable";
			case LF_EStallTrade.CASH_SHORT:
				return "#LF-Stall_NoCashBuy";
			case LF_EStallTrade.OUT_OF_REACH:
				return "#LF-Stall_Unavailable";
		}

		switch (rule)
		{
			case LF_EStallResult.CANT_AFFORD:
				return "#LF-Stall_NoCashBuy";
			case LF_EStallResult.SELF_BUY:
				return "#LF-Stall_OwnStall";
			case LF_EStallResult.CLOSED:
			case LF_EStallResult.NOT_RENTED:
				return "#LF-Stall_Closed";
		}

		return "#LF-Stall_Unavailable";
	}
}
