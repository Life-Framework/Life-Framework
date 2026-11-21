// red-proof: observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_CargoBed`: RED-OK carry/cargo-bed (fails with the hook on, 56 assertions); the loads are skipped, so the counts, the refusal, the unload order and the round trip fail.
// red-proof (layout, review 2026-11-19): the SlotTransform assertions came with the fix (the old code had no such function); the before picture is in website/public/docs-images/3265/bed-stack-before.png.
// red-proof (leftover logs, 2026-11-19): the unload-and-drop loop left four loose WoodLog_01_Small at the gate, which blocked housing/furniture's shelf and farming's seed spot in a `--tier all` run; with the new "leaves no log at the gate" check and the loop's logs not deleted, `--only LF_Test_CargoBed` failed it; with each dropped log deleted it passes.
// red-proof (lock, review 2026-11-19): with IsLockedFor forced false, `--only LF_Test_CargoBed` failed "a locked truck takes no load" and "a locked truck cannot be unloaded"; green with the fix.

//! Truck cargo bed (#3265): 3 softwood and 2 hardwood logs (7 units) go into a 12-unit bed, a 13th unit is
//! refused, unloading returns the last kind first onto the shoulder, and the stack round-trips through the
//! text RplSave/RplLoad write. Also: the hauling gate keeps a log out of a pack, a car has no bed.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CargoBed : LF_Test
{
	static const ResourceName TRUCK = "{E1A923BE1A000001}Prefabs/Vehicles/Wheeled/M923A1/EL_M923A1_transport.et";
	static const ResourceName LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	static const ResourceName HARDWOOD = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";

	override string GetName()
	{
		return "carry/cargo-bed";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rules.
		ctx.True(LF_CargoBed.DecideLoad(12, 0, 1, true) == LF_ECargoRefusal.NONE, "an empty bed takes a log");
		ctx.True(LF_CargoBed.DecideLoad(12, 11, 2, true) == LF_ECargoRefusal.FULL, "a hardwood log (2 units) does not fit 1 free unit");
		ctx.True(LF_CargoBed.DecideLoad(12, 12, 1, true) == LF_ECargoRefusal.FULL, "a full bed refuses");
		ctx.True(LF_CargoBed.DecideLoad(0, 0, 1, true) == LF_ECargoRefusal.NO_BED, "a car (capacity 0) has no bed");
		ctx.True(LF_CargoBed.DecideLoad(12, 0, 0, false) == LF_ECargoRefusal.NOT_ACCEPTED, "an unknown kind is refused");
		ctx.Equal(1, LF_CargoBed.UnitsOf(LF_CargoBed.KindIndex("log")), "a softwood log is 1 unit");
		ctx.Equal(2, LF_CargoBed.UnitsOf(LF_CargoBed.KindIndex("hardwood")), "a hardwood log is 2 units");
		ctx.Equal(2, LF_CargoBed.UnitsOf(LF_CargoBed.KindIndex("crate")), "a crate is 2 units");
		array<int> packedIn = {0, 0, 1, 1};
		array<int> packedOut = {};
		LF_CargoBed.Unpack(LF_CargoBed.Pack(packedIn), packedOut, 12);
		ctx.Equal(4, packedOut.Count(), "pack and unpack keep the stack");
		LF_CargoBed.Unpack("0,9,x,1", packedOut, 12);
		ctx.Equal(2, packedOut.Count(), "a bad token in a save is dropped, not a crash");

		// The drawn log lies along the bed, centred on its slot, whatever its pivot (review: the stack stuck
		// 3 m out of the truck's side because the 4 m log mesh has its pivot at one end and was turned across).
		vector mat[4];
		vector slot = "0 1.4 -2.5";
		LF_CargoBedComponent.SlotTransform("-0.15 0 -4", "0.15 0.3 0", slot, 0, mat);
		vector endA = Vector(0, 0, 0).Multiply4(mat);
		vector endB = Vector(0, 0, -4).Multiply4(mat);
		ctx.True(vector.Distance(Vector(0, 0, -2).Multiply4(mat), slot) < 0.01, "the log's bottom centre sits on its slot");
		ctx.True(Math.AbsFloat(endA[0] - endB[0]) < 0.01, "the log lies along the bed, not across it");
		ctx.True(Math.AbsFloat(endA[2] - endB[2] - 4) < 0.01, "both ends 2 m either side of the slot");
		LF_CargoBedComponent.SlotTransform("-2 0 -0.15", "2 0.3 0.15", slot, 0, mat);
		endA = Vector(-2, 0, 0).Multiply4(mat);
		endB = Vector(2, 0, 0).Multiply4(mat);
		ctx.True(Math.AbsFloat(endA[0] - endB[0]) < 0.01, "a mesh long on x is turned to lie along the bed");

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		if (!carrier)
			return;

		IEntity truck = LF_Utils.SpawnEntityPrefab(TRUCK, carrier.GetOrigin() + "0 0 5.7");
		ctx.NotNull(truck, "the truck spawns");
		LF_CargoBedComponent bed;
		if (truck)
			bed = LF_Component<LF_CargoBedComponent>.Find(truck);

		ctx.NotNull(bed, "the truck carries a cargo bed");
		IEntity softLog = LF_Utils.SpawnEntityPrefab(LOG, carrier.GetOrigin() + "1 0 0");
		IEntity hardLog = LF_Utils.SpawnEntityPrefab(HARDWOOD, carrier.GetOrigin() + "-1 0 0");
		ctx.NotNull(softLog, "the softwood log spawns");
		ctx.NotNull(hardLog, "the hardwood log spawns");
		if (!bed || !softLog || !hardLog)
		{
			Cleanup(carrier, truck, softLog, hardLog);
			return;
		}

		LF_ShoulderCarryComponent softProfile = LF_Component<LF_ShoulderCarryComponent>.Find(softLog);
		LF_ShoulderCarryComponent hardProfile = LF_Component<LF_ShoulderCarryComponent>.Find(hardLog);
		ctx.True(softProfile && softProfile.GetKind() == "log", "the softwood log is a shoulder load of kind log");
		ctx.True(hardProfile && hardProfile.GetKind() == "hardwood", "the hardwood log is a shoulder load of kind hardwood");
		vector mins, maxs;
		truck.GetBounds(mins, maxs);
		LF_Debug.Info(LF_CargoBed.FEATURE, "truck bounds " + mins.ToString() + " " + maxs.ToString() + " tailgate " + bed.GetTailgateWorld().ToString() + " truck at " + truck.GetOrigin().ToString());
		ctx.Equal(12, bed.GetCapacity(), "the 1989 truck holds 12 units");

		// The hauling gate: a log never goes to a pack or a pocket, only to the hand-carry storage.
		ctx.True(LF_CargoHaulingGate.Refuses(softLog, null), "a log is refused 'anywhere' (a pack)");
		ctx.False(LF_CargoHaulingGate.Refuses(truck, null), "an ordinary item is not gated");
		ctx.False(LF_CargoHaulingGate.Refuses(softLog, LF_Component<LF_HandInventoryStorageComponent>.Find(carrier)), "the hand-carry storage takes a log");

		// Load 3 softwood and 2 hardwood: 7 units. The hand-carried hardwood log goes through the full path.
		if (!RedProof())
		{
			ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.NONE, "softwood 1 goes in");
			ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.NONE, "softwood 2 goes in");
			ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.NONE, "softwood 3 goes in");
			ctx.True(bed.ServerLoadKind("hardwood") == LF_ECargoRefusal.NONE, "hardwood 1 goes in");
			ctx.True(LF_ShoulderCarry.Hoist(carrier, hardLog), "the carrier hoists the second hardwood log");
			ctx.True(bed.ServerLoad(carrier) == LF_ECargoRefusal.NONE, "the carried hardwood log is loaded at the tailgate");
		}

		ctx.Equal(7, bed.GetUnits(), "3 softwood and 2 hardwood are 7 units");
		ctx.Equal(3, bed.CountOf(LF_CargoBed.KindIndex("log")), "3 softwood units on the bed");
		ctx.Equal(4, bed.CountOf(LF_CargoBed.KindIndex("hardwood")), "2 hardwood logs are 4 units");
		ctx.True(LF_ShoulderCarry.Carried(carrier) == null, "loading takes the item off the shoulder");

		// Unloading returns the last kind first, onto the unloader's shoulder.
		ctx.Equal(LF_CargoBed.KindIndex("hardwood"), bed.TopKind(), "the last kind loaded is on top");
		ctx.True(bed.ServerUnload(carrier) == LF_ECargoRefusal.NONE, "unloading puts the top item on the shoulder");
		IEntity unloaded = LF_ShoulderCarry.Carried(carrier);
		ctx.NotNull(unloaded, "the unloader carries the item");
		LF_ShoulderCarryComponent unloadedProfile = LF_Component<LF_ShoulderCarryComponent>.Find(unloaded);
		ctx.True(unloadedProfile && unloadedProfile.GetKind() == "hardwood", "the first item out is the hardwood log (last in, first out)");
		ctx.Equal(5, bed.GetUnits(), "a hardwood log leaving frees 2 units");
		ctx.True(bed.ServerUnload(carrier) == LF_ECargoRefusal.NO_HANDS, "a second unload with full hands is refused");

		// A locked truck: the bed follows the trunk's lock, so a stranger can neither empty nor fill it.
		LF_VehicleLockComponent truckLock = LF_Component<LF_VehicleLockComponent>.Find(truck);
		ctx.NotNull(truckLock, "the truck has a lock");
		if (truckLock)
		{
			truckLock.SetLocked(true);
			ctx.True(bed.ServerLoad(carrier) == LF_ECargoRefusal.LOCKED, "a locked truck takes no load");
			ctx.NotNull(LF_ShoulderCarry.Carried(carrier), "a refused load stays on the shoulder");
			ctx.True(bed.ServerUnload(carrier) == LF_ECargoRefusal.LOCKED, "a locked truck cannot be unloaded");
			ctx.Equal(5, bed.GetUnits(), "a locked refusal leaves the stack alone");
			truckLock.SetLocked(false);
		}

		// Put it back, fill the bed, and the 13th unit is refused.
		ctx.True(bed.ServerLoad(carrier) == LF_ECargoRefusal.NONE, "the hardwood log goes back on");
		ctx.Equal(7, bed.GetUnits(), "back to 7 units");
		for (int i = 0; i < 5; i++)
			bed.ServerLoadKind("log");

		ctx.Equal(12, bed.GetUnits(), "the bed is full at 12 units");
		ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.FULL, "a 13th unit is refused");
		ctx.True(bed.ServerLoadKind("hardwood") == LF_ECargoRefusal.FULL, "a hardwood log is refused too");
		ctx.Equal(12, bed.GetUnits(), "a refusal leaves the count alone");

		// The stack round-trips through the text RplSave and RplLoad write.
		string saved = bed.SavedText();
		for (int j = 0; j < 4; j++)
		{
			bed.ServerUnload(carrier);
			IEntity dropped = LF_ShoulderCarry.Carried(carrier);
			LF_ShoulderCarry.Drop(carrier);
			LF_TestCharacterFixture.Delete(dropped); // a dropped log is a loose entity at the gate; it must not outlive the test
		}

		ctx.True(bed.GetUnits() < 12, "the bed was emptied a little before the reload");
		bed.LoadText(saved);
		ctx.Equal(12, bed.GetUnits(), "the saved text restores all 12 units");
		ctx.True(saved == bed.SavedText(), "the saved text comes back byte for byte");
		bed.LoadText("");
		ctx.Equal(0, bed.GetUnits(), "an empty save loads an empty bed");

		// Every log this test unloaded and dropped is deleted with the rest: a loose log left at the gate
		// blocked the shelf spot of housing/furniture and a seed spot of the farming tests (#3265).
		vector gate = carrier.GetOrigin();
		Cleanup(carrier, truck, softLog, hardLog);
		LF_TestDebrisProbe probe = new LF_TestDebrisProbe();
		ctx.False(probe.Describe(gate).Contains("WoodLog"), "the test leaves no log at the gate:" + probe.Describe(gate));
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity carrier, IEntity truck, IEntity softLog, IEntity hardLog)
	{
		// A drop in the middle of the test leaves loose logs near the carrier; sweep what is still ours.
		if (softLog)
			LF_TestCharacterFixture.Delete(softLog);

		if (hardLog)
			LF_TestCharacterFixture.Delete(hardLog);

		if (truck)
			SCR_EntityHelper.DeleteEntityAndChildren(truck);

		LF_TestCharacterFixture.Delete(carrier);
	}
}
