// red-proof: 2026-11-19, `tools cli test --no-build --red LF_Test_JailEscortCar`: `RED-OK jail/escort-car (fails with the hook on, 19 assertions)`, the hook skips the seat rows so the load and slot checks fail; green without it.
//! #3431: a real police car gets escort bookkeeping on demand (a stable id, one passenger seat row per
//! suspect), and the put-in reach is a pure rule. The physical seating (MoveInVehicle) is an engine
//! request the owning client answers a frame later; it is proved by the two-client session proof.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JailEscortCar : LF_Test
{
	override string GetName()
	{
		return "jail/escort-car";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "escort-car-officer";
		string firstUid = "escort-car-first";
		string secondUid = "escort-car-second";
		string vehicleId = "escort-car-3431";

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount first = LF_PlayerAccountManager.GetOrCreate(firstUid);
		first.SetWantedLevel(2);
		LF_PlayerAccount second = LF_PlayerAccountManager.GetOrCreate(secondUid);
		second.SetWantedLevel(2);

		ctx.EqualStr("", LF_JailEscort.VehicleIdOf(null), "no vehicle, no escort id");
		ctx.True(LF_JailEscortCar.InReach(3.0), "a suspect at the car's side is in reach");
		ctx.False(LF_JailEscortCar.InReach(LF_JailEscortCar.PUT_IN_REACH + 5.0), "a suspect across the road is not");

		ctx.False(LF_JailEscort.Load(officerUid, firstUid, vehicleId), "an unregistered car takes no one");

		ctx.True(LF_JailEscort.Escort(officerUid, firstUid), "the officer holds the first suspect on foot");
		int firstSlot = -1;
		if (!RedProof())
			firstSlot = LF_JailEscort.EnsurePassengerSeat(vehicleId);

		ctx.Equal(1, firstSlot, "the first seat row is slot 1, the car is registered");
		ctx.Equal(1, LF_JailEscort.EnsurePassengerSeat(vehicleId), "a free seat row is reused, not grown");
		ctx.True(LF_JailEscort.Load(officerUid, firstUid, vehicleId), "the first suspect is seated");
		ctx.Equal(1, LF_JailEscort.GetSeat(firstUid), "in slot 1");
		ctx.EqualStr(firstUid, LF_JailEscort.GetSeatedPrisoner(officerUid), "the officer holds a seated suspect");
		ctx.EqualStr("", LF_JailEscort.GetOnFootPrisoner(officerUid), "and none on foot");

		ctx.True(LF_JailEscort.Escort(officerUid, secondUid), "the officer holds a second suspect on foot");
		ctx.Equal(2, LF_JailEscort.EnsurePassengerSeat(vehicleId), "a taken seat row grows a second one");
		ctx.True(LF_JailEscort.Load(officerUid, secondUid, vehicleId), "the second suspect is seated");
		ctx.Equal(2, LF_JailEscort.GetSeat(secondUid), "in slot 2");

		ctx.True(LF_JailEscort.TakeOut(officerUid, firstUid), "the first suspect is taken out");
		ctx.False(LF_JailEscort.IsSeated(firstUid), "out of the seat");
		ctx.Equal(1, LF_JailEscort.EnsurePassengerSeat(vehicleId), "their seat row is free again");

		LF_JailEscort.ResetStatics();
	}
}
