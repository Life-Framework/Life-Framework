//------------------------------------------------------------------------------------------------
//! Why the barkeeper refused an under-the-counter sale. OK is the only result that moved anything.
enum LF_ECounterOutletSale
{
	OK,
	NO_BUYER,
	NOT_ALIVE,
	RESTRAINED,
	POLICE,
	TOO_FAR,
	TOO_FAST,
	NOTHING,
	FULL,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The under-the-counter outlet (#4200, 0.3.0 crime B5): a barkeeper who buys smuggled cartons and
//! bottles. Server only. The barkeeper is a premium buyer (LF_BuyerDemand): the pack's demand rows for
//! the outlet's buyer id say which goods, how many a period and at what price, and the shared purse book
//! (LF_BuyerDemandBook, saved) keeps the cap across sales and restarts. A seller hands over every good
//! the rows name, up to what each purse still takes; the rest stays with the seller. The pay is marked
//! notes when the dirty-money switch is on (LF_LaunderConfig), cash when it is off, like the illicit
//! buyer. An on-duty police officer is refused: the barkeeper does not sell to the law.
//!
//! All or nothing: the purses are spent first, the goods are taken, then the notes are paid; a short take
//! or a payout that does not land returns the goods, takes back what was paid and restores every purse.
class LF_CounterOutlet
{
	static const string FEATURE = "Laundering";
	static const string REASON_SALE = "counter-sale";
	static const string REASON_REFUND = "counter-sale-refund";

	//! Server: world seconds of each seller's last attempt, for the rate limit.
	protected static ref map<string, float> s_mLastSale = new map<string, float>();

	//------------------------------------------------------------------------------------------------
	//! Goods `user` carries that `buyer` has a demand row for: `goods[i]` x `counts[i]`.
	//! \return The number of units carried, 0 when there is nothing the barkeeper buys.
	static int Carried(IEntity user, string buyer, notnull array<ResourceName> goods, notnull array<int> counts)
	{
		goods.Clear();
		counts.Clear();
		LF_BuyerDemandConfig config = LF_BuyerDemandConfig.Get();
		if (!user || buyer.IsEmpty() || !config || !config.m_aRows)
			return 0;

		int total;
		foreach (LF_BuyerDemandRow row : config.m_aRows)
		{
			if (!row || row.m_sBuyer != buyer || goods.Contains(row.m_sGood))
				continue;

			int count = LF_InventoryUtils.GetAmount(user, row.m_sGood);
			if (count <= 0)
				continue;

			goods.Insert(row.m_sGood);
			counts.Insert(count);
			total += count;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` sells under the counter of `counter`. Checks, in order: a buyer id, a living seller,
	//! a living barkeeper (when the counter is a character), not cuffed, not on-duty police, in reach, not repeating faster than `minActionMs`, something the
	//! barkeeper buys, room left in a purse.
	//! \param counter The barkeeper (or the counter) the action sits on.
	//! \param buyer The demand-row buyer id of this outlet.
	//! \param onDutyPolice Whether `user` is an on-duty officer (the action reads it from the account).
	//! \param nowUnix Real time: the purse refill clock.
	//! \param nowSeconds World seconds: the rate limit.
	//! \param paid Receives the amount paid (0 on any refusal).
	//! \param secondsToNext Receives the seconds until a spent purse takes a lot again (FULL only, else 0).
	static LF_ECounterOutletSale ServerSell(IEntity user, IEntity counter, string buyer, float reach, int minActionMs, bool onDutyPolice, int nowUnix, float nowSeconds, out int paid, out int secondsToNext)
	{
		paid = 0;
		secondsToNext = 0;
		if (!Replication.IsServer() || !user || !counter)
			return LF_ECounterOutletSale.FAILED;

		if (buyer.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "counter sale refused: the outlet has no buyer id");
			return LF_ECounterOutletSale.NO_BUYER;
		}

		string key = LF_LaunderStationComponent.WorkerKey(user);
		if (key.IsEmpty())
			return LF_ECounterOutletSale.FAILED;

		if (!LF_Utils.IsAliveState(user) || LF_Downed.IsDowned(user))
			return LF_ECounterOutletSale.NOT_ALIVE;

		// A barkeeper who is a character must be alive and standing; a counter prop has no life state.
		if (ChimeraCharacter.Cast(counter) && (!LF_Utils.IsAliveState(counter) || LF_Downed.IsDowned(counter)))
		{
			LF_Debug.Info(FEATURE, string.Format("counter sale by %1 at '%2' refused: the barkeeper is down", key, buyer));
			return LF_ECounterOutletSale.NO_BUYER;
		}

		if (LF_CuffEnforcement.Refuse(user, "selling under the counter"))
			return LF_ECounterOutletSale.RESTRAINED;

		if (onDutyPolice)
		{
			LF_Debug.Info(FEATURE, string.Format("counter sale by %1 at '%2' refused: on-duty police", key, buyer));
			return LF_ECounterOutletSale.POLICE;
		}

		if (vector.Distance(user.GetOrigin(), counter.GetOrigin()) > reach)
			return LF_ECounterOutletSale.TOO_FAR;

		float last;
		if (s_mLastSale.Find(key, last) && (nowSeconds - last) * 1000 < minActionMs)
		{
			LF_Debug.Warn(FEATURE, string.Format("counter sale by %1 refused: asked again faster than %2 ms", key, minActionMs));
			return LF_ECounterOutletSale.TOO_FAST;
		}

		s_mLastSale.Set(key, nowSeconds);

		array<ResourceName> goods = {};
		array<int> counts = {};
		if (Carried(user, buyer, goods, counts) <= 0)
			return LF_ECounterOutletSale.NOTHING;

		// Spend each purse by what the seller carries; keep the state before so any failure puts it back.
		array<ResourceName> soldGoods = {};
		array<int> soldUnits = {};
		array<ref LF_BuyerPurse> before = {};
		int total;
		int wait = -1;
		foreach (int i, ResourceName good : goods)
		{
			LF_BuyerPurse snapshot = LF_BuyerDemandBook.Snapshot(buyer, good);
			LF_BuyerDemandSale sale = LF_BuyerDemandBook.Sell(buyer, good, counts[i], nowUnix);
			if (!sale || sale.m_iAccepted <= 0 || sale.m_iPayout <= 0)
			{
				// Nothing taken: a zero-pay row must not keep a spent purse.
				LF_BuyerDemandBook.Restore(buyer, good, snapshot);
				if (sale && sale.m_iSecondsToNext > 0 && (wait < 0 || sale.m_iSecondsToNext < wait))
					wait = sale.m_iSecondsToNext;

				continue;
			}

			soldGoods.Insert(good);
			soldUnits.Insert(sale.m_iAccepted);
			before.Insert(snapshot);
			total += sale.m_iPayout;
		}

		if (soldGoods.IsEmpty() || total <= 0)
		{
			secondsToNext = LF_Utils.MaxInt(wait, 0);
			LF_Debug.Info(FEATURE, string.Format("counter sale by %1 at '%2' refused: the barkeeper has enough (back in %3 s)", key, buyer, secondsToNext));
			return LF_ECounterOutletSale.FULL;
		}

		// Take every line. A short take puts back what was taken and every purse.
		array<int> taken = {};
		bool whole = true;
		foreach (int i, ResourceName good : soldGoods)
		{
			int removed = LF_InventoryUtils.RemoveAmount(user, good, soldUnits[i]);
			taken.Insert(removed);
			if (removed != soldUnits[i])
			{
				whole = false;
				break;
			}
		}

		if (!whole)
		{
			RollBack(user, buyer, soldGoods, taken, before);
			LF_Debug.Error(FEATURE, string.Format("counter sale by %1 failed: the goods could not all be taken; put them back", key));
			return LF_ECounterOutletSale.FAILED;
		}

		bool marked = LF_LaunderConfig.CrimePaysMarked();
		int got;
		if (marked)
			got = LF_MarkedCash.Add(user, total, REASON_SALE, true);
		else
			got = LF_MoneyUtils.AddCash(user, total, REASON_SALE, "", true);

		if (got != total)
		{
			if (got > 0)
			{
				if (marked)
					LF_MarkedCash.Take(user, got, REASON_REFUND);
				else
					LF_MoneyUtils.RemoveCash(user, got, REASON_REFUND);
			}

			RollBack(user, buyer, soldGoods, taken, before);
			LF_Debug.Error(FEATURE, string.Format("counter sale by %1 failed: paid %2 of %3 (marked=%4); goods returned", key, got, total, marked));
			return LF_ECounterOutletSale.FAILED;
		}

		// Cash is on the ledger already (AddCash); marked notes get their own trail line.
		if (marked)
			LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(key, LF_Ledger.ACCOUNT_MARKED, total, true, REASON_SALE, LF_MarkedCash.Count(user), ""));

		paid = total;
		LF_Debug.Info(FEATURE, string.Format("counter sale by %1 at '%2': %3 line(s) for %4 (%5)", key, buyer, soldGoods.Count(), total, LF_BuyerSale.Currency(marked)));
		return LF_ECounterOutletSale.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RollBack(IEntity user, string buyer, notnull array<ResourceName> goods, notnull array<int> taken, notnull array<ref LF_BuyerPurse> before)
	{
		foreach (int i, int count : taken)
		{
			if (count > 0)
				LF_InventoryUtils.AddAmount(user, goods[i], count, true);
		}

		foreach (int i, ResourceName good : goods)
		{
			LF_BuyerDemandBook.Restore(buyer, good, before[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key telling the seller why, or empty for a result that needs no words.
	static string RefusalKey(LF_ECounterOutletSale result)
	{
		switch (result)
		{
			case LF_ECounterOutletSale.NO_BUYER: return "#LF-CounterOutlet_Nothing";
			case LF_ECounterOutletSale.POLICE: return "#LF-CounterOutlet_Police";
			case LF_ECounterOutletSale.TOO_FAR: return "#LF-Chain_TooFar";
			case LF_ECounterOutletSale.NOTHING: return "#LF-CounterOutlet_Nothing";
			case LF_ECounterOutletSale.FULL: return "#LF-CounterOutlet_Full";
			case LF_ECounterOutletSale.FAILED: return "#LF-Chain_Failed";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastSale.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CounterOutlet's statics (LF_StaticReset finds it by reflection).
class LF_CounterOutlet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CounterOutlet";
	}

	override void Apply()
	{
		LF_CounterOutlet.ResetStatics();
	}
}
