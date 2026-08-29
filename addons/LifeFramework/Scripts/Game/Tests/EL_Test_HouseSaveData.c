// red-proof: remove the empty-identifier guard from EL_HouseLockRecord.IsValidFor (return
// m_sHouseIdentifier == houseIdentifier) and run `tools\cli test --tier fast`; the
// empty-identifier assertions fail because an unbound record applies to every house, so a save
// with no stable key would relock random buildings on load.

// tier: LOGIC
class EL_Test_HouseSaveData : EL_Test
{
	override string GetName()
	{
		return "houses/save-data";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(1, EL_HouseLockRecord.VERSION, "house save layout version is 1");

		EL_HouseLockRecord record = EL_HouseLockRecord.Create("house-a", true);
		ctx.EqualStr("house-a", record.m_sHouseIdentifier, "record keeps the stable house identifier");
		ctx.True(record.m_bLocked, "record keeps the locked flag");

		// Reload: a fresh record built from the saved fields is the apply shape the manager
		// consumes (ApplyLockState routes through IsValidFor before touching live state).
		EL_HouseLockRecord restored = EL_HouseLockRecord.Create(record.m_sHouseIdentifier, record.m_bLocked);
		ctx.True(restored.IsValidFor("house-a"), "a record for the same house applies");
		ctx.False(restored.IsValidFor("house-b"), "a record for a different house is rejected");
		ctx.False(EL_HouseLockRecord.Create("", true).IsValidFor("house-a"), "an unbound record is rejected");
		ctx.False(EL_HouseLockRecord.Create("", true).IsValidFor(""), "an unbound record is rejected everywhere");
	}
};