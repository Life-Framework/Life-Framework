// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_GarageSlots` before LF_GarageSlots existed:
// the scripts failed to compile (unknown type), so the suite failed. Then `--red LF_Test_GarageSlots` printed
// `RED-OK vehicles/garage-slots` (the at-cap refusal and its reason fail with the hook on); green without it.

// tier: LOGIC
//! Garage slots (#2655, spec #774): base slots, price of the next slot, the max, and the cap that
//! refuses a park with a reason naming the cap.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GarageSlots : LF_Test
{
	override string GetName()
	{
		return "vehicles/garage-slots";
	}

	override void Run(LF_TestContext ctx)
	{
		// The decided core defaults.
		ctx.Equal(2, LF_GarageConfig.BaseSlots(), "base slots");
		ctx.Equal(6, LF_GarageConfig.MaxSlots(), "max slots");
		ctx.Equal(500, LF_GarageConfig.SlotPriceStart(), "first bought slot price");
		ctx.Equal(250, LF_GarageConfig.SlotPriceStep(), "slot price step");
		ctx.Equal(60, LF_GarageConfig.TowAfterMinutes(), "tow after minutes");
		ctx.Equal(150, LF_GarageConfig.TowFee(), "tow fee");
		ctx.Equal(30, LF_GarageConfig.TowExemptNearGarage(), "tow exempt radius");

		// Price of slot index n (0-based): start + step * (n - base); the base slots are free.
		ctx.Equal(0, LF_GarageSlots.PriceOfSlot(0, 2, 500, 250), "a base slot costs nothing");
		ctx.Equal(0, LF_GarageSlots.PriceOfSlot(1, 2, 500, 250), "the second base slot costs nothing");
		ctx.Equal(500, LF_GarageSlots.PriceOfSlot(2, 2, 500, 250), "first bought slot costs the start price");
		ctx.Equal(750, LF_GarageSlots.PriceOfSlot(3, 2, 500, 250), "second bought slot adds one step");
		ctx.Equal(1250, LF_GarageSlots.PriceOfSlot(5, 2, 500, 250), "the last slot adds three steps");

		// Cap: base plus bought, never past the max.
		ctx.Equal(2, LF_GarageSlots.CapFor(2, 0, 6), "no bought slots: the base");
		ctx.Equal(4, LF_GarageSlots.CapFor(2, 2, 6), "two bought slots");
		ctx.Equal(6, LF_GarageSlots.CapFor(2, 9, 6), "bought slots never pass the max");

		// The next slot's price; -1 once the max is reached.
		ctx.Equal(500, LF_GarageSlots.NextSlotPrice(2, 2, 6, 500, 250), "next slot at the base is the start price");
		ctx.Equal(1000, LF_GarageSlots.NextSlotPrice(4, 2, 6, 500, 250), "next slot after two bought");
		ctx.Equal(-1, LF_GarageSlots.NextSlotPrice(6, 2, 6, 500, 250), "no next slot at the max");
		ctx.False(LF_GarageSlots.CanBuyNext(6, 6), "the max refuses another slot");
		ctx.True(LF_GarageSlots.CanBuyNext(5, 6), "one below the max can still grow");

		// The live accessors: nobody has bought a slot yet (G2), so the cap is the base.
		ctx.Equal(0, LF_GarageSlots.BoughtSlots("test-uid-nobody"), "bought slots read 0 until G2");
		ctx.Equal(2, LF_GarageSlots.CapOf("test-uid-nobody"), "cap of an owner with no bought slot");
		ctx.Equal(0, LF_GarageSlots.UsedBy("test-uid-nobody"), "an owner with no stored vehicle uses no slot");

		// CanPark at the cap.
		// RedProof(): the cap check is skipped, so the refusal assertions below fail.
		int atCap = 2;
		if (RedProof())
			atCap = 0;

		string reason;
		ctx.True(LF_Garage.CanPark(true, false, true, 1, 2, reason), "one below the cap can park");
		ctx.True(reason.IsEmpty(), "no reason when parking is allowed");
		ctx.False(LF_Garage.CanPark(true, false, true, atCap, 2, reason), "an owner at the cap is refused");
		ctx.True(reason == "#LF-Garage_SlotsFull", "the refusal names the full slots");
		ctx.False(LF_Garage.CanPark(true, false, true, 4, 4, reason), "an owner at a raised cap is refused");
		ctx.True(LF_Garage.CanPark(true, false, true, 4, 5, reason), "a raised cap lets the next park through");
		ctx.False(LF_Garage.CanPark(true, true, true, 0, 2, reason), "a seated player is still refused under the cap");
	}
}
