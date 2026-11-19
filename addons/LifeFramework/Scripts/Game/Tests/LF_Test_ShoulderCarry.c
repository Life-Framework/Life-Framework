// red-proof: observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_ShoulderCarry`: RED-OK carry/shoulder-carry (fails with the hook on, 35 assertions); the hoist skipped, nothing is carried and the cap is never held.

//! Shoulder carry (#3263): a character hoists ShoulderCarryItem_Base into the hand slot, the speed cap
//! applies, a second hoist is refused, and drop puts the load on the ground and lifts the cap.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ShoulderCarry : LF_Test
{
	static const ResourceName LOAD_PREFAB = "{C418C6BBD8107C00}Prefabs/Items/Core/ShoulderCarryItem_Base.et";

	override string GetName()
	{
		return "carry/shoulder-carry";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ShoulderCarry.ResetStatics();

		// The shared rule: first refusal wins, a clear case passes.
		ctx.True(LF_ShoulderCarry.Decide(true, true, true, false, true, false, false, 1) == LF_EShoulderRefusal.NONE, "a free carrier hoists a loose load");
		ctx.True(LF_ShoulderCarry.Decide(true, true, true, false, true, true, false, 1) == LF_EShoulderRefusal.ALREADY_CARRYING, "a second load is refused");
		ctx.True(LF_ShoulderCarry.Decide(true, true, true, true, true, false, false, 1) == LF_EShoulderRefusal.CARRIER_DOWN, "a downed carrier cannot hoist");
		ctx.True(LF_ShoulderCarry.Decide(true, false, true, false, true, false, false, 1) == LF_EShoulderRefusal.NOT_CARRYABLE, "an item with no profile is refused");
		ctx.True(LF_ShoulderCarry.Decide(true, true, true, false, true, false, true, 1) == LF_EShoulderRefusal.ITEM_HELD, "a load already in a pack is refused");
		ctx.True(LF_ShoulderCarry.Decide(true, true, true, false, true, false, false, 50) == LF_EShoulderRefusal.TOO_FAR, "a far load is refused");
		ctx.True(LF_ShoulderCarry.ClampCap(0) == LF_ShoulderCarry.MIN_CAP, "a zero cap is raised to the floor");
		ctx.True(LF_ShoulderCarry.ClampCap(3) == 1, "a cap above jog is lowered to 1");

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		if (!carrier)
			return;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(carrier);
		ctx.NotNull(controller, "the carrier has a character controller");
		IEntity load = LF_Utils.SpawnEntityPrefab(LOAD_PREFAB, carrier.GetOrigin() + "1 0 0");
		IEntity second = LF_Utils.SpawnEntityPrefab(LOAD_PREFAB, carrier.GetOrigin() + "-1 0 0");
		ctx.NotNull(load, "the shoulder load spawns");
		ctx.NotNull(second, "a second shoulder load spawns");
		if (!controller || !load || !second)
		{
			LF_TestCharacterFixture.Delete(carrier);
			LF_TestCharacterFixture.Delete(load);
			LF_TestCharacterFixture.Delete(second);
			return;
		}

		LF_ShoulderCarryComponent profile = LF_Component<LF_ShoulderCarryComponent>.Find(load);
		ctx.NotNull(profile, "the load carries LF_ShoulderCarryComponent");
		ctx.NotNull(LF_Component<LF_HandCarryComponent>.Find(load), "the load is a hand-carry item");
		ctx.False(LF_ParcelComponent.InSlot(load), "a load on the ground is in no slot");

		bool hoisted = false;
		if (!RedProof())
			hoisted = LF_ShoulderCarry.Hoist(carrier, load);

		ctx.True(hoisted, "the hoist is accepted");
		ctx.True(LF_ShoulderCarry.Carried(carrier) == load, "the carrier carries the load");
		ctx.True(LF_ParcelComponent.InSlot(load), "the load is in the hand slot");
		if (profile)
			{
			// The engine only reads the dynamic speed back on a player-controlled character, so the fixture checks the cap is held, not its readback.
			ctx.True(profile.GetCarrier() == carrier, "the speed cap is held for the carrier while carried");
			ctx.True(profile.GetSpeedCap() < 1, "the cap is under jog speed, so no sprint");
		}

		ctx.False(LF_ShoulderCarry.Hoist(carrier, second), "a second hoist is refused");
		ctx.True(LF_ShoulderCarry.Carried(carrier) == load, "the first load is still the carried one");
		ctx.False(LF_ParcelComponent.InSlot(second), "the refused load stays on the ground");

		bool dropped = false;
		if (!RedProof())
			dropped = LF_ShoulderCarry.Drop(carrier);

		ctx.True(dropped, "the drop is accepted");
		ctx.True(LF_ShoulderCarry.Carried(carrier) == null, "nothing is carried after the drop");
		ctx.False(LF_ParcelComponent.InSlot(load), "the dropped load is on the ground");
		if (profile)
			ctx.True(profile.GetCarrier() == null, "the drop lifts the speed cap");

		LF_TestCharacterFixture.Delete(load);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(carrier);
	}
}
