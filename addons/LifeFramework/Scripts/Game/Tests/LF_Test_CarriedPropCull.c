// red-proof: Observed 2026-11-17 with `tools/cli test --no-build --red LF_Test_CarriedPropCull`: `RED PROOF OK` (the hook restores the old behaviour, a counter that never resets and a hang every frame at any range)

// tier: LOGIC
//! A carried prop's per-frame work (#2339): the give-up counter counts frames missing in a row (a good
//! frame resets it), and a walker that is not rendered or is past the view distance is not hung.
class LF_Test_CarriedPropCull : LF_Test
{
	override string GetName()
	{
		return "character/carried-prop-cull";
	}

	override void Run(LF_TestContext ctx)
	{
		// Counter: 299 missing frames, one good frame, 299 more: still holding (the old total, 598, gave up).
		int missing = 0;
		for (int i = 0; i < LF_CarriedPropComponent.MISSING_FRAMES - 1; i++)
			missing = Next(false, missing);

		ctx.True(!LF_CarriedPropComponent.GivesUp(missing), string.Format("%1 missing frames in a row is not yet the give-up point", missing));
		missing = Next(true, missing);
		ctx.Equal(0, missing, "a good frame resets the counter");
		for (int j = 0; j < LF_CarriedPropComponent.MISSING_FRAMES - 1; j++)
			missing = Next(false, missing);

		ctx.True(!LF_CarriedPropComponent.GivesUp(missing), "gaps with a good frame between do not add up");
		missing = Next(false, missing);
		ctx.True(LF_CarriedPropComponent.GivesUp(missing), "MISSING_FRAMES in a row gives up");

		// Cull: hung in view and near, skipped far or not rendered.
		float near = 10;
		float far = LF_CarriedPropComponent.VIEW_DISTANCE + 5;
		ctx.True(Hang(true, near * near), "a rendered walker 10 m away is hung");
		ctx.True(Hang(true, LF_CarriedPropComponent.VIEW_DISTANCE * LF_CarriedPropComponent.VIEW_DISTANCE), "a walker exactly at the view distance is hung");
		ctx.True(!Hang(true, far * far), "a walker past the view distance is skipped");
		ctx.True(!Hang(false, near * near), "a walker that is not rendered is skipped");
	}

	protected int Next(bool hung, int missing)
	{
		if (RedProof())
			return missing + 1; // the old counter: never resets
		return LF_CarriedPropComponent.NextMissing(hung, missing);
	}

	protected bool Hang(bool rendered, float distanceSq)
	{
		if (RedProof())
			return true; // the old behaviour: every frame, any range
		return LF_CarriedPropComponent.ShouldHang(rendered, distanceSq);
	}
}
