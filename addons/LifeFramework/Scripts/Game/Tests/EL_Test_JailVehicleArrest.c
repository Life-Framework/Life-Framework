// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_JailVehicleArrest`:
// removed the seated branch of LF_JailMoveRules.Pick; got
// `- a seated player is dismounted, never teleported with the vehicle (expected 2, got 1)`. Restored; green.
// Why LOGIC: the engine seats a character a frame after GetInVehicle (the request returns true
// and IsInCompartment stays false in the same call), and the suite runs synchronously, so a live
// seated body cannot be arranged here. The rule below is what MoveBody follows.
// manual: dedicated server, two clients. Seat the wanted player in Roadmap_Escort_Car, then the
// on-duty officer arrests them through the window. The prisoner appears in Roadmap_Jail_Cell;
// the car stays where it was.

// tier: LOGIC
class EL_Test_JailVehicleArrest : EL_Test
{
	override string GetName()
	{
		return "jail/vehicle-arrest";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(LF_EJailMove.DISMOUNT, LF_JailMoveRules.Pick(true, true), "a seated player is dismounted, never teleported with the vehicle");
		ctx.Equal(LF_EJailMove.DISMOUNT, LF_JailMoveRules.Pick(true, false), "a seated stand-in is dismounted, not set in place in the seat");
		ctx.Equal(LF_EJailMove.TELEPORT_PLAYER, LF_JailMoveRules.Pick(false, true), "a player on foot uses the vanilla player teleport");
		ctx.Equal(LF_EJailMove.DIRECT, LF_JailMoveRules.Pick(false, false), "a stand-in on foot is set directly");
	}
}
