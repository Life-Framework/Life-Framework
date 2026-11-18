// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_FixtureLeftovers`: RED-OK, with the hook on the character stays and `- deleted, the gate lists nothing (expected 0, got 1)` fails; green without it. Before the payphone fix the booth step failed the same way, the loose part outliving DeleteEntityAndChildren.
//! The end-of-run leftover check (#2792, LF_TestDebrisProbe): a fixture character left standing at the gate
//! and the loose part a spawned payphone booth leaves are both listed; deleting them (the character, and the
//! booth through LF_TestLooseSweep.DeleteBuilding) empties the list again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FixtureLeftovers : LF_Test
{
	override string GetName()
	{
		return "tests/fixture-leftovers";
	}

	override void Run(LF_TestContext ctx)
	{
		vector gate = LF_TestDebrisProbe.Gate();
		ctx.True(gate != vector.Zero, "the world has a spawn point to be the gate");
		if (gate == vector.Zero)
			return;

		LF_TestDebrisProbe probe = new LF_TestDebrisProbe();
		probe.Watch(gate);
		probe.Leftovers(gate);
		ctx.Equal(0, probe.Count(), "a gate nobody has used lists nothing");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		probe.Leftovers(gate);
		ctx.Equal(1, probe.Count(), "a fixture character left standing is listed (its jacket goes with it)");

		if (!RedProof())
			LF_TestCharacterFixture.Delete(character);

		probe.Leftovers(gate);
		ctx.Equal(0, probe.Count(), "deleted, the gate lists nothing");

		IEntity booth = LF_Utils.SpawnEntityPrefab(LF_Test_PhoneCallWorld.PAYPHONE, gate + "3 0 0");
		ctx.NotNull(booth, "the payphone booth spawns");
		probe.Leftovers(gate);
		ctx.Equal(1, probe.Count(), "the loose part a payphone booth leaves is listed");

		LF_TestLooseSweep.DeleteBuilding(booth);
		if (RedProof())
			LF_TestCharacterFixture.Delete(character);

		probe.Leftovers(gate);
		ctx.Equal(0, probe.Count(), "deleted through DeleteBuilding, the gate lists nothing");
	}
}
