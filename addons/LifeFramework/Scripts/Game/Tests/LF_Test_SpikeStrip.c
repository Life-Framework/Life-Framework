// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_SpikeStrip`: the Puncture call in LF_SpikeStripComponent.CheckVehicle disabled failed `wheel 1 (1.04016 m along the strip) crossed the strip and is flat`, `... runs on a smaller radius` and `the strip counts its punctures (expected 2, got 0)`; restored, green.
// tier: WORLD
//! #646 acceptance: only on-duty police can place a strip, and a vehicle crossing it loses tyre
//! health. An officer stands at the verge of the main street facing across it and deploys the
//! folded strip; it lies on the ground in front of them and the item is used up. A civilian is
//! refused. A car is put on the road with its left wheels in line with the strip and moved across
//! it between two checks: the two wheels whose paths crossed are DESTROYED through the vanilla wheel
//! damage (radius state below the tyre's own radius), the two that passed beside it are not.
//! Picking the strip up gives the officer the folded item back.
class LF_Test_SpikeStrip : LF_Test
{
	static const ResourceName ITEM = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";
	static const ResourceName STRIP = "{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et";
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	protected static const string OFFICER_UID = "spike-strip-officer";
	protected static const string CIVILIAN_UID = "spike-strip-civilian";
	//! Open verge south of Village_MainStreet (centre line z = 250), east of the fuel station.
	protected static const vector VERGE = "300 0 245.5";

	override string GetName()
	{
		return "police/spike-strip";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(600, LF_PoliceConfig.SpikeStripLifetimeSeconds(), "a strip lasts the configured ten minutes");

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !civilian)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		LF_PlayerAccount civilianAccount = LF_TestPoliceKit.Account(CIVILIAN_UID, false);
		LF_PoliceRanks.BindOfficer(civilian, CIVILIAN_UID);

		Stand(officer, VERGE, 0);
		Stand(civilian, VERGE + "4 0 0", 0);
		IEntity strip;
		IEntity car;
		RunDeployAndPuncture(ctx, officer, civilian, strip, car);

		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(strip);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDeployAndPuncture(LF_TestContext ctx, IEntity officer, IEntity civilian, out IEntity strip, out IEntity car)
	{
		IEntity refusedItem = LF_Utils.SpawnEntityPrefab(ITEM, civilian.GetOrigin() + "0 0 0.5");
		ctx.True(refusedItem != null, "the folded strip spawns");
		ctx.True(LF_SpikeStrips.Deploy(civilian, refusedItem) == null, "a civilian cannot deploy the strip");
		ctx.True(LF_SpikeStrips.LastRefusal().IndexOf("not on-duty police") != -1, "the refusal says why: " + LF_SpikeStrips.LastRefusal());
		LF_TestCharacterFixture.Delete(refusedItem);

		IEntity item = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		ctx.True(placeable != null && LF_Utils.SamePrefab(placeable.GetPlacedPrefab(), STRIP), "the folded strip places the strip prefab");

		int before = LF_SpikeStrips.Count();
		EntityID itemId = item.GetID();
		strip = LF_SpikeStrips.Deploy(officer, item);
		if (!strip)
		{
			ctx.Fail("an on-duty officer's deploy was refused: " + LF_SpikeStrips.LastRefusal());
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		ctx.Equal(before + 1, LF_SpikeStrips.Count(), "the deployed strip registers");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) == null, "the folded item is used up");

		// The placement clearance check (#928): a second strip on the same spot is refused, the
		// first one lies in its clearance box.
		IEntity second = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");
		ctx.True(LF_SpikeStrips.Deploy(officer, second) == null, "a second strip on the same spot is refused");
		ctx.True(LF_SpikeStrips.LastRefusal().IndexOf("in the way") != -1, "the refusal says something is in the way: " + LF_SpikeStrips.LastRefusal());
		LF_TestCharacterFixture.Delete(second);
		vector transform[4];
		strip.GetWorldTransform(transform);
		vector origin = transform[3];
		float ground = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		ctx.InRange(origin[1] - ground, -0.05, 0.3, "the strip lies on the ground");
		ctx.InRange(Math.AbsFloat(transform[2][2]), 0.95, 1.01, "it runs across the street (along z)");
		ctx.InRange(origin[2], VERGE[2] + 2.4, VERGE[2] + 3.1, "its middle is the gap plus half its length in front of the officer");

		LF_SpikeStripComponent spikes = LF_Component<LF_SpikeStripComponent>.Find(strip);
		if (!spikes)
		{
			ctx.Fail("the strip prefab has no LF_SpikeStripComponent");
			return;
		}

		ctx.True(LF_Utils.SamePrefab(spikes.GetItemPrefab(), ITEM), "the strip folds back into the item prefab");

		// The car drives along the street (x). Its left wheels run in line with the strip's far
		// half, its right wheels beyond the strip's far end.
		float carZ = origin[2] + 1.75;
		car = SpawnCar(Vector(origin[0] - 4, 0, carZ));
		if (!car)
		{
			ctx.Fail("the car did not spawn: " + CAR);
			return;
		}

		array<SCR_WheelHitZone> wheels = {};
		LF_SpikeStrips.Wheels(car, wheels);
		ctx.Equal(4, wheels.Count(), "the car's four wheel hit zones are found on its wheel slots");
		spikes.Tick();
		Place(car, Vector(origin[0] + 4, 0, carZ));
		spikes.Tick();

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		int flat = 0;
		int whole = 0;
		foreach (SCR_WheelHitZone wheel : wheels)
		{
			float side = wheel.GetOwner().GetOrigin()[2] - origin[2];
			bool onStrip = Math.AbsFloat(side) <= 1.75;
			bool destroyed = wheel.GetDamageState() == EDamageState.DESTROYED;
			string label = string.Format("wheel %1 (%2 m along the strip)", wheel.GetWheelIndex(), side);
			if (onStrip)
			{
				ctx.True(destroyed, label + " crossed the strip and is flat");
				flat++;
				if (simulation && simulation.IsValid())
					ctx.True(simulation.WheelGetRadiusState(wheel.GetWheelIndex()) < simulation.WheelGetRadius(wheel.GetWheelIndex()), label + " runs on a smaller radius");
			}
			else
			{
				ctx.False(destroyed, label + " passed beside the strip and is whole");
				whole++;
			}
		}

		ctx.Equal(2, flat, "two wheels crossed the strip");
		ctx.Equal(2, whole, "two wheels passed beside it");
		ctx.Equal(2, spikes.GetPunctures(), "the strip counts its punctures");

		// A second pass over the same strip does not damage a flat wheel again.
		Place(car, Vector(origin[0] - 4, 0, carZ));
		spikes.Tick();
		ctx.Equal(2, spikes.GetPunctures(), "a flat wheel is not punctured twice");

		// The civilian cannot pick it up; the officer can.
		ctx.False(LF_SpikeStrips.PickUp(civilian, strip), "a civilian cannot pick the strip up");
		EntityID stripId = strip.GetID();
		ctx.True(LF_SpikeStrips.PickUp(officer, strip), "the officer picks the strip up");
		ctx.True(GetGame().GetWorld().FindEntityByID(stripId) == null, "the strip is gone");
		ctx.True(LF_TestCharacterFixture.HasItem(officer, ITEM), "the officer carries the folded strip again");
	}

	//------------------------------------------------------------------------------------------------
	protected void Stand(IEntity character, vector at, float yaw)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnCar(vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.6;
		// Facing +x: yaw 90.
		return LF_Utils.SpawnEntityPrefab(CAR, at, "90 0 0");
	}

	//------------------------------------------------------------------------------------------------
	protected void Place(IEntity car, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.6;
		vector transform[4];
		car.GetWorldTransform(transform);
		transform[3] = at;
		car.SetWorldTransform(transform);
		car.Update();
		Physics physics = car.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}
	}
}
