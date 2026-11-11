// red-proof: 2026-11-11 `tools/cli test --no-build --only LF_Test_SpawnHub` with the `if (!hubEnabled)` refusal in LF_SpawnHubRules.Check disabled failed `- a game mode that is not a playtest refuses (expected 1, got 0)` and `- a disabled hub does not move the body (expected 1, got 0)`. Restored; green.
// tier: WORLD
//! Spawn hub (#607): the shipped config loads with the areas the playtest notes name, every row has
//! a unique id and a name string, a travel request is refused when the game mode is not a
//! playtest mode, for an unknown id and for no body, and a good request puts a body at the row.
class LF_Test_SpawnHub : LF_Test
{
	override string GetName()
	{
		return "spawnhub/travel";
	}

	override void Run(LF_TestContext ctx)
	{
		// Config load.
		LF_SpawnHubConfig config = LF_SpawnHubConfig.Get();
		ctx.NotNull(config, "the spawn hub config loads");
		array<ref LF_SpawnHubPoint> points = LF_SpawnHubConfig.Points();
		ctx.True(points.Count() >= 20, "the config lists the playtest areas (got " + points.Count().ToString() + ")");
		array<string> required = {"spawn", "police", "bank", "jail", "fence", "farm", "orchard", "mining", "garage", "car_lot", "chop_shop", "fuel_station", "hospital", "market", "quarry"};
		foreach (string id : required)
		{
			ctx.NotNull(LF_SpawnHubConfig.Find(id), "the config has a row for " + id);
		}

		ctx.True(LF_SpawnHubConfig.Find("no_such_area") == null, "an unknown id finds no row");
		foreach (LF_SpawnHubPoint point : points)
		{
			string key = LF_SpawnHubRules.NameKey(point.m_sId);
			ctx.True(WidgetManager.Translate(key) != key, "row " + point.m_sId + " has a name string");
			ctx.True(point.m_vPosition[0] > 0 && point.m_vPosition[2] > 0, "row " + point.m_sId + " has a position");
		}

		// Pure rules.
		ctx.Equal(LF_ESpawnHubCheck.OK, LF_SpawnHubRules.Check(true, true, true), "a playtest request for a known point passes");
		ctx.Equal(LF_ESpawnHubCheck.DISABLED, LF_SpawnHubRules.Check(false, true, true), "a game mode that is not a playtest refuses");
		ctx.Equal(LF_ESpawnHubCheck.NO_BODY, LF_SpawnHubRules.Check(true, false, true), "no body refuses");
		ctx.Equal(LF_ESpawnHubCheck.UNKNOWN_POINT, LF_SpawnHubRules.Check(true, true, false), "an unknown point refuses");
		vector onGround = LF_SpawnHubRules.Destination("1 0 2", 12);
		vector written = LF_SpawnHubRules.Destination("1 5 2", 12);
		ctx.EqualFloat(12.2, onGround[1], 0.001, "a Y of 0 stands on the ground");
		ctx.EqualFloat(5, written[1], 0.001, "a written Y is kept");

		// The debug game mode is a playtest mode.
		ctx.True(LF_SpawnHub.IsEnabled(), "the DebugWorld game mode opts into the hub");

		// Server path with a real body.
		IEntity body = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, vector.Zero);
		ctx.NotNull(body, "fixture: a body spawns");
		if (!body)
			return;

		LF_SpawnHubPoint bank = LF_SpawnHubConfig.Find("bank");
		ctx.Equal(LF_ESpawnHubCheck.DISABLED, LF_SpawnHub.Serve(body, "bank", false), "a disabled hub does not move the body");
		ctx.EqualFloat(0, body.GetOrigin()[0], 0.01, "the refused body stayed put");
		ctx.Equal(LF_ESpawnHubCheck.UNKNOWN_POINT, LF_SpawnHub.Serve(body, "no_such_area", true), "an unknown id does not move the body");
		ctx.EqualFloat(0, body.GetOrigin()[0], 0.01, "the unknown-id body stayed put");
		ctx.Equal(LF_ESpawnHubCheck.NO_BODY, LF_SpawnHub.Serve(null, "bank", true), "no body is refused");
		ctx.Equal(LF_ESpawnHubCheck.OK, LF_SpawnHub.Serve(body, "bank", true), "a good request is served");
		ctx.EqualFloat(bank.m_vPosition[0], body.GetOrigin()[0], 0.05, "the body stands at the bank x");
		ctx.EqualFloat(bank.m_vPosition[2], body.GetOrigin()[2], 0.05, "the body stands at the bank z");
		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}
}
