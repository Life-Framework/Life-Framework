// red-proof: RedProof() skips the parse of the drive's point and options, so "the x,z point parses" and
// "speed= option is read" fail (tools\cli test --no-build --red LF_Test_DriveHarness).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DriveHarness : LF_Test
{
	override string GetName()
	{
		return "agent-harness/drive-parse";
	}

	override void Run(LF_TestContext ctx)
	{
		vector point;
		bool parsed;
		string speed;
		array<string> tokens = {"to", "4688.5,10752", "speed=30", "--timeout=90", "wait"};
		if (!RedProof())
		{
			parsed = LF_DriveHarness.ParsePoint(tokens[1], point);
			speed = LF_DriveHarness.Option(tokens, "speed");
		}

		ctx.True(parsed, "the x,z point parses");
		ctx.EqualFloat(4688.5, point[0], 0.01, "point x");
		ctx.EqualFloat(10752, point[2], 0.01, "point z");
		ctx.EqualStr("30", speed, "speed= option is read");
		ctx.EqualStr("90", LF_DriveHarness.Option(tokens, "timeout"), "a --key=value option is read");
		ctx.EqualStr("", LF_DriveHarness.Option(tokens, "tol"), "a missing option is empty");
		ctx.True(LF_DriveHarness.HasWord(tokens, "wait"), "a bare word is seen");

		vector bad;
		ctx.False(LF_DriveHarness.ParsePoint("4688", bad), "one number is not a point");
		ctx.False(LF_DriveHarness.ParsePoint("1,2,3", bad), "three numbers are not a point");

		float t;
		float d = LF_DriveHarness.SegmentDistance("5 0 3", "0 0 0", "10 0 0", t);
		ctx.EqualFloat(3, d, 0.01, "distance to the segment's middle");
		ctx.EqualFloat(0.5, t, 0.01, "nearest point halfway along");
		d = LF_DriveHarness.SegmentDistance("-4 0 3", "0 0 0", "10 0 0", t);
		ctx.EqualFloat(5, d, 0.01, "before the segment the start point is nearest");
		ctx.EqualFloat(0, t, 0.01, "clamped to the start");
	}
}
