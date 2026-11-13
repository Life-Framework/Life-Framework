// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_TreatmentBill`: removing the
// m_bPaid guard from LF_TreatmentBills.OldestOpen failed it with `- paying again finds no unpaid bill`;
// removing `record.m_bShareTaken = true` in TakeShare failed it with
// `- the medic share is paid once (expected 0, got 50)`. Both restored; green.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! medic/bill (#260): one revive writes one bill at the configured amount; paying takes cash once and
//! clears it; the medic share is paid once, only after the patient paid.
class LF_Test_TreatmentBill : LF_Test
{
	protected static const string PATIENT = "bill-patient";
	protected static const string MEDIC = "bill-medic";

	override string GetName()
	{
		return "medic/bill";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_TreatmentBills.ResetStatics();

		ctx.Equal(50, LF_TreatmentBills.ShareOf(100, 0.5), "half of 100 is 50");
		ctx.Equal(33, LF_TreatmentBills.ShareOf(100, 0.333), "the share rounds down");
		ctx.Equal(0, LF_TreatmentBills.ShareOf(100, 0), "no share configured pays nothing");
		ctx.Equal(100, LF_TreatmentBills.ShareOf(100, 3), "a share never exceeds the bill");
		ctx.Equal(0, LF_TreatmentBills.ShareOf(-5, 0.5), "a negative bill has no share");

		// One revive, one bill.
		ctx.True(LF_TreatmentBills.Issue("", "x", MEDIC, "Med", 100, 50, "d") == null, "no patient, no bill");
		ctx.True(LF_TreatmentBills.Issue(PATIENT, "Pat", MEDIC, "Med", 0, 0, "d") == null, "a zero bill is not written");
		LF_TreatmentBillRecord bill = LF_TreatmentBills.Issue(PATIENT, "Pat", MEDIC, "Med", 100, 50, "10/11/1989 09:00");
		ctx.NotNull(bill, "a revive writes a bill");
		if (!bill)
			return;

		ctx.Equal(1, LF_TreatmentBills.CountOpen(PATIENT), "the patient owes one bill");
		ctx.Equal(100, bill.m_iAmount, "the bill carries the amount");
		ctx.EqualStr(MEDIC, bill.m_sMedicUid, "the bill names the medic");
		ctx.False(bill.m_bPaid, "a new bill is unpaid");

		// Not paid yet: the medic is owed nothing.
		ctx.Equal(0, LF_TreatmentBills.ShareOwed(MEDIC), "an unpaid bill earns the medic nothing");
		ctx.Equal(0, LF_TreatmentBills.TakeShare(MEDIC), "clock-out before payment pays nothing");

		// Short on cash: nothing is taken.
		LF_TestCashWallet poor = LF_TestCashWallet.Create(99, 1000);
		LF_TreatmentBillRecord shown;
		ctx.True(LF_TreatmentBills.Pay(PATIENT, poor, "p", shown) == LF_EBillPay.NOT_ENOUGH_CASH, "99 does not pay a 100 bill");
		ctx.Equal(99, poor.Carried(), "a refused payment takes nothing");
		ctx.False(bill.m_bPaid, "the refused bill stays unpaid");

		// Paid in cash at the desk, once.
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(250, 1000);
		ctx.True(LF_TreatmentBills.Pay(PATIENT, wallet, "10/11/1989 10:00", shown) == LF_EBillPay.PAID, "250 pays the bill");
		ctx.Equal(150, wallet.Carried(), "the desk takes exactly the bill from carried cash");
		ctx.True(bill.m_bPaid, "paying clears the bill");
		ctx.EqualStr("10/11/1989 10:00", bill.m_sPaidAt, "the bill is stamped with the payment time");
		ctx.Equal(0, LF_TreatmentBills.CountOpen(PATIENT), "nothing is owed after paying");
		ctx.True(LF_TreatmentBills.Pay(PATIENT, wallet, "p", shown) == LF_EBillPay.NO_BILL, "paying again finds no unpaid bill");
		ctx.Equal(150, wallet.Carried(), "the second payment takes nothing");

		// The medic share is paid once.
		ctx.Equal(50, LF_TreatmentBills.ShareOwed(MEDIC), "a paid bill earns the medic the share");
		ctx.Equal(50, LF_TreatmentBills.TakeShare(MEDIC), "clock-out pays the share");
		ctx.Equal(0, LF_TreatmentBills.TakeShare(MEDIC), "the medic share is paid once");
		ctx.Equal(0, LF_TreatmentBills.ShareOwed(MEDIC), "nothing is left owed");

		// A wake bill has no medic and no share; the oldest bill is paid first; open bills are capped.
		LF_TreatmentBillRecord wake = LF_TreatmentBills.Issue(PATIENT, "Pat", "", "", 150, 50, "d");
		ctx.Equal(0, wake.m_iShare, "a bill with no medic carries no share");
		LF_TreatmentBillRecord second = LF_TreatmentBills.Issue(PATIENT, "Pat", MEDIC, "Med", 80, 40, "d");
		ctx.NotNull(second, "a second open bill is written");
		ctx.True(LF_TreatmentBills.OldestOpen(PATIENT) == wake, "the oldest unpaid bill is paid first");
		for (int i = 0; i < LF_MedicConfig.Get().m_iBillMaxOpenPerPatient; i++)
		{
			LF_TreatmentBills.Issue(PATIENT, "Pat", MEDIC, "Med", 10, 5, "d");
		}

		ctx.Equal(LF_MedicConfig.Get().m_iBillMaxOpenPerPatient, LF_TreatmentBills.CountOpen(PATIENT), "unpaid bills are capped per patient");

		LF_TreatmentBills.ResetStatics();
	}
}
