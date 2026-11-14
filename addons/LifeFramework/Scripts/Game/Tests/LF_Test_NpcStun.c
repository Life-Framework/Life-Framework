// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_NpcStun`: expecting the farther NPC failed `of two NPCs on the line the nearer takes the dart (expected 0, got 1)`; restored, green.
// tier: LOGIC
//! The taser on a town NPC (#922): which NPC a dart along a line hits. Pure geometry, no world.
class LF_Test_NpcStun : LF_Test
{
	override string GetName()
	{
		return "police/npc-stun";
	}

	override void Run(LF_TestContext ctx)
	{
		vector from = "0 1.5 0";
		vector forward = "0 0 1";
		array<vector> centres = {};

		ctx.Equal(-1, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "nobody to hit");

		centres.Insert("0 1.2 6");
		ctx.Equal(0, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "an NPC dead ahead in reach is hit");

		centres.Clear();
		centres.Insert("0 1.2 11");
		ctx.Equal(-1, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "an NPC beyond the weapon's reach is not");

		centres.Clear();
		centres.Insert("2 1.2 6");
		ctx.Equal(-1, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "an NPC two metres off the line is not");

		centres.Clear();
		centres.Insert("0 1.2 -4");
		ctx.Equal(-1, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "an NPC behind the shooter is not");

		centres.Clear();
		centres.Insert("0.2 1.2 8");
		centres.Insert("0 1.2 3");
		centres.Insert("3 1.2 2");
		ctx.Equal(1, LF_NpcStun.PickTarget(from, forward, 10, 0.6, centres), "of two NPCs on the line the nearer takes the dart");
	}
}
