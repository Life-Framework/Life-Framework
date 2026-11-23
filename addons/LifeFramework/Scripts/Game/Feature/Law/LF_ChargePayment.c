//------------------------------------------------------------------------------------------------
//! What one pay-now attempt at the booking desk came to.
enum LF_EChargePay
{
	PAID,
	//! Cash plus bank does not reach the fine: nothing was taken.
	SHORT,
	//! No fine to pay (already paid, jail only, or zero).
	NOT_PAYABLE,
	//! A wallet gave back less than it showed; whatever moved was put back.
	FAILED_TAKE
}

//------------------------------------------------------------------------------------------------
//! A bank account seen as a wallet, so the cash-then-bank rule runs the same in a LOGIC check.
class LF_ChargeBankWallet : LF_CashWallet
{
	protected ref LF_BankAccount m_Account;
	protected string m_sReason;

	//------------------------------------------------------------------------------------------------
	void LF_ChargeBankWallet(LF_BankAccount account, string reason)
	{
		m_Account = account;
		m_sReason = reason;
	}

	//------------------------------------------------------------------------------------------------
	override int Carried()
	{
		if (!m_Account)
			return 0;

		return LF_Utils.MaxInt(m_Account.GetBalance(), 0);
	}

	//------------------------------------------------------------------------------------------------
	override int Take(int amount)
	{
		if (!m_Account || amount <= 0 || !m_Account.Withdraw(amount, m_sReason))
			return 0;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	override int Put(int amount)
	{
		if (!m_Account || amount <= 0)
			return 0;

		m_Account.Deposit(amount, m_sReason);
		return amount;
	}
}

//------------------------------------------------------------------------------------------------
//! Pay now at the booking desk (#4427, police L6). The suspect pays the offer's fine from carried cash
//! first, then the bank, all or nothing. The money goes to the town treasury (plan disagreement 11):
//! there is no treasury store yet, so it leaves the game as a sink logged `treasury +$X`. The booking
//! officer's cash and bank are never touched. A paid suspect is uncuffed and their charges cleared; a
//! suspect who cannot cover the fine goes to the cell for the offer's time instead.
class LF_ChargePayment
{
	static const string FEATURE = "Law";
	//! Ledger reason on the suspect's cash and bank lines.
	static const string REASON = "fine-treasury";
	//! A refused fine-only charge costs one minute in the cell per this many dollars (the plan's property
	//! row: $200 to $600 or 5 to 10 minutes), at least one minute, at most the jail's 15-minute cap.
	static const int DOLLARS_PER_CELL_MINUTE = 60;	// lf-fixed: the plan's fines-as-minutes table, not a server tunable

	//------------------------------------------------------------------------------------------------
	//! Pure: the cell time a fine-only charge costs when the suspect takes the time instead of paying.
	static int RefusedFineSeconds(int fine)
	{
		if (fine <= 0)
			return 0;

		int minutes = (fine + DOLLARS_PER_CELL_MINUTE - 1) / DOLLARS_PER_CELL_MINUTE;
		int seconds = LF_Utils.MaxInt(minutes, 1) * 60;
		return LF_Utils.MinInt(seconds, LF_JailCellComponent.MAX_SENTENCE_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes exactly `amount`, cash first and the bank for the rest, or nothing at all.
	static LF_EChargePay Take(int amount, notnull LF_CashWallet cash, notnull LF_CashWallet bank, out int outFromCash, out int outFromBank)
	{
		outFromCash = 0;
		outFromBank = 0;
		if (amount <= 0)
			return LF_EChargePay.NOT_PAYABLE;

		int carried = LF_Utils.MaxInt(cash.Carried(), 0);
		int banked = LF_Utils.MaxInt(bank.Carried(), 0);
		int wantCash = LF_Utils.MinInt(amount, carried);
		int wantBank = amount - wantCash;
		if (wantBank > banked)
			return LF_EChargePay.SHORT;

		if (wantCash > 0)
		{
			int tookCash = cash.Take(wantCash);
			if (tookCash != wantCash)
			{
				if (tookCash > 0)
					cash.Put(tookCash);

				LF_Debug.Error(FEATURE, string.Format("pay now: took $%1 of $%2 cash; put back", tookCash, wantCash));
				return LF_EChargePay.FAILED_TAKE;
			}
		}

		if (wantBank > 0)
		{
			int tookBank = bank.Take(wantBank);
			if (tookBank != wantBank)
			{
				if (tookBank > 0)
					bank.Put(tookBank);

				if (wantCash > 0)
					cash.Put(wantCash);

				LF_Debug.Error(FEATURE, string.Format("pay now: took $%1 of $%2 from the bank; cash and bank put back", tookBank, wantBank));
				return LF_EChargePay.FAILED_TAKE;
			}
		}

		outFromCash = wantCash;
		outFromBank = wantBank;
		return LF_EChargePay.PAID;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_BookingDesk.PayNow: settle a payable offer. `cash` and `bank` default to the
	//! suspect's carried cash and their bank account. \return "" when the offer is settled (paid and
	//! free, or short and in the cell), else the key the suspect reads; the offer then stays open.
	static string Settle(notnull LF_BookingOffer offer, IEntity suspect, LF_CashWallet cash = null, LF_CashWallet bank = null)
	{
		if (!Replication.IsServer())
			return "#LF-BookingDesk_NoOffer";

		string uid = offer.m_sSuspectUid;
		if (!offer.m_bPayable || offer.m_iFine <= 0 || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("pay now refused: %1 has nothing to pay", uid));
			return "#LF-BookingDesk_NoOffer";
		}

		ref LF_CashWallet pocket = cash;
		if (!pocket)
			pocket = new LF_EntityCashWallet(suspect, REASON);

		ref LF_CashWallet account = bank;
		if (!account)
			account = new LF_ChargeBankWallet(LF_ATMManager.GetOrCreate(uid), REASON);

		int fine = offer.m_iFine;
		int fromCash;
		int fromBank;
		LF_EChargePay result = Take(fine, pocket, account, fromCash, fromBank);
		if (result == LF_EChargePay.SHORT)
			return ToCell(offer, suspect);

		if (result != LF_EChargePay.PAID)
		{
			LF_Debug.Warn(FEATURE, string.Format("pay now: %1 could not pay $%2 (result %3), offer stays open", uid, fine, typename.EnumToString(LF_EChargePay, result)));
			return "#LF-ChargePayment_Failed";
		}

		// Spent: a second click on the same offer finds nothing to pay.
		offer.m_bPayable = false;
		offer.m_iFine = 0;
		LF_Debug.Info(FEATURE, string.Format("treasury +$%1 (fine from %2: cash $%3, bank $%4; booked by %5)", fine, uid, fromCash, fromBank, offer.m_sOfficerUid));
		Free(offer, suspect);
		Notify(suspect, "#LF-ChargePayment_Paid", LF_ENotificationType.SUCCESS, fine.ToString());
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Paid: the charges are settled (wanted level 0 takes the reports and posters down) and the cuffs
	//! come off and go back to the booking officer.
	protected static void Free(notnull LF_BookingOffer offer, IEntity suspect)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetAccount(offer.m_sSuspectUid);

		if (account)
			account.SetWantedLevel(0);
		else
			LF_Debug.Error(FEATURE, string.Format("pay now: %1 paid but has no account to clear", offer.m_sSuspectUid));

		// Out of custody like a cell would end it: the escort hold goes (else a paid suspect stays "held",
		// barred from driving, and a logout counts as leaving custody), then the cuffs.
		LF_JailEscort.Drop(offer.m_sSuspectUid);
		LF_Restraint.OnJailed(offer.m_sSuspectUid, offer.m_sOfficerUid, suspect);
		LF_Debug.Info(FEATURE, string.Format("pay now: %1 paid and walks free", offer.m_sSuspectUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Short of money: the cell for the offer's time. The booking officer books them through the jail's
	//! own path when still at hand; otherwise the jail takes them on the officer's booking.
	protected static string ToCell(notnull LF_BookingOffer offer, IEntity suspect)
	{
		IEntity officer;
		if (offer.m_OfficerId != EntityID.INVALID && GetGame().GetWorld())
			officer = GetGame().GetWorld().FindEntityByID(offer.m_OfficerId);

		bool booked;
		if (officer && suspect && LF_Utils.GetPlayerUid(officer) == offer.m_sOfficerUid)
			booked = LF_JailBooking.Book(officer, suspect, offer.m_iPercent);
		else
			booked = LF_Jail.Arrest(offer.m_sOfficerUid, offer.m_sSuspectUid, suspect, offer.m_iPercent);

		if (!booked)
		{
			LF_Debug.Warn(FEATURE, string.Format("pay now: %1 is short of $%2 and the jail refused them, offer stays open", offer.m_sSuspectUid, offer.m_iFine));
			return "#LF-JailBook_Failed";
		}

		LF_Debug.Info(FEATURE, string.Format("pay now: %1 is short of $%2, to the cell for %3 s", offer.m_sSuspectUid, offer.m_iFine, offer.m_iServeSeconds));
		Notify(suspect, "#LF-ChargePayment_Short", LF_ENotificationType.WARNING, offer.m_iFine.ToString());
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Notify(IEntity suspect, string key, LF_ENotificationType type, string arg)
	{
		int playerId = LF_PlayerInteraction.PlayerIdOf(suspect);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-BookingDesk_Title", key, 6.0, type, arg);
	}
}
