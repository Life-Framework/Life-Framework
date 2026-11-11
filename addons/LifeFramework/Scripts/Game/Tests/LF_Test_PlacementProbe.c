// tier: WORLD
// red-proof: 2026-11-11, LF_PlacementAudit.RayLine reporting `d = len` instead of the traced distance: `--only LF_Test_PlacementProbe` failed "a ray down stops on the table top (expected 0.500393, got 1.90127)" and "a ray from the side stops at the table's edge (expected 1, got 1.60437)". Restored, green.
//! The probe ray behind `tools\cli place` (LF_PlacementAudit.RayLine, tools/place/place.mjs): the
//! placement tool asks the test server where a surface's top and a wall's face really are instead of
//! trusting mesh bounds. On a table spawned on open ground south of the DebugWorld town (310, 100):
//! a ray down finds the table top and names the table, a ray from the side stops at its edge, and a
//! ray through open air reports its full length and no hit.
class LF_Test_PlacementProbe : LF_Test
{
	protected static const ResourceName TABLE = "{622F9989DDC4865B}PrefabLibrary/Props/Civilian/Recreation/TableRecreation_01/TableRecreation_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "agent-harness/placement-probe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		float groundY = world.GetSurfaceY(310, 100);
		IEntity table = LF_Utils.SpawnEntityPrefab(TABLE, Vector(310, groundY, 100));
		ctx.NotNull(table, "the table spawned");
		if (!table)
			return;

		vector tmin;
		vector tmax;
		table.GetWorldBounds(tmin, tmax);
		float cx = (tmin[0] + tmax[0]) * 0.5;
		float cz = (tmin[2] + tmax[2]) * 0.5;
		string topLabel;
		float topY = LF_PlacementAudit.SupportAt(null, cx, cz, tmax[1] + 0.5, topLabel);

		string down = LF_PlacementAudit.RayLine("down", Vector(cx, tmax[1] + 0.5, cz), Vector(cx, tmin[1] - 0.5, cz));
		LF_Debug.Info("PlaceAudit", "probe " + down);
		ctx.True(down.StartsWith("ray down "), "the line starts with its id (" + down + ")");
		ctx.EqualFloat(tmax[1] + 0.5 - topY, Field(down, "d").ToFloat(), 0.03, "a ray down stops on the table top");
		ctx.True(Field(down, "root") == LF_PlacementAudit.Label(table), "it names the table (root=" + Field(down, "root") + ")");
		ctx.EqualFloat(groundY, Field(down, "sy").ToFloat(), 0.05, "it reports the terrain height under the hit");

		// From 1 m outside the long side, just under the top's upper face: the table's edge is the first surface.
		string side = LF_PlacementAudit.RayLine("side", Vector(tmin[0] - 1, topY - 0.02, cz), Vector(cx, topY - 0.02, cz));
		LF_Debug.Info("PlaceAudit", "probe " + side);
		ctx.EqualFloat(1.0, Field(side, "d").ToFloat(), 0.12, "a ray from the side stops at the table's edge");

		string clear = LF_PlacementAudit.RayLine("clear", Vector(tmin[0] - 1, tmax[1] + 3, cz), Vector(tmax[0] + 1, tmax[1] + 3, cz));
		LF_Debug.Info("PlaceAudit", "probe " + clear);
		ctx.True(Field(clear, "hit") == "none", "a ray through open air hits nothing (hit=" + Field(clear, "hit") + ")");
		ctx.EqualFloat(Field(clear, "len").ToFloat(), Field(clear, "d").ToFloat(), 0.001, "and reports its full length");

		SCR_EntityHelper.DeleteEntityAndChildren(table);
	}

	//------------------------------------------------------------------------------------------------
	//! The value of `key=` in a key=value line, or "".
	protected static string Field(string line, string key)
	{
		array<string> parts = {};
		line.Split(" ", parts, true);
		foreach (string part : parts)
		{
			if (part.StartsWith(key + "="))
				return part.Substring(key.Length() + 1, part.Length() - key.Length() - 1);
		}

		return "";
	}
}
