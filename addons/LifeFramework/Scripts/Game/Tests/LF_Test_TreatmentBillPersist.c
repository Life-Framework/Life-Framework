// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_TreatmentBillPersist`: commenting
// out LF_TreatmentBills.ApplyAll in the serializer's bills read failed it with `- the paid bill is back after the load`.
// Restored; green.

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
//! medic/bill-persist (#260): bills ride the real save path (the "bills" payload, save version 19):
//! the amount, the paid flag and the unpaid medic share come back after the store is wiped.
class LF_Test_TreatmentBillPersist : LF_Test
{
	protected static const string PATIENT = "bill-save-patient";
	protected static const string MEDIC = "bill-save-medic";

	override string GetName()
	{
		return "medic/bill-persist";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_TreatmentBillRecord> before = LF_TreatmentBills.ExportAll();
		LF_TreatmentBills.ApplyAll(null);
		LF_TreatmentBillRecord first = LF_TreatmentBills.Issue(PATIENT, "Ada Patient", MEDIC, "Bo Medic", 100, 50, "10/11/1989 08:00");
		LF_TreatmentBillRecord second = LF_TreatmentBills.Issue(PATIENT, "Ada Patient", MEDIC, "Bo Medic", 80, 40, "10/11/1989 09:00");
		LF_TreatmentBillRecord shown;
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(500, 1000);
		// Bills are paid oldest first: the first is paid, the second stays open.
		LF_TreatmentBills.Pay(PATIENT, wallet, "10/11/1989 10:00", shown);
		int paidNumber = first.m_iNumber;
		int openNumber = second.m_iNumber;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(PATIENT), "the save carries the bills payload");

		// Restart: the session has no bills.
		LF_TreatmentBills.ApplyAll(null);
		ctx.Equal(0, LF_TreatmentBills.CountOpen(PATIENT), "the restarted session starts without the bills");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(27, version, "the save is written at version 27 (bills is payload 19)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "bills");
		LF_TreatmentBillRecord loadedPaid = LF_TreatmentBills.Find(paidNumber);
		LF_TreatmentBillRecord loadedOpen = LF_TreatmentBills.Find(openNumber);
		ctx.NotNull(loadedPaid, "the paid bill is back after the load");
		ctx.NotNull(loadedOpen, "the open bill is back after the load");
		if (loadedPaid && loadedOpen)
		{
			ctx.True(loadedPaid.m_bPaid, "the paid flag is back, so it is not paid twice");
			ctx.False(loadedPaid.m_bShareTaken, "the medic share is still owed");
			ctx.Equal(100, loadedPaid.m_iAmount, "the amount is back");
			ctx.Equal(50, loadedPaid.m_iShare, "the medic share is back");
			ctx.EqualStr("Bo Medic", loadedPaid.m_sMedicName, "the medic is back");
			ctx.False(loadedOpen.m_bPaid, "the open bill is still unpaid");
			ctx.Equal(1, LF_TreatmentBills.CountOpen(PATIENT), "one bill is still owed");
			ctx.Equal(50, LF_TreatmentBills.TakeShare(MEDIC), "the share is paid once after the restart");
			ctx.Equal(0, LF_TreatmentBills.TakeShare(MEDIC), "and not twice");
		}

		LF_TreatmentBills.ApplyAll(before);
	}
}
