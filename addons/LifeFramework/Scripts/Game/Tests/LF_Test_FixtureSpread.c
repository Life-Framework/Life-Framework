// red-proof: Observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_FixtureSpread`: RED-OK, with the hook on the second character is moved onto the first and `the second character stands clear of the first` fails. On the unfixed fixture (GetPositionAndRotation) this test failed 3 of 3 boots: a lone character stood 1.1 to 2.2 m off the gate (<75.07,109.87>, <75.16,109.46>, <75.25,108.66> for a gate at <74,108>), the second 2.3 to 4 m away.
// tier: WORLD
//! tests/fixture-spread (#4323): a fixture character stands exactly on the spawn point (the point prefab randomizes
//! GetPositionAndRotation within 2 m, so characters landed 1 to 4 m apart by chance), and a second or third one
//! stands on its own spot clear of the others, within reach of the gate.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FixtureSpread : LF_Test
{
	protected static const float CLEAR_M = 0.8;
	protected static const float NEAR_GATE_M = 3.0;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "tests/fixture-spread";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector gate = LF_TestDebrisProbe.Gate();
		ctx.True(gate != vector.Zero, "the world has a spawn point to be the gate");
		if (gate == vector.Zero)
			return;

		ctx.True(vector.Distance(LF_TestCharacterFixture.FreeSpot(gate), gate) < 0.01, "an empty gate is the free spot");

		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity third = LF_TestCharacterFixture.Spawn(ctx);
		if (first && second && third)
			Check(ctx, gate, first, second, third);

		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(third);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, vector gate, IEntity first, IEntity second, IEntity third)
	{
		if (RedProof())
			second.SetOrigin(first.GetOrigin());

		ctx.True(HorizontalDistance(first.GetOrigin(), gate) < 0.05, "a lone character stands on the gate");
		ctx.True(HorizontalDistance(first.GetOrigin(), second.GetOrigin()) >= CLEAR_M, "the second character stands clear of the first");
		ctx.True(HorizontalDistance(first.GetOrigin(), third.GetOrigin()) >= CLEAR_M, "the third character stands clear of the first");
		ctx.True(HorizontalDistance(second.GetOrigin(), third.GetOrigin()) >= CLEAR_M, "the third character stands clear of the second");
		ctx.True(HorizontalDistance(second.GetOrigin(), gate) < NEAR_GATE_M, "the second character stays within reach of the gate");
		ctx.True(HorizontalDistance(third.GetOrigin(), gate) < NEAR_GATE_M, "the third character stays within reach of the gate");
	}

	//------------------------------------------------------------------------------------------------
	protected float HorizontalDistance(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}
}
