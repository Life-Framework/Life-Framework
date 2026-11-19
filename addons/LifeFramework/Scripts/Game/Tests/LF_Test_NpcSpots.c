// red-proof: batched `tools\cli test --no-build --red LF_Test_NpcSpots` (2026-11-19): RedProof() skips the free-spot search and the settle, so the arrival stays on the occupied spot and `the arrival takes a spot clear of the one who stands there` and `a body standing inside another is settled` fail.
//! #3831 "two people spawned inside each other": one spot holds one character. LF_NpcSpots is the rule every spawner
//! asks (residents, scene walkers, workplace NPCs, buyers): the pure search gives the wanted spot when it is clear and
//! else the nearest clear one around it; the world side counts every character in reach; a body that arrives on its
//! own inside another is settled off it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NpcSpots : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "npc/spots";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunPure(ctx);

		LF_NpcSpots.ResetStatics();
		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		if (first && second)
			RunWorld(ctx, first, second);

		LF_NpcSpots.ResetStatics();
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPure(LF_TestContext ctx)
	{
		vector wanted = "100 10 100";
		vector right = "1 0 0";
		vector forward = "0 0 1";
		array<vector> taken = {};

		bool found;
		vector at = LF_NpcSpots.PickFree(wanted, right, forward, taken, found);
		ctx.True(found, "an empty spot is free");
		ctx.EqualFloat(0, vector.DistanceXZ(at, wanted), 0.01, "a free spot is taken as it is");

		taken.Insert(wanted);
		at = LF_NpcSpots.PickFree(wanted, right, forward, taken, found);
		ctx.True(found, "a taken spot has a free one beside it");
		ctx.True(vector.DistanceXZ(at, wanted) >= LF_NpcSpots.MIN_M, "the free spot is clear of the body on the wanted one");
		ctx.True(vector.DistanceXZ(at, wanted) <= LF_NpcSpots.STEP_M + 0.01, "the nearest free spot is one step out");

		// Two on it, then a ring: the third stays within reach and clear of both.
		taken.Insert(at);
		vector third = LF_NpcSpots.PickFree(wanted, right, forward, taken, found);
		ctx.True(found, "a third arrival still finds a spot");
		ctx.True(LF_NpcSpots.IsFree(third, taken), "the third spot is clear of both");

		ctx.False(LF_NpcSpots.IsFree("100.5 10 100", taken), "0.5 m from a body is not free");
		ctx.True(LF_NpcSpots.IsFree("100 10 102.9", taken), "2.9 m from a body is free");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorld(LF_TestContext ctx, IEntity first, IEntity second)
	{
		// Put the second on the first: the pair the maintainer saw (the fixture's spawn points are random).
		second.SetOrigin(first.GetOrigin());
		ctx.True(vector.DistanceXZ(second.GetOrigin(), first.GetOrigin()) < LF_NpcSpots.MIN_M, "the fixture put the second character on the first");
		vector spot = first.GetOrigin();
		ctx.True(LF_NpcSpots.IsOccupied(spot, second.GetID()), "a character stands on the spawn point");

		vector wanted = spot;
		if (!RedProof())
			wanted = LF_NpcSpots.Free(spot, 0, "test arrival", second.GetID());

		ctx.True(vector.DistanceXZ(wanted, first.GetOrigin()) >= LF_NpcSpots.MIN_M, "the arrival takes a spot clear of the one who stands there");

		// A body that arrived inside another is moved off it.
		bool moved;
		if (!RedProof())
			moved = LF_NpcSpots.Settle(second, "test body");

		ctx.True(moved, "a body standing inside another is settled");
		// The teleport lands next frame; a body that stands alone is left where it is.
		second.SetOrigin(first.GetOrigin() + "10 0 0");
		ctx.False(LF_NpcSpots.Settle(second, "test body"), "a body that stands alone stays where it is");
		ctx.True(LF_NpcSpots.MovedCount() > 0, "the move is counted");
	}
}
