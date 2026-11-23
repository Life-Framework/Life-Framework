// red-proof: RED-OK law/charge-payment (fails with the hook on, 52 assertions), observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_ChargePayment`: with the payments skipped, `cash plus bank: taken from the bank (expected 200, got 0)`, `a second click takes nothing (expected 350, got 175)`, `a short suspect goes to the cell` fail.
//! law/charge-payment (#4427, police L6): pay now at the booking desk takes exactly the fine once, cash
//! first and the bank for the rest (cash only, cash plus bank, bank only); a suspect short of money is
//! jailed for the offer's time instead; the booking officer's bank never changes; a second click on a
//! paid offer pays nothing; a refused fine-only charge costs cell time by the fine, capped at 15 minutes.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ChargePayment : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-pay-officer";
	protected static const string SUSPECT_UID = "lf-test-pay-suspect";

	override string GetName()
	{
		return "law/charge-payment";
	}

	//------------------------------------------------------------------------------------------------
	//! Two catalog codes that carry a fine and are not jail only, so "pay now" is offered.
	protected bool PickCodes(notnull array<string> outIds)
	{
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!config || !config.m_aCrimes)
			return false;

		foreach (LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (!entry || entry.m_sCrimeId.IsEmpty() || entry.m_bJailOnly || entry.m_iFine <= 0 || !entry.m_sStackGroup.IsEmpty())
				continue;

			outIds.Insert(entry.m_sCrimeId);
			if (outIds.Count() == 2)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetBalance(notnull LF_BankAccount account, int amount)
	{
		int now = account.GetBalance();
		if (now > 0)
			account.Withdraw(now, "test");

		if (amount > 0)
			account.Deposit(amount, "test");
	}

	//------------------------------------------------------------------------------------------------
	//! One split through the pure rule: what each wallet gave and what is left.
	protected void CheckSplit(LF_TestContext ctx, string label, int cashHeld, int bankHeld, int fine, int expectCash, int expectBank)
	{
		LF_TestCashWallet cash = LF_TestCashWallet.Create(cashHeld, 100000);
		LF_TestCashWallet bank = LF_TestCashWallet.Create(bankHeld, 100000);
		int fromCash;
		int fromBank;
		LF_EChargePay result = LF_EChargePay.NOT_PAYABLE;
		if (!RedProof())
			result = LF_ChargePayment.Take(fine, cash, bank, fromCash, fromBank);

		ctx.Equal(LF_EChargePay.PAID, result, label + ": paid");
		ctx.Equal(expectCash, fromCash, label + ": taken from cash");
		ctx.Equal(expectBank, fromBank, label + ": taken from the bank");
		ctx.Equal(fine, (cashHeld - cash.Carried()) + (bankHeld - bank.Carried()), label + ": exactly the fine left the suspect");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BookingOffer OpenOffer(LF_TestContext ctx, notnull array<string> ids, vector atDesk, vector desk, IEntity suspectBody)
	{
		LF_PlayerAccount suspect = LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID);
		suspect.SetWantedLevel(2);
		LF_WantedReports.ClearFor(SUSPECT_UID);
		foreach (string id : ids)
			LF_WantedReports.Add(SUSPECT_UID, id, "", "", "", true);

		string refusal = LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID);
		ctx.EqualStr(string.Empty, refusal, "the desk opens an offer");
		LF_BookingOffer offer = LF_BookingDesk.OfferFor(SUSPECT_UID);
		if (offer && suspectBody)
			offer.m_SuspectId = suspectBody.GetID();

		return offer;
	}

	override void Run(LF_TestContext ctx)
	{
		// The rule: cash first, the bank for the rest, all or nothing.
		CheckSplit(ctx, "cash only", 500, 1000, 300, 300, 0);
		CheckSplit(ctx, "cash plus bank", 100, 1000, 300, 100, 200);
		CheckSplit(ctx, "bank only", 0, 1000, 300, 0, 300);

		LF_TestCashWallet shortCash = LF_TestCashWallet.Create(100, 100000);
		LF_TestCashWallet shortBank = LF_TestCashWallet.Create(100, 100000);
		int noCash;
		int noBank;
		ctx.Equal(LF_EChargePay.SHORT, LF_ChargePayment.Take(300, shortCash, shortBank, noCash, noBank), "$200 between cash and bank is short of $300");
		ctx.Equal(200, shortCash.Carried() + shortBank.Carried(), "a short suspect keeps every dollar");

		// A refused fine-only charge: a minute per $60, at least one, at most the jail's 15 minutes.
		ctx.Equal(60, LF_ChargePayment.RefusedFineSeconds(1), "$1 costs a minute");
		ctx.Equal(240, LF_ChargePayment.RefusedFineSeconds(200), "$200 costs four minutes");
		ctx.Equal(600, LF_ChargePayment.RefusedFineSeconds(600), "$600 costs ten minutes");
		ctx.Equal(LF_JailCellComponent.MAX_SENTENCE_SECONDS, LF_ChargePayment.RefusedFineSeconds(5000), "a big fine stops at the jail's cap");

		// The real desk: a booked suspect pays from their pockets and their bank.
		LF_BookingDesk.ResetStatics();
		array<string> ids = {};
		if (!PickCodes(ids))
		{
			ctx.Fail("the crime catalog has no two plain fine codes");
			return;
		}

		if (!LF_JailBooking.IsCellKnown() || !LF_Jail.HasCell())
		{
			ctx.Fail("the test world has no jail cell for the desk to serve");
			return;
		}

		LF_Jail.Configure(60, 300, 1, 8);
		vector desk = LF_Jail.GetCellOrigin() + Vector(2, 0, 0);
		vector atDesk = desk + Vector(1, 0, 0);

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(0);
		LF_BankAccount officerBank = LF_ATMManager.GetOrCreate(OFFICER_UID);
		SetBalance(officerBank, 250);
		LF_BankAccount suspectBank = LF_ATMManager.GetOrCreate(SUSPECT_UID);

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			ctx.Fail("suspect fixture did not spawn");
			return;
		}

		LF_BookingOffer offer = OpenOffer(ctx, ids, atDesk, desk, body);
		if (offer && offer.m_bPayable && offer.m_iFine > 1)
		{
			int fine = offer.m_iFine;
			int pocket = fine / 2;
			ctx.Equal(pocket, LF_MoneyUtils.AddCash(body, pocket), "the suspect carries half the fine");
			SetBalance(suspectBank, fine);
			ctx.True(LF_JailEscort.Escort(OFFICER_UID, SUSPECT_UID), "the officer walked the suspect to the desk");

			string paid = "skipped";
			if (!RedProof())
				paid = LF_ChargePayment.Settle(offer, body);

			ctx.EqualStr(string.Empty, paid, "pay now settles the offer");
			ctx.Equal(0, LF_MoneyUtils.GetCash(body), "the pocket cash went first");
			ctx.Equal(pocket, suspectBank.GetBalance(), "the bank paid only the rest");
			ctx.Equal(0, LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID).GetWantedLevel(), "a paid suspect is no longer wanted");
			ctx.False(LF_JailEscort.IsEscorted(SUSPECT_UID), "a paid suspect is no longer held by the escort");
			LF_JailEscort.Drop(SUSPECT_UID);
			ctx.Equal(250, officerBank.GetBalance(), "the officer's bank never changes");

			// A double click: the same offer again pays nothing.
			int bankAfter = suspectBank.GetBalance();
			ctx.EqualStr("#LF-BookingDesk_NoOffer", LF_ChargePayment.Settle(offer, body), "a second click is refused");
			ctx.Equal(bankAfter, suspectBank.GetBalance(), "a second click takes nothing");
			LF_BookingDesk.Close(SUSPECT_UID);
		}
		else
		{
			ctx.Fail("the desk offered no payable fine to test");
		}

		// Short of money: the cell for the offer's time.
		LF_BookingOffer shortOffer = OpenOffer(ctx, ids, atDesk, desk, body);
		if (shortOffer && shortOffer.m_bPayable)
		{
			LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
			SetBalance(suspectBank, shortOffer.m_iFine - 1);

			string jailed = "skipped";
			if (!RedProof())
				jailed = LF_ChargePayment.Settle(shortOffer, body);

			ctx.EqualStr(string.Empty, jailed, "a short suspect's answer settles the offer");
			ctx.True(LF_Jail.IsServing(SUSPECT_UID), "a short suspect goes to the cell");
			ctx.EqualFloat(shortOffer.m_iServeSeconds, LF_Jail.GetRemaining(SUSPECT_UID), 0.5, "for the offer's time");
			ctx.Equal(shortOffer.m_iFine - 1, suspectBank.GetBalance(), "a short suspect pays nothing");
			ctx.Equal(250, officerBank.GetBalance(), "the officer's bank still never changes");

			array<string> online = {SUSPECT_UID};
			LF_Jail.TickOnline(LF_Jail.GetRemaining(SUSPECT_UID) + 5, online);
			LF_Jail.SyncPlacedCell();
		}
		else
		{
			ctx.Fail("the desk offered no payable fine for the short case");
		}

		LF_TestCharacterFixture.Delete(body);
		SetBalance(suspectBank, 0);
		SetBalance(officerBank, 0);
		LF_WantedReports.ClearFor(SUSPECT_UID);
		LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID).SetWantedLevel(0);
		officer.SetOnDuty(false);
		LF_BookingDesk.ResetStatics();
	}
}
