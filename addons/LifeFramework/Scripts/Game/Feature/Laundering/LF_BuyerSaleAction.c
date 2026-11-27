//------------------------------------------------------------------------------------------------
//! Why the buyer refused a sale. OK is the only result that moved anything.
enum LF_EBuyerSale
{
	OK,
	NO_CONFIG,
	ABSENT,
	CLOSED,
	TOO_FAR,
	NOT_ALIVE,
	RESTRAINED,
	TOO_FAST,
	NOTHING,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! Selling chain goods to the illicit buyer (#2920, M7 beat 4 of #1025). Server only. A character who
//! stands in reach of the live buyer at the open spot hands over every chain good the price list names;
//! the buyer pays the sum in marked notes when the dirty-money switch is on (LF_LaunderConfig) and in
//! cash when it is off. All or nothing on the inventory side: the goods are taken first, and a short
//! take or a payout that does not land puts every unit back and takes back what was paid.
class LF_BuyerSale
{
	static const string FEATURE = "Laundering";
	static const string REASON_SALE = "buyer-sale";
	static const string REASON_REFUND = "buyer-sale-refund";

	//! Server: world seconds of each seller's last sale attempt, for the rate limit.
	protected static ref map<string, float> s_mLastSale;
	protected static map<string, float> LzLastSale()
	{
		if (!s_mLastSale)
			s_mLastSale = new map<string, float>();
		return s_mLastSale;
	}

	//------------------------------------------------------------------------------------------------
	//! Chain goods `user` carries that the price list names: `prefabs[i]` x `counts[i]` at `prices[i]`.
	//! \return The total value, 0 when there is nothing to sell.
	static int Carried(IEntity user, notnull array<ResourceName> prefabs, notnull array<int> counts, notnull array<int> prices)
	{
		prefabs.Clear();
		counts.Clear();
		prices.Clear();
		LF_BuyerPrices config = LF_BuyerPrices.Get();
		if (!user || !config)
			return 0;

		int total;
		foreach (LF_BuyerPriceRow row : config.m_aPrices)
		{
			int bulk;
			if (!row || !LF_ChainSearch.IsGood(row.m_Prefab, bulk))
				continue;

			int count = LF_InventoryUtils.GetAmount(user, row.m_Prefab);
			if (count <= 0)
				continue;

			prefabs.Insert(row.m_Prefab);
			counts.Insert(count);
			prices.Insert(row.m_iPrice);
			total += count * row.m_iPrice;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` sells everything the buyer takes. Checks, in order: a living standing seller, not
	//! cuffed, the buyer is the live one and stands at the spot the rotation has open at `unixNow`, in
	//! reach, not repeating faster than the config allows, something to sell.
	//! \param buyer The character the action sits on.
	//! \param unixNow Real time; picks the open spot.
	//! \param nowSeconds World seconds; the rate limit.
	//! \param paid Receives the amount paid (0 on any refusal).
	static LF_EBuyerSale ServerSell(IEntity user, IEntity buyer, int unixNow, float nowSeconds, out int paid)
	{
		paid = 0;
		if (!Replication.IsServer() || !user)
			return LF_EBuyerSale.FAILED;

		LF_BuyerPrices config = LF_BuyerPrices.Get();
		if (!config)
			return LF_EBuyerSale.NO_CONFIG;

		string key = LF_LaunderStationComponent.WorkerKey(user);
		if (key.IsEmpty())
			return LF_EBuyerSale.FAILED;

		if (!LF_Utils.IsAliveState(user) || LF_Downed.IsDowned(user))
			return LF_EBuyerSale.NOT_ALIVE;

		// Cuffed: refused like robbery and laundering (LF_CuffEnforcement tells the player why).
		if (LF_CuffEnforcement.Refuse(user, "selling to the buyer"))
			return LF_EBuyerSale.RESTRAINED;

		if (!buyer || LF_BuyerNpc.GetLive() != buyer)
			return LF_EBuyerSale.ABSENT;

		string open = LF_BuyerRotation.OpenSpot(unixNow);
		if (open.IsEmpty() || open != LF_BuyerNpc.LiveSpot())
		{
			LF_Debug.Info(FEATURE, string.Format("buyer sale by %1 refused: he stands at '%2' but '%3' is open", key, LF_BuyerNpc.LiveSpot(), open));
			return LF_EBuyerSale.CLOSED;
		}

		if (vector.Distance(user.GetOrigin(), buyer.GetOrigin()) > config.m_fReach)
			return LF_EBuyerSale.TOO_FAR;

		// Find leaves `last` at 0 on a miss, so a first sale early in the world must not read as a repeat.
		float last;
		LzLastSale();
		if (s_mLastSale.Find(key, last) && (nowSeconds - last) * 1000 < config.m_iMinActionMs)
		{
			LF_Debug.Warn(FEATURE, string.Format("buyer sale by %1 refused: asked again faster than %2 ms", key, config.m_iMinActionMs));
			return LF_EBuyerSale.TOO_FAST;
		}

		LzLastSale();
		s_mLastSale.Set(key, nowSeconds);

		array<ResourceName> prefabs = {};
		array<int> counts = {};
		array<int> prices = {};
		int total = Carried(user, prefabs, counts, prices);
		if (total <= 0)
			return LF_EBuyerSale.NOTHING;

		// Take every line first. A short take puts back what was taken and nothing else changes.
		array<int> taken = {};
		bool whole = true;
		foreach (int i, ResourceName prefab : prefabs)
		{
			int removed = LF_InventoryUtils.RemoveAmount(user, prefab, counts[i]);
			taken.Insert(removed);
			if (removed != counts[i])
			{
				whole = false;
				break;
			}
		}

		if (!whole)
		{
			GiveBack(user, prefabs, taken);
			LF_Debug.Error(FEATURE, string.Format("buyer sale by %1 failed: the goods could not all be taken; put them back", key));
			return LF_EBuyerSale.FAILED;
		}

		bool marked = LF_LaunderConfig.CrimePaysMarked();
		int got;
		if (marked)
			got = LF_MarkedCash.Add(user, total, REASON_SALE, true);
		else
			got = LF_MoneyUtils.AddCash(user, total, REASON_SALE, "", true);

		if (got != total)
		{
			// A payout that did not land: take back what did, return every unit.
			if (got > 0)
			{
				if (marked)
					LF_MarkedCash.Take(user, got, REASON_REFUND);
				else
					LF_MoneyUtils.RemoveCash(user, got, REASON_REFUND);
			}

			GiveBack(user, prefabs, taken);
			LF_Debug.Error(FEATURE, string.Format("buyer sale by %1 failed: paid %2 of %3 (%4); goods returned", key, got, total, marked));
			return LF_EBuyerSale.FAILED;
		}

		// Cash is on the ledger already (AddCash); marked notes get their own trail line, like the station's.
		if (marked)
			LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(key, LF_Ledger.ACCOUNT_MARKED, total, true, REASON_SALE, LF_MarkedCash.Count(user), ""));

		paid = total;
		LF_Debug.Info(FEATURE, string.Format("buyer sale by %1: %2 line(s) for %3 (%4)", key, prefabs.Count(), total, LF_BuyerSale.Currency(marked)));

		// A sale can close a quest ("sell the sample"): the quests hear which goods were paid for (#3768).
		LF_QuestSignals.ReportBuyerSale(user, prefabs);
		return LF_EBuyerSale.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveBack(IEntity user, notnull array<ResourceName> prefabs, notnull array<int> taken)
	{
		foreach (int i, int count : taken)
		{
			if (count > 0)
				LF_InventoryUtils.AddAmount(user, prefabs[i], count, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string Currency(bool marked)
	{
		if (marked)
			return "marked notes";

		return "cash";
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key telling the character why, or empty for a result that needs no words.
	static string RefusalKey(LF_EBuyerSale result)
	{
		switch (result)
		{
			case LF_EBuyerSale.NO_CONFIG: return "#LF-BuyerSale_Closed";
			case LF_EBuyerSale.ABSENT: return "#LF-BuyerSale_Closed";
			case LF_EBuyerSale.CLOSED: return "#LF-BuyerSale_Closed";
			case LF_EBuyerSale.TOO_FAR: return "#LF-Chain_TooFar";
			case LF_EBuyerSale.NOTHING: return "#LF-BuyerSale_Nothing";
			case LF_EBuyerSale.FAILED: return "#LF-Chain_Failed";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LzLastSale();
		s_mLastSale.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuyerSale's statics (LF_StaticReset finds it by reflection).
class LF_BuyerSale_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerSale";
	}

	override void Apply()
	{
		LF_BuyerSale.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! "Sell the goods" on the illicit buyer. The offer is a client hint; the server decides everything
//! again in LF_BuyerSale.ServerSell and says a refusal in words.
class LF_BuyerSaleAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		SetCannotPerformReason("#LF-BuyerSale_Nothing");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client hint only: selling needs a chain good the price list names.
	override bool CanBePerformedScript(IEntity user)
	{
		array<ResourceName> prefabs = {};
		array<int> counts = {};
		array<int> prices = {};
		return LF_BuyerSale.Carried(user, prefabs, counts, prices) > 0;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_BuyerSale.FEATURE, "buyer sale performed without a buyer or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		int paid;
		LF_EBuyerSale result = LF_BuyerSale.ServerSell(pUserEntity, pOwnerEntity, System.GetUnixTime(), now, paid);
		if (result == LF_EBuyerSale.OK)
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-BuyerSale_Sold", "#LF-BuyerSale_Title", 4.0, LF_MoneyFormat.FormatMoney(paid));
			return;
		}

		LF_Debug.Info(LF_BuyerSale.FEATURE, string.Format("buyer sale refused (%1)", typename.EnumToString(LF_EBuyerSale, result)));
		string key = LF_BuyerSale.RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-BuyerSale_Title", 4.0);
	}
}
