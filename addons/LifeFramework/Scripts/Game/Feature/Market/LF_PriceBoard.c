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
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		LF_BoardAnswer answer = BuildAnswerAt(System.GetUnixTime());
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
