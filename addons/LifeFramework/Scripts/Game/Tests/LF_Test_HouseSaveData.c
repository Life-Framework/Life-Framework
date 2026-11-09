// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: replacing the empty-identifier guard in
// LF_HouseLockRecord.IsValidFor with `if (false)` failed it with `[LFTEST] FAIL houses/save-data`,
// `- an unbound record is rejected everywhere`. Restored; green. The VERSION constant and Create
// field read-backs were removed: they restated what the test had just written and could not fail.

// tier: LOGIC
class LF_Test_HouseSaveData : LF_Test
{
	override string GetName()
	{
		return "houses/save-data";
	}

	override void Run(LF_TestContext ctx)
	{
		// ApplyLockState routes every loaded record through IsValidFor before touching a house.
		LF_HouseLockRecord record = LF_HouseLockRecord.Create("house-a", true);
		ctx.True(record.IsValidFor("house-a"), "a record for the same house applies");
		ctx.False(record.IsValidFor("house-b"), "a record for a different house is rejected");
		ctx.False(record.IsValidFor(""), "a record is not applied to a house with no stable key");
		ctx.False(LF_HouseLockRecord.Create("", true).IsValidFor("house-a"), "an unbound record is rejected");
		ctx.False(LF_HouseLockRecord.Create("", true).IsValidFor(""), "an unbound record is rejected everywhere");
	}
};
