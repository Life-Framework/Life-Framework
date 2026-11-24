// red-proof: observed 2026-11-24 on the unfixed code, `tools/cli test --no-build --only LF_Test_CargoBedYardTrucks`: failed "the Ural has a cargo bed", "a too-far refusal has a reason a player can read" and "a refusal never reaches the player as silence" (RefusalKey had no TOO_FAR or NO_ITEM_PREFAB row, the yard's Ural4320 had no bed); green with the fix.
//! The trucks the yard sells carry a bed that takes what the lumber arc fells (#4886). A hoisted hardwood log loads
//! at the tailgate of the cargo truck and of the Ural (12 units each, the ladder's "truck" rung), a carrier who
//! stands too far away is refused TOO_FAR, and every refusal has text for the toast and the action's reason: a
//! refusal that reached a player as silence is what made the playtest's truck run unplayable.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CargoBedYardTrucks : LF_Test
{
	static const ResourceName CARGO_TRUCK = "{E1A923BE1A000001}Prefabs/Vehicles/Wheeled/M923A1/EL_M923A1_transport.et";
	static const ResourceName URAL = "{2B402516452EC797}Prefabs/Vehicles/Wheeled/Ural4320/LF_Ural4320_transport_CIV.et";
	static const ResourceName HARDWOOD = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";

	override string GetName()
	{
		return "carry/cargo-bed-yard-trucks";
	}

	override void Run(LF_TestContext ctx)
	{
		// Every refusal names its reason. NONE is the one answer that says nothing.
		for (int refusal = LF_ECargoRefusal.NONE + 1; refusal <= LF_ECargoRefusal.LOCKED; refusal++)
		{
			string key = LF_CargoBed.RefusalKey(refusal);
			ctx.True(key.StartsWith("#"), "a refusal never reaches the player as silence: " + LF_CargoBed.RefusalText(refusal));
		}

		ctx.True(LF_CargoBed.RefusalKey(LF_ECargoRefusal.NONE).IsEmpty(), "a load that goes in has no refusal text");
		ctx.True(LF_CargoBed.RefusalKey(LF_ECargoRefusal.TOO_FAR) == "#LF-Cargo_TooFar", "a too-far refusal has a reason a player can read");

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		if (!carrier)
			return;

		array<ResourceName> trucks = {CARGO_TRUCK, URAL};
		array<string> names = {"cargo truck", "Ural"};
		foreach (int index, ResourceName prefab : trucks)
			CheckTruck(ctx, carrier, prefab, names[index]);

		LF_TestCharacterFixture.Delete(carrier);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTruck(LF_TestContext ctx, IEntity carrier, ResourceName prefab, string name)
	{
		IEntity truck = LF_Utils.SpawnEntityPrefab(prefab, carrier.GetOrigin() + "0 0 6");
		ctx.NotNull(truck, "the " + name + " spawns");
		if (!truck)
			return;

		LF_CargoBedComponent bed = LF_Component<LF_CargoBedComponent>.Find(truck);
		ctx.NotNull(bed, "the " + name + " has a cargo bed");
		IEntity log = LF_Utils.SpawnEntityPrefab(HARDWOOD, carrier.GetOrigin() + "-1 0 0");
		ctx.NotNull(log, "the hardwood log spawns");
		if (!bed || !log)
		{
			if (log)
				LF_TestCharacterFixture.Delete(log);

			SCR_EntityHelper.DeleteEntityAndChildren(truck);
			return;
		}

		vector mins, maxs;
		truck.GetBounds(mins, maxs);
		LF_Debug.Info(LF_CargoBed.FEATURE, name + " bounds " + mins.ToString() + " " + maxs.ToString() + " tailgate " + bed.GetTailgateWorld().ToString() + " truck at " + truck.GetOrigin().ToString());
		ctx.Equal(12, bed.GetCapacity(), "the " + name + " holds 12 units, the ladder's truck rung");

		// The tailgate one metre behind the carrier: the load is offered where a player stands.
		vector shift = carrier.GetOrigin() + "0 0 1" - bed.GetTailgateWorld();
		shift[1] = 0;
		truck.SetOrigin(truck.GetOrigin() + shift);
		truck.Update();
		ctx.True(LF_ShoulderCarry.Hoist(carrier, log), "the carrier hoists a felled hardwood log");
		ctx.True(bed.DecideFor(carrier, LF_CargoBed.KindIndex("hardwood")) == LF_ECargoRefusal.NONE, "the " + name + " offers the load at its tailgate");
		if (!RedProof())
			ctx.True(bed.ServerLoad(carrier) == LF_ECargoRefusal.NONE, "a hoisted hardwood log loads onto the " + name);

		ctx.Equal(2, bed.GetUnits(), "a hardwood log is 2 units on the " + name);
		ctx.True(LF_ShoulderCarry.Carried(carrier) == null, "the log left the shoulder");

		// Too far from the tailgate: refused, with the reason a player reads, not silence.
		truck.SetOrigin(truck.GetOrigin() + "0 0 12");
		truck.Update();
		LF_ECargoRefusal tooFar = bed.DecideFor(carrier, LF_CargoBed.KindIndex("hardwood"));
		ctx.True(tooFar == LF_ECargoRefusal.TOO_FAR, "a carrier 13 m from the tailgate of the " + name + " is refused TOO_FAR");
		ctx.False(LF_CargoBed.RefusalKey(tooFar).IsEmpty(), "the too-far refusal of the " + name + " has text");

		if (log)
			LF_TestCharacterFixture.Delete(log);

		SCR_EntityHelper.DeleteEntityAndChildren(truck);
	}
}
