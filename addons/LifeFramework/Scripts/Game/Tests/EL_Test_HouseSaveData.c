// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: replacing the empty-identifier guard in
// EL_HouseLockRecord.IsValidFor with `if (false)` failed it with `[ELTEST] FAIL houses/save-data`,
// `- an unbound record is rejected everywhere`. Restored; green. The VERSION constant and Create
// field read-backs were removed: they restated what the test had just written and could not fail.

// tier: LOGIC
class EL_Test_HouseSaveData : EL_Test
{
	override string GetName()
	{
		return "houses/save-data";
	}

	override void Run(EL_TestContext ctx)
	{
		// ApplyLockState routes every loaded record through IsValidFor before touching a house.
		EL_HouseLockRecord record = EL_HouseLockRecord.Create("house-a", true);
		ctx.True(record.IsValidFor("house-a"), "a record for the same house applies");
		ctx.False(record.IsValidFor("house-b"), "a record for a different house is rejected");
		ctx.False(record.IsValidFor(""), "a record is not applied to a house with no stable key");
		ctx.False(EL_HouseLockRecord.Create("", true).IsValidFor("house-a"), "an unbound record is rejected");
		ctx.False(EL_HouseLockRecord.Create("", true).IsValidFor(""), "an unbound record is rejected everywhere");
	}
};
