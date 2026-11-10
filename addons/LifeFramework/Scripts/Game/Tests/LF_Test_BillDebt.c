// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_BillDebt`: scaling the deadline in PastDeadline x100 failed it with `- past the deadline the bill is debt` and `- the bank pays what it holds (expected 30, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! medic/bill-debt (#464): a bill unpaid past its deadline becomes debt; the bank pays it as soon as it
//! has money, partially if needed, with the ledger reason; a paid bill never becomes debt; the debt
//! survives the save/load of the bills record (it lives in the existing record, layout unchanged).
class LF_Test_BillDebt : LF_Test
{
	protected static const string PATIENT = "debt-patient";

	override string GetName()
	{
		return "medic/bill-debt";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_TreatmentBillRecord> before = LF_TreatmentBills.ExportAll();
		LF_TreatmentBills.ResetStatics();

		ctx.True(LF_TreatmentBills.StampMinutes("11/11/2026 00:00") - LF_TreatmentBills.StampMinutes("10/11/2026 00:00") == 1440, "a day is 1440 minutes");
		ctx.True(LF_TreatmentBills.StampMinutes("01/03/2026 00:00") - LF_TreatmentBills.StampMinutes("28/02/2026 00:00") == 1440, "28 Feb to 1 Mar is one day");
		ctx.Equal(-1, LF_TreatmentBills.StampMinutes("garbage"), "a bad stamp does not parse");

		LF_TreatmentBillRecord bill = LF_TreatmentBills.Issue(PATIENT, "Pat", "", "", 100, 0, "10/11/2026 09:00");
		ctx.NotNull(bill, "a bill is written");
		if (!bill)
			return;

		LF_BankAccount bank = LF_BankAccount.Create(PATIENT);

		// Before the deadline: nothing happens, even with money in the bank.
		bank.SetBalance(500);
		ctx.Equal(0, LF_TreatmentBills.Sweep(PATIENT, bank, "11/11/2026 08:00", 48), "inside the deadline the bank is left alone");
		ctx.Equal(500, bank.GetBalance(), "the balance is untouched before the deadline");
		ctx.False(LF_TreatmentBills.IsDebt(bill), "an in-time bill is not debt");

		// Past the deadline, empty bank: it becomes debt and shows as outstanding.
		bank.SetBalance(0);
		ctx.Equal(0, LF_TreatmentBills.Sweep(PATIENT, bank, "12/11/2026 09:00", 48), "an empty bank pays nothing");
		ctx.True(LF_TreatmentBills.IsDebt(bill), "past the deadline the bill is debt");
		ctx.Equal(100, LF_TreatmentBills.TotalDebt(PATIENT), "the outstanding debt is the bill");

		// It survives a save and a load of the bills record.
		LF_TreatmentBills.ApplyAll(LF_TreatmentBills.ExportAll());
		ctx.Equal(100, LF_TreatmentBills.TotalDebt(PATIENT), "the debt is back after the load");
		bill = LF_TreatmentBills.Find(1);

		// Partial take: the bank has 30.
		bank.SetBalance(30);
		ctx.Equal(30, LF_TreatmentBills.Sweep(PATIENT, bank, "12/11/2026 10:00", 48), "the bank pays what it holds");
		ctx.Equal(0, bank.GetBalance(), "the bank is emptied, not overdrawn");
		ctx.Equal(70, LF_TreatmentBills.TotalDebt(PATIENT), "70 is still owed");

		// The rest, as soon as it has money; only what is owed is taken.
		bank.SetBalance(500);
		ctx.Equal(70, LF_TreatmentBills.Sweep(PATIENT, bank, "12/11/2026 11:00", 48), "the remainder is taken");
		ctx.Equal(430, bank.GetBalance(), "500 less 70");
		ctx.Equal(0, LF_TreatmentBills.TotalDebt(PATIENT), "the debt is cleared");
		ctx.True(bill.m_bPaid, "a cleared debt marks the bill paid");

		// Cleared bills never become debt again.
		ctx.Equal(0, LF_TreatmentBills.Sweep(PATIENT, bank, "30/12/2026 11:00", 48), "a paid bill is not charged twice");

		LF_TreatmentBills.ApplyAll(before);
	}
}
