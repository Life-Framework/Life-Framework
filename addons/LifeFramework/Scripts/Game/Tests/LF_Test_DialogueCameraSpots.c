// red-proof: 2026-11-23, `--only LF_Test_DialogueCameraSpots` before the change: LF_DialogueCamera.FrameAt, SpotAlong and SpotSide did not exist, so the test could not compile.
//! dialogue/camera-spots: the conversation camera tries the middle spot first and, behind a counter with a
//! glass partition at the middle, spots nearer the NPC (re-review A8: the camera sat inside the glass and framed
//! a door frame instead of the clerk). The first spot is the old shot, every later one is nearer the NPC or on
//! the other side, and every spot looks at the middle between the two heads.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DialogueCameraSpots : LF_Test
{
	override string GetName()
	{
		return "dialogue/camera-spots";
	}

	override void Run(LF_TestContext ctx)
	{
		vector npcHead = "0 1.7 0";
		vector playerHead = "0 1.7 2";

		vector old;
		vector oldLook;
		float oldFov;
		LF_DialogueCamera.Frame(npcHead, playerHead, old, oldLook, oldFov);

		vector first;
		vector firstLook;
		float firstFov;
		LF_DialogueCamera.FrameAt(npcHead, playerHead, LF_DialogueCamera.SpotAlong(0), LF_DialogueCamera.SpotSide(0), first, firstLook, firstFov);
		ctx.True(vector.Distance(old, first) < 0.001, "the first spot is the old middle shot: " + first);

		float nearest = 1000;
		for (int i = 0; i < LF_DialogueCamera.SPOT_COUNT; i++)
		{
			vector position;
			vector lookAt;
			float fov;
			LF_DialogueCamera.FrameAt(npcHead, playerHead, LF_DialogueCamera.SpotAlong(i), LF_DialogueCamera.SpotSide(i), position, lookAt, fov);
			ctx.True(vector.Distance(lookAt, "0 1.7 1") < 0.001, "spot " + i + " looks at the middle between the heads: " + lookAt);
			float toNpc = vector.DistanceXZ(position, npcHead);
			if (toNpc < nearest)
				nearest = toNpc;
		}

		vector last;
		vector lastLook;
		float lastFov;
		int lastIndex = LF_DialogueCamera.SPOT_COUNT - 1;
		LF_DialogueCamera.FrameAt(npcHead, playerHead, LF_DialogueCamera.SpotAlong(lastIndex), LF_DialogueCamera.SpotSide(lastIndex), last, lastLook, lastFov);
		ctx.True(last[2] < first[2], "a later spot stands nearer the NPC along the line (z " + last[2] + " against " + first[2] + ")");

		// The two sides of one distance mirror each other across the line.
		vector left;
		vector leftLook;
		float leftFov;
		LF_DialogueCamera.FrameAt(npcHead, playerHead, LF_DialogueCamera.SpotAlong(1), LF_DialogueCamera.SpotSide(1), left, leftLook, leftFov);
		ctx.True(Math.AbsFloat(left[0] + first[0]) < 0.001 && Math.AbsFloat(left[2] - first[2]) < 0.001, "spot 1 is spot 0 on the other side of the line: " + left + " against " + first);
		ctx.True(nearest < vector.DistanceXZ(first, npcHead), "some spot is nearer the NPC than the middle shot");
	}
}
