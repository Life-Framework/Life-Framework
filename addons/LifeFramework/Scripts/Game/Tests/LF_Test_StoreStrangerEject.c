// red-proof: #2411 follow-up. The rule that shipped with #2457 refused a store for ANY occupant; the
// RedProof() hook below restores that rule, so the stranger assertion fails under `--red`.
// Why LOGIC: the engine seats and ejects a character a frame later (see LF_Test_JailVehicleArrest), and the
// suite runs synchronously, so a live stranger in a seat cannot be arranged here. The eject and the
// re-check that never deletes a player are checked by hand (PR "Manual testing").
// Observed 2026-11-18 `tools/cli test --no-build --red LF_Test_StoreStrangerEject,LF_Test_JobDepositRestart`: RED-OK, 7 assertions fail with the hook on.
// tier: LOGIC
//! Garaging a car: the owner and a keyholder seated in it block the store, a stranger does not (the
//! store ejects them), and an empty car with no keyholder has no refusal.
class LF_Test_StoreStrangerEject : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/store-ejects-stranger";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool strangerBlocks = LF_Garage.OccupantBlocksStore(false, false);
		if (RedProof())
			strangerBlocks = true;

		ctx.False(strangerBlocks, "a stranger seated in the car does not block the store (it ejects them)");
		ctx.True(LF_Garage.OccupantBlocksStore(true, false), "the owner seated in the car blocks the store");
		ctx.True(LF_Garage.OccupantBlocksStore(false, true), "a keyholder seated in the car blocks the store");
		ctx.True(LF_Garage.OccupantBlocksStore(true, true), "the owner with the key blocks the store");

		ctx.False(LF_Garage.KeyholderInside(null, "none", "nobody"), "no car, no keyholder inside");
		ctx.False(LF_Garage.HasOccupants(null), "no car, no occupants");
	}
}
