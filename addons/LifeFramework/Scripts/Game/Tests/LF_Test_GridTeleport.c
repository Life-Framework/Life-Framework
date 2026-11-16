// red-proof: 2026-11-16 `cli test --no-build --red LF_Test_GridTeleport` (GridWorld): RED-OK, the Serve act skipped fails "the player stands within 3 m of the frame spawn (129.275 m)"; green with the act on.
// tier: WORLD
//! Grid teleport (#1813): `grid frame` puts the caller within 3 m of the frame spawn, an unknown slug
//! logs LF_Debug.Warn and moves nothing, a caller without the dev gate moves nothing, and a world with
//! no grid cells refuses (the suite's world 2, GridWorld, has them; the check is skipped elsewhere).
class LF_Test_GridTeleport : LF_Test
{
	override string GetName()
	{
		return "grid/teleport";
	}

	override void Run(LF_TestContext ctx)
	{
		bool hasCells = LF_GridTeleport.WorldHasCells();
		IEntity body = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, vector.Zero);
		ctx.NotNull(body, "fixture: a body spawns");
		if (!body)
			return;

		if (!hasCells)
		{
			// DebugWorld and the pack town place no grid cells: the command refuses and moves nothing.
			int before = LF_GridTeleport.WarningCount();
			ctx.Equal(LF_EGridTeleport.NO_GRID, LF_GridTeleport.Serve(body, "frame", true), "a world with no grid cells refuses");
			ctx.Equal(before + 1, LF_GridTeleport.WarningCount(), "the refusal is logged as a warning");
			ctx.EqualFloat(0, body.GetOrigin()[0], 0.01, "the body stayed put");
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return;
		}

		vector frameSpawn;
		ctx.True(LF_Places.FindOrigin(LF_GridTeleport.KIND, "frame", frameSpawn), "the frame places a GridCell spawn named frame");

		// The request: `grid frame`.
		if (!RedProof())
			ctx.Equal(LF_EGridTeleport.OK, LF_GridTeleport.Serve(body, "frame", true), "grid frame is served");

		float away = vector.Distance(body.GetOrigin(), frameSpawn);
		ctx.True(away <= 3.0, string.Format("the player stands within 3 m of the frame spawn (%1 m)", away));

		// Typed by hand: spaces and capitals do not matter.
		SCR_EntityHelper.DeleteEntityAndChildren(body);
		body = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, vector.Zero);
		ctx.NotNull(body, "fixture: a second body spawns");
		if (!body)
			return;

		ctx.Equal(LF_EGridTeleport.OK, LF_GridTeleport.Serve(body, "  Frame ", true), "a slug typed with a capital and spaces is served");
		ctx.True(vector.Distance(body.GetOrigin(), frameSpawn) <= 3.0, "the tidied slug reaches the same spawn");

		// An unknown slug: a warning and the body stays where it was.
		vector where = body.GetOrigin();
		int warned = LF_GridTeleport.WarningCount();
		ctx.Equal(LF_EGridTeleport.UNKNOWN_CELL, LF_GridTeleport.Serve(body, "no-such-cell", true), "an unknown slug is refused");
		ctx.Equal(warned + 1, LF_GridTeleport.WarningCount(), "the unknown slug logs LF_Debug.Warn");
		ctx.EqualFloat(0, vector.Distance(where, body.GetOrigin()), 0.01, "the unknown slug did not move the player");

		// No dev gate: the server refuses before it looks at the slug.
		IEntity far = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, Vector(5, 0, 5));
		if (far)
		{
			vector farAt = far.GetOrigin();
			ctx.Equal(LF_EGridTeleport.NOT_ALLOWED, LF_GridTeleport.Serve(far, "frame", false), "a player without the dev gate is refused");
			ctx.EqualFloat(0, vector.Distance(farAt, far.GetOrigin()), 0.01, "the refused player did not move");
			SCR_EntityHelper.DeleteEntityAndChildren(far);
		}

		ctx.Equal(LF_EGridTeleport.NO_BODY, LF_GridTeleport.Serve(null, "frame", true), "a caller with no body is refused");
		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}
}
