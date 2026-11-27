// red-proof: RED-OK 2026-11-27 `tools/cli test --no-build --red LF_Test_DutyLockerCash,LF_Test_SearchSuspectName` (log test-20261127-183949): with the clock-in skipped it fails "the civilian cash is in the duty locker (expected 100, got 0)", "the officer on duty carries none of it", "the toast amount is the locker cash, not the pay". Green without the hook.
//! police/duty-locker-cash (#5148): clock-in puts the civilian cash in the duty locker and the desk
//! says so; clock-out hands exactly that cash back, once, on its own line beside the shift pay.
//! The locker never holds cash twice and the body never gets it twice.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DutyLockerCash : LF_Test
{
	protected static const string UID = "test-duty-locker-cash";
	protected static const int CASH = 100;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-locker-cash";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("#LF-Duty_LockerCashReturned", LF_DutyLocker.LockerCashReturnedKey(CASH), "clock-out with locker cash says it came back");
		ctx.EqualStr("", LF_DutyLocker.LockerCashReturnedKey(0), "clock-out with an empty locker says nothing about it");

		array<ref LF_StorageSnapshotRecord> snapshots = LF_StorageSnapshot.ExportAll();
		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			LF_TestPoliceKit.Release(UID);
			LF_StorageSnapshot.ApplyAll(snapshots);
			return;
		}

		int start = LF_MoneyUtils.GetCash(body);
		ctx.Equal(CASH, LF_MoneyUtils.AddCash(body, CASH, "test"), "the new officer carries the test cash");
		int civilianCash = start + CASH;

		if (!RedProof())
			ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "the officer clocks in");
		ctx.Equal(civilianCash, LF_DutyLocker.LockerCash(characterId), "the civilian cash is in the duty locker");
		ctx.Equal(0, LF_MoneyUtils.GetCash(body), "the officer on duty carries none of it");
		ctx.Equal(civilianCash, LF_DutyLocker.CashIn(LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_CIVILIAN)), "the stored tree counts the same cash");

		int returning = LF_DutyLocker.LockerCash(characterId);
		ctx.EqualStr("", LF_DutyLocker.ClockOut(account, body), "the officer clocks out");
		int back = LF_MoneyUtils.GetCash(body);
		// The shift lasted a moment: any duty pay is a few dollars at $350-450 an hour.
		ctx.True(back >= civilianCash, string.Format("the locker cash comes back (expected at least %1, got %2)", civilianCash, back));
		ctx.True(back < civilianCash + CASH, string.Format("the locker cash comes back once, not twice (got %1 from %2)", back, civilianCash));
		ctx.Equal(civilianCash, returning, "the toast amount is the locker cash, not the pay");
		ctx.Equal(0, LF_DutyLocker.LockerCash(characterId), "the locker is empty after clock-out");

		LF_TestPoliceKit.Release(UID);
		LF_TestCharacterFixture.Delete(body);
		LF_StorageSnapshot.ApplyAll(snapshots);
	}
}
