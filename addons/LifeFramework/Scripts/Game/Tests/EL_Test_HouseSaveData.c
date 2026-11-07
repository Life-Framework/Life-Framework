// red-proof: PENDING for this revision - not yet observed (the server install was being restored).
// Planned perturbation: remove the empty-identifier guard from EL_HouseLockRecord.IsValidFor
// (return m_sHouseIdentifier == houseIdentifier) and run `tools\cli test --no-build --tier fast`;
// the unbound-record assertions go red, because an unbound record would relock every house.
// The VERSION constant and Create field read-backs were removed: they restated what the test
// had just written and could not fail.

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
