// red-proof: observed 2026-11-15 with `tools\cli test --no-build --red LF_Test_StoreStackRules,LF_Test_StoreFoldedStack`: RED-OK store/stack-rules (the hook skips ParseSlotYaws and StackMoves) failed `- one yaw per point (expected 3, got 0)` and `- three copies move (expected 3, got 0)`; green with the hook off.

// tier: LOGIC
//! The pure rules of a folded pile on a store shelf: a slot's yaw is the fourth number of its point,
//! each run of N points is one pile, and a gap left by a copy taken from the middle closes by moving
//! the copies above it down, never into another pile. No world, no entity.
class LF_Test_StoreStackRules : LF_Test
{
	override string GetName()
	{
		return "store/stack-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// A fourth number is the yaw; a point with three has none. The points themselves read as before.
		string text = "1 2 3 | 4 5 6 30 | | 7 8 9 -15.5";
		array<vector> slots = {};
		array<float> yaws = {};
		LF_StoreRules.ParseSlots(text, slots);
		if (!RedProof())
			LF_StoreRules.ParseSlotYaws(text, yaws);

		ctx.Equal(3, slots.Count(), "three points, the empty piece skipped");
		if (slots.Count() == 3)
			ctx.True(vector.Distance(slots[1], "4 5 6") < 0.001, string.Format("a point with a yaw keeps its position (got %1)", slots[1]));

		ctx.Equal(3, yaws.Count(), "one yaw per point");
		if (yaws.Count() == 3)
		{
			ctx.True(yaws[0] == LF_StoreRules.NO_YAW, "a point of three numbers has no yaw");
			ctx.True(Math.AbsFloat(yaws[1] - 30) < 0.001, string.Format("the fourth number is the yaw (got %1)", yaws[1]));
			ctx.True(Math.AbsFloat(yaws[2] + 15.5) < 0.001, string.Format("a negative fractional yaw reads (got %1)", yaws[2]));
		}

		// Piles of four: the bottom of slot 6 is slot 4. No piles: every slot is its own.
		ctx.Equal(4, LF_StoreRules.StackBase(6, 4), "slot 6 in piles of four stands on slot 4");
		ctx.Equal(6, LF_StoreRules.StackBase(6, 0), "no piles: a slot is its own bottom");
		ctx.Equal(6, LF_StoreRules.StackBase(6, 1), "piles of one are no piles");

		// Two piles of four. The middle of the first was taken: the two above move down one each.
		// The second pile lost its bottom: its copy moves down to the bottom of its own pile, never
		// into the first.
		array<bool> occupied = {true, false, true, true, false, true, false, false};
		array<int> from = {};
		array<int> to = {};
		if (!RedProof())
			LF_StoreRules.StackMoves(occupied, 4, from, to);

		ctx.Equal(3, from.Count(), "three copies move");
		if (from.Count() == 3 && to.Count() == 3)
		{
			ctx.True(from[0] == 2 && to[0] == 1, string.Format("slot 2 drops to 1 (got %1 to %2)", from[0], to[0]));
			ctx.True(from[1] == 3 && to[1] == 2, string.Format("then slot 3 to 2 (got %1 to %2)", from[1], to[1]));
			ctx.True(from[2] == 5 && to[2] == 4, string.Format("the second pile closes in itself (got %1 to %2)", from[2], to[2]));
		}

		// A full pile, a pile taken from the top, and no piles at all: nothing moves.
		array<bool> tidy = {true, true, true, false};
		LF_StoreRules.StackMoves(tidy, 4, from, to);
		ctx.Equal(0, from.Count(), "a pile taken from the top stays as it is");
		LF_StoreRules.StackMoves(occupied, 0, from, to);
		ctx.Equal(0, from.Count(), "a line with no piles never moves a copy");
	}
}
