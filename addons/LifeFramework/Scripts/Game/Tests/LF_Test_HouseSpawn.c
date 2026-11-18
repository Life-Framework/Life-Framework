// red-proof: Observed 2026-11-18 with the same batched `--red` run: RED PROOF OK, the blocked point is not skipped with the clearance choice off. Green on the real code.
//! spawn/house-point (#2652): a house's authored spawn point with a wall on it is skipped for the next
//! clear one; with none clear the player is put outside the door; with none clear and no outside point
//! there is no point and the login takes the role spawn.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HouseSpawn : LF_Test
{
	protected static const ResourceName WALL = "{419130031F0009C9}Prefabs/Structures/Walls/Brick/BrickWall_01/BrickWall_01_4m.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/house-point";
	}

	//------------------------------------------------------------------------------------------------
	protected vector OnGround(float x, float z)
	{
		vector spot = Vector(x, 0, z);
		BaseWorld world = GetGame().GetWorld();
		if (world)
			spot[1] = world.GetSurfaceY(x, z);

		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Choose(array<vector> inside, vector outside, bool hasOutside, out vector spot, int start)
	{
		if (RedProof())
		{
			// The unguarded version: take the first authored point, clear or not.
			spot = inside[0];
			return true;
		}

		return LF_HouseSpawnComponent.Choose(inside, outside, hasOutside, spot, start);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector blocked = OnGround(300, 300);
		vector clear = OnGround(310, 300);
		vector outside = OnGround(320, 300);

		IEntity wall = LF_Utils.SpawnEntityPrefab(WALL, blocked);
		ctx.NotNull(wall, "the wall that blocks the first point spawns");
		if (!wall)
			return;

		ctx.False(LF_HouseSpawnComponent.IsClear(blocked), "a point with a wall on it is not clear");
		ctx.True(LF_HouseSpawnComponent.IsClear(clear), "an open point is clear");

		array<vector> both = {blocked, clear};
		vector spot;
		ctx.True(Choose(both, outside, true, spot, 0), "a set with one clear point gives a spot");
		ctx.True(spot == clear, "the blocked point is skipped for the next clear one (walk starts at the blocked one)");
		ctx.True(Choose(both, outside, true, spot, 1), "a set with one clear point gives a spot from either start");
		ctx.True(spot == clear, "the clear point wins when the walk starts on it");

		array<vector> onlyBlocked = {blocked};
		ctx.True(Choose(onlyBlocked, outside, true, spot, 0), "no clear point still gives a spot when there is an outside point");
		ctx.True(spot == outside, "no clear point puts the player outside the door");
		ctx.False(Choose(onlyBlocked, outside, false, spot, 0), "no clear point and no outside point gives no spot");

		LF_TestCharacterFixture.Delete(wall);
	}
}
