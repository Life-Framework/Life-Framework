//------------------------------------------------------------------------------------------------
//! The chalk price board at the trader (#2847): one line per curved good with today's price, and a
//! down arrow when it stands below its base. It is a board kind (LF_BoardProviders): reading it asks
//! the server, which builds the answer fresh from the market book and replies once to that reader;
//! nothing is replicated, so there is no join-in-progress state and a read never shows a stale price.
//! A disabled market (or one that lists no goods) answers an empty board, painted as "no prices today".
//! Wire shape: per good one text (the item's display name or key) in m_aTexts and two numbers in
//! m_aNumbers (today's price, 1 when it stands below base else 0). The base and the curve stay on the server.
//------------------------------------------------------------------------------------------------
class LF_PriceBoard
{
	static const string FEATURE = "Market";
	static const string KIND = "prices";
	static const string READ_ACTION = "LF_PriceBoardReadAction";
	static const int NUMBERS_PER_GOOD = 2;	// lf-fixed: wire stride (price, below-base flag), not a tunable
	static const string ARROW_DOWN = "v";
	//! How far from the reader a stall's trader is looked for.
	static const float TRADER_RANGE = 8.0; // lf-fixed: one stall's reach; the next stall stands further off

	//------------------------------------------------------------------------------------------------
	//! Server: the answer for one read, in config order. Empty when the market is off.
	static LF_BoardAnswer BuildAnswerAt(int nowUnix)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (!config || !config.m_bEnabled || !config.m_aGoods)
			return answer;

		foreach (LF_MarketGoodDef def : config.m_aGoods)
		{
			if (!def)
				continue;

			// A good priced per lot (#3622) shows its lot after the name ("Grain sack x200"): the price is per lot.
			string name = LF_ShopManagerComponent.ResolveDisplayName(def.m_sGood);
			if (def.Lot() > 1)
				name = name + " x" + def.Lot().ToString();

			answer.m_aTexts.Insert(name);
			int price = LF_MarketBook.CurrentPriceAt(def.m_sGood, nowUnix);
			answer.m_aNumbers.Insert(price);
			// Only what the board shows crosses: the arrow, never the base or the curve.
			if (price < def.m_iBase)
				answer.m_aNumbers.Insert(1);
			else
				answer.m_aNumbers.Insert(0);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Pure: the prefabs of the posted rows a trader buys (value above 0), in list order.
	static array<ResourceName> PostedGoods(array<ref LF_TraderItem> list)
	{
		array<ResourceName> goods = {};
		if (!list)
			return goods;

		foreach (LF_TraderItem row : list)
		{
			if (row && row.m_bPosted && row.m_ValuePerItem > 0 && !row.m_ItemPrefab.IsEmpty())
				goods.Insert(row.m_ItemPrefab);
		}

		return goods;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the board of a stall (first-two-hours review A20): the trader's posted goods with what one
	//! unit sells for there right now (LF_TraderSale.QuoteUnits, the price its "Sell 1" row shows), and
	//! the down arrow for a curved good below its base. A good that sells only by the lot is left off.
	//! \return null when the trader posts nothing (the caller shows the market's goods instead).
	static LF_BoardAnswer BuildTraderAnswerAt(LF_TraderManagerComponent trader, int nowUnix)
	{
		if (!trader)
			return null;

		array<ResourceName> goods = PostedGoods(trader.m_aTradableItems);
		if (goods.IsEmpty())
			return null;

		LF_BoardAnswer answer = new LF_BoardAnswer();
		foreach (ResourceName good : goods)
		{
			int price = LF_TraderSale.QuoteUnits(trader, good, 1, nowUnix);
			if (price <= 0)
				continue;

			answer.m_aTexts.Insert(LF_ShopManagerComponent.ResolveDisplayName(good));
			answer.m_aNumbers.Insert(price);
			LF_MarketGoodDef def = LF_MarketBook.Curved(good);
			if (def && price < def.m_iBase)
				answer.m_aNumbers.Insert(1);
			else
				answer.m_aNumbers.Insert(0);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the trader whose stall the reader stands at, or null. A reader is always at the board
	//! (LF_ActionSite), so the trader within TRADER_RANGE of the reader is the board's own.
	static LF_TraderManagerComponent TraderNear(int playerId)
	{
		IEntity reader = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!reader)
			return null;

		IEntity owner = LF_ActionSite.FindNear(reader.GetOrigin(), TRADER_RANGE, "LF_TraderSellAction");
		if (!owner)
			return null;

		return LF_Component<LF_TraderManagerComponent>.Find(owner);
	}

	//------------------------------------------------------------------------------------------------
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		int nowUnix = System.GetUnixTime();
		LF_BoardAnswer answer = BuildTraderAnswerAt(TraderNear(playerId), nowUnix);
		if (!answer)
			answer = BuildAnswerAt(nowUnix);
		LF_Debug.Info(FEATURE, string.Format("price board read by player %1: goods=%2", playerId, answer.m_aTexts.Count()));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the lines of the card. A text with no pair of numbers is dropped.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-Market_BoardTitle")));

		int goods = answer.m_aTexts.Count();
		if (answer.m_aNumbers.Count() < goods * NUMBERS_PER_GOOD)
		{
			LF_Debug.Error(FEATURE, string.Format("price board reply dropped: %1 goods need %2 numbers, got %3", goods, goods * NUMBERS_PER_GOOD, answer.m_aNumbers.Count()));
			goods = 0;
		}

		if (goods == 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-Market_NoPrices")));

		for (int i = 0; i < goods; i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Price" + i, PriceLine(answer.m_aTexts[i], answer.m_aNumbers[i * NUMBERS_PER_GOOD], answer.m_aNumbers[i * NUMBERS_PER_GOOD + 1])));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! "Copper Bar  38" or "Copper Bar  32 v" when the price is below its base.
	static string PriceLine(string name, int price, int belowBase)
	{
		string good = LF_WantedBoard.ResolveKey(name);
		if (belowBase != 0)
			return WidgetManager.Translate("#LF-Market_PriceDown", good, price.ToString(), ARROW_DOWN);

		return WidgetManager.Translate("#LF-Market_Price", good, price.ToString());
	}
}

//------------------------------------------------------------------------------------------------
class LF_PriceBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_PriceBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_PriceBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_PriceBoard.BuildAnswer(playerId);
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_PriceBoard.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_PriceBoard.FEATURE, string.Format("price card open: goods=%1", answer.m_aTexts.Count()));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_PriceBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read today's prices" on the chalk board. Local to the reader: it asks the server, which answers
//! with the book's prices now.
class LF_PriceBoardReadAction : ScriptedUserAction
{
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Market_Read";
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		// A server-driven use (the agent harness) answers that reader directly; the range rule applies.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			LF_BoardProviders.ServerRead(SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId)), LF_PriceBoard.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_PriceBoard.FEATURE, "price board read not sent: no SCR_PlayerController");
			return;
		}

		LF_WantedBoard.SetPendingBoard(pOwnerEntity.GetID());
		controller.LF_AskBoard(LF_PriceBoard.KIND);
	}
}
