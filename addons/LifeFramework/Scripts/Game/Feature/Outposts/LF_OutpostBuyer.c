//------------------------------------------------------------------------------------------------
//! The outpost's contraband buyer (0.3.0 gangs O5). The rebels take contraband crates for 10 to 20%
//! more than the fence pays (the convoy drop, LF_Contraband.PAYOUT), but only up to their demand: a
//! buyer-demand purse (LF_BuyerDemand, CR-B1) per outpost, "outpost:<id>" plus the crate prefab. A full
//! purse refuses in words and says when it wants more.
//!
//! The purse lives in LF_BuyerDemandBook, so it is saved in the "buyerdemand" section with the other
//! buyers. A pack may give the buyer its own row in LF_BuyerDemand.conf (same buyer id and crate
//! prefab); without one the built-in row below applies, priced from the fence.
//!
//! Order on the server (Sell): the officer refusal (LF_Outpost.RefusesService, told by NotifyRefusal),
//! the crate carried, the seller's convoy run, the purse, cash first, then the crate, then the run is
//! closed (one crate per run); a failure puts the cash, the crate and the purse back. A completed sale books the outpost standing (LF_OutpostStanding.OnSale).
//------------------------------------------------------------------------------------------------
enum LF_EOutpostSell
{
	SOLD,
	BAD_INPUT,
	REFUSED_OFFICER,
	NOTHING_CARRIED,
	NO_RUN,
	BUYER_FULL,
	UNPAYABLE,
	NOT_REMOVED
}

//------------------------------------------------------------------------------------------------
class LF_OutpostBuyer
{
	static const string FEATURE = "Outposts";
	static const string REASON_SELL = "outpost-sell";
	static const string BUYER_PREFIX = "outpost:";
	static const string TITLE_KEY = "#LF-OutpostBuyer_Title";

	//! The premium over the fence, percent: the plan's band is 10 to 20.
	static const int MIN_PREMIUM_PCT = 10;	// lf-fixed: the plan band (0.3.0 GA-O5), a rule not a tunable
	static const int MAX_PREMIUM_PCT = 20;	// lf-fixed: the plan band (0.3.0 GA-O5), a rule not a tunable
	static const int DEFAULT_PREMIUM_PCT = 15;	// lf-fixed: fallback; a pack row in LF_BuyerDemand.conf sets its own price

	//! The built-in demand: six crates when the purse is full, one more each real hour.
	static const int DEFAULT_CAP = 6;	// lf-fixed: fallback; a pack row in LF_BuyerDemand.conf sets its own cap
	static const int DEFAULT_REFILL_PER_HOUR = 1;	// lf-fixed: fallback; a pack row in LF_BuyerDemand.conf sets its own refill

	//------------------------------------------------------------------------------------------------
	//! \return The fence's price for one crate: what the convoy drop pays.
	static int FencePrice()
	{
		return LF_Contraband.PAYOUT;
	}

	//------------------------------------------------------------------------------------------------
	//! The pure price rule: the fence price plus a premium held to 10..20 percent, rounded down to
	//! whole dollars but always at least one dollar above a positive fence price.
	static int PriceAboveFence(int fencePrice, int premiumPct)
	{
		if (fencePrice <= 0)
			return 0;

		int pct = LF_Utils.ClampInt(premiumPct, MIN_PREMIUM_PCT, MAX_PREMIUM_PCT);
		// fencePrice is bounded by LF_PriceCurve.MAX_CASH below; the product stays inside an int.
		int bounded = LF_Utils.MinInt(fencePrice, LF_PriceCurve.MAX_CASH / (100 + MAX_PREMIUM_PCT));
		int price = (bounded * (100 + pct)) / 100;
		return LF_Utils.MaxInt(price, bounded + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The buyer id of an outpost (the demand book's key half).
	static string BuyerId(string outpostId)
	{
		return BUYER_PREFIX + outpostId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The built-in demand row for an outpost: the crate at the fence price plus the default
	//! premium, the default cap and refill.
	static LF_BuyerDemandRow DefaultRow(string outpostId)
	{
		return LF_BuyerDemandRow.Make(BuyerId(outpostId), LF_Contraband.ITEM, DEFAULT_CAP, DEFAULT_REFILL_PER_HOUR, PriceAboveFence(FencePrice(), DEFAULT_PREMIUM_PCT));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The outpost's row: the config's when it has one, else DefaultRow.
	static LF_BuyerDemandRow RowFor(string outpostId)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandBook.FindRow(BuyerId(outpostId), LF_Contraband.ITEM);
		if (row && row.Problem().IsEmpty() && row.Lot() == 1)
			return row;

		// One run sells one crate, so a row priced per lot of several can never sell: use the built-in row.
		if (row)
			LF_Debug.Error(FEATURE, string.Format("outpost buyer row %1 is unusable (%2, lot %3); using the built-in row", row.Label(), row.Problem(), row.Lot()));

		return DefaultRow(outpostId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the outpost's purse would pay for `units` crates at `nowUnix`, booking nothing.
	static int Quote(string outpostId, int units, int nowUnix)
	{
		LF_BuyerDemandRow row = RowFor(outpostId);
		LF_BuyerPurse purse = LF_BuyerDemandBook.Snapshot(row.m_sBuyer, row.m_sGood);
		if (!purse)
			purse = LF_BuyerPurse.Full(row, nowUnix);

		return LF_BuyerDemand.Sell(row, purse, units, nowUnix, LF_BuyerDemandConfig.CurveFor(row)).m_iPayout;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Server: `seller` sells one contraband crate to the buyer of outpost `outpostId`, whose table stands
	//! at `traderPos`. The outpost is the other end of a convoy run: the sale closes the seller's running,
	//! driven run (LF_Contraband.Deliver, which marks it paid and starts its cooldown) instead of the drop,
	//! so one run sells one crate and a crate handed out at the barn without a run fetches nothing. The
	//! purse takes it only while it has demand left.
	//! \param[out] sold Crates taken.
	//! \param[out] paid Dollars paid.
	static LF_EOutpostSell Sell(IEntity seller, string outpostId, vector traderPos, int nowUnix, out int sold, out int paid)
	{
		sold = 0;
		paid = 0;
		if (Replication.IsRunning() && !Replication.IsServer())
			return LF_EOutpostSell.BAD_INPUT;

		if (!seller || outpostId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("outpost crate sale refused: seller %1 outpost '%2'", seller != null, outpostId));
			Tell(seller, "#LF-OutpostBuyer_Unavailable");
			return LF_EOutpostSell.BAD_INPUT;
		}

		if (LF_Outpost.RefusesService(seller, traderPos))
		{
			LF_Outpost.NotifyRefusal(seller, traderPos);
			return LF_EOutpostSell.REFUSED_OFFICER;
		}

		int carried = LF_Contraband.CountCarried(seller);
		if (carried <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("outpost %1 crate sale refused: no crates carried", outpostId));
			Tell(seller, "#LF-OutpostBuyer_NoCrate");
			return LF_EOutpostSell.NOTHING_CARRIED;
		}

		string characterId = LF_Hideout.CharacterIdOf(seller);
		if (characterId.IsEmpty() || !LF_Contraband.HasRun(characterId))
		{
			LF_Debug.Info(FEATURE, string.Format("outpost %1 crate sale refused (NO_RUN): '%2' hauls no convoy run", outpostId, characterId));
			Tell(seller, "#LF-OutpostBuyer_NoRun");
			return LF_EOutpostSell.NO_RUN;
		}

		LF_BuyerDemandRow row = RowFor(outpostId);
		LF_BuyerPurse before = LF_BuyerDemandBook.Snapshot(row.m_sBuyer, row.m_sGood);
		LF_BuyerPurse purse = LF_BuyerDemandBook.Snapshot(row.m_sBuyer, row.m_sGood);
		if (!purse)
			purse = LF_BuyerPurse.Full(row, nowUnix);

		// One crate per run: the run, not the crate count, is what the convoy pays for.
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 1, nowUnix, LF_BuyerDemandConfig.CurveFor(row));
		if (sale.m_iAccepted <= 0 || sale.m_iPayout <= 0)
		{
			// Keep the refill the sale applied, so the clock moves on; nothing was taken.
			LF_BuyerDemandBook.Restore(row.m_sBuyer, row.m_sGood, purse);
			NotifyFull(seller, sale.m_iSecondsToNext);
			LF_Debug.Info(FEATURE, string.Format("outpost %1 crate sale refused (BUYER_FULL): back in %2 s", outpostId, sale.m_iSecondsToNext));
			return LF_EOutpostSell.BUYER_FULL;
		}

		LF_BuyerDemandBook.Restore(row.m_sBuyer, row.m_sGood, purse);
		// Cash is an item: what does not fit lands at the seller's feet rather than failing the sale.
		int added = LF_MoneyUtils.AddCash(seller, sale.m_iPayout, REASON_SELL, outpostId, true);
		if (added != sale.m_iPayout)
		{
			if (added > 0)
				LF_MoneyUtils.RemoveCash(seller, added, REASON_SELL + "-rollback");

			LF_BuyerDemandBook.Restore(row.m_sBuyer, row.m_sGood, before);
			LF_Debug.Error(FEATURE, string.Format("outpost %1 crate payout failed (%2 of %3), purse restored", outpostId, added, sale.m_iPayout));
			Tell(seller, "#LF-OutpostBuyer_Unavailable");
			return LF_EOutpostSell.UNPAYABLE;
		}

		int removed = LF_InventoryUtils.RemoveAmount(seller, LF_Contraband.ITEM, sale.m_iAccepted);
		if (removed != sale.m_iAccepted)
		{
			RollBack(seller, added, removed, row, before);
			LF_Debug.Error(FEATURE, string.Format("outpost %1 crate sale: took %2 of %3 crates, rolled back", outpostId, removed, sale.m_iAccepted));
			Tell(seller, "#LF-OutpostBuyer_Unavailable");
			return LF_EOutpostSell.NOT_REMOVED;
		}

		// Close the run last (payTo null: the drop's own cash and crate removal are skipped). A run that is
		// not running, was never driven, or whose hauler left the gang that holds the hideout pays nothing.
		if (LF_Contraband.Deliver(characterId, removed, null) <= 0)
		{
			RollBack(seller, added, removed, row, before);
			LF_Debug.Info(FEATURE, string.Format("outpost %1 crate sale refused (NO_RUN): %2", outpostId, LF_Contraband.GetLastRefusal()));
			Tell(seller, "#LF-OutpostBuyer_NoRun");
			return LF_EOutpostSell.NO_RUN;
		}

		sold = sale.m_iAccepted;
		paid = sale.m_iPayout;
		LF_Debug.Info(FEATURE, string.Format("outpost %1 bought %2 crate(s) from %3 for %4 (purse %5/%6)", outpostId, sold, characterId, paid, purse.m_iRemaining, row.m_iCap));
		LF_OutpostStanding.OnSale(seller, traderPos);
		Tell(seller, "#LF-OutpostBuyer_Sold", LF_MoneyFormat.FormatMoney(paid));
		return LF_EOutpostSell.SOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts back a sale that failed after the payout: the cash, the crates taken and the purse.
	protected static void RollBack(IEntity seller, int cash, int crates, notnull LF_BuyerDemandRow row, LF_BuyerPurse before)
	{
		if (cash > 0)
			LF_MoneyUtils.RemoveCash(seller, cash, REASON_SELL + "-rollback");

		if (crates > 0)
			LF_InventoryUtils.AddAmount(seller, LF_Contraband.ITEM, crates, true);

		LF_BuyerDemandBook.Restore(row.m_sBuyer, row.m_sGood, before);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tell(IEntity seller, string message, string arg = "")
	{
		LF_Utils.NotifyEntity(seller, message, TITLE_KEY, 5.0, arg);
	}


	//------------------------------------------------------------------------------------------------
	//! The buyer has enough, in words, and roughly when it wants more.
	protected static void NotifyFull(IEntity seller, int secondsToNext)
	{
		if (secondsToNext < 0)
		{
			LF_Utils.NotifyEntity(seller, "#LF-OutpostBuyer_FullNever", TITLE_KEY, 5.0);
			return;
		}

		int minutes = LF_Utils.MaxInt((secondsToNext + 59) / 60, 1);
		LF_Utils.NotifyEntity(seller, "#LF-OutpostBuyer_Full", TITLE_KEY, 5.0, minutes.ToString());
	}
}

//------------------------------------------------------------------------------------------------
//! The buyer's Sell row on its table: "Sell contraband crate ($230)". Shown while the user
//! carries a crate; the server decides everything in LF_OutpostBuyer.Sell. The outpost is the one whose
//! zone holds the table (LF_OutpostStanding.OutpostAt), so the action needs no id of its own.
class LF_OutpostBuyerSellAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Contraband.CountCarried(user) > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity owner = GetOwner();
		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		if (!owner || !user)
			return false;

		string outpostId = LF_OutpostStanding.OutpostAt(owner.GetOrigin());
		int price = LF_OutpostBuyer.PriceAboveFence(LF_OutpostBuyer.FencePrice(), LF_OutpostBuyer.DEFAULT_PREMIUM_PCT);
		if (!outpostId.IsEmpty())
			price = LF_OutpostBuyer.RowFor(outpostId).m_iPrice;

		outName = WidgetManager.Translate("#LF-OutpostBuyer_Sell", LF_MoneyFormat.FormatMoney(price));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		string outpostId = LF_OutpostStanding.OutpostAt(pOwnerEntity.GetOrigin());
		if (outpostId.IsEmpty())
		{
			LF_Debug.Error(LF_OutpostBuyer.FEATURE, "outpost crate buyer stands outside every outpost; sale refused");
			LF_Utils.NotifyEntity(pUserEntity, "#LF-OutpostBuyer_Unavailable", LF_OutpostBuyer.TITLE_KEY, 5.0);
			return;
		}

		int sold;
		int paid;
		LF_OutpostBuyer.Sell(pUserEntity, outpostId, pOwnerEntity.GetOrigin(), System.GetUnixTime(), sold, paid);
	}
}
