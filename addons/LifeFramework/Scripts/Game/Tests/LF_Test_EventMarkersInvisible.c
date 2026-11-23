// red-proof: 2026-11-23 on the unfixed prefabs (the waypoint, spawn and target markers inherited ConeTraffic_01_red) `--only LF_Test_EventMarkersInvisible` failed "<marker> carries no mesh" for all three; green after they stopped inheriting the cone.
//! events/markers-invisible: an event marker (waypoint, spawn, target, item) is data for the scene, not a prop. A marker that inherits a
//! traffic cone put a red cone on every road and square the island's scenes use (the first-2-hours re-review saw them by the market stall).
//! Reads the prefab's container chain instead of spawning one: a marker with no scene id logs an error on spawn.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventMarkersInvisible : LF_Test
{
	protected static const string MARKER_DIR = "Prefabs/Systems/Events/";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/markers-invisible";
	}

	//------------------------------------------------------------------------------------------------
	//! The class name of the first drawn component in the prefab or one of its ancestors, or "" when none draws.
	protected string FindMesh(BaseContainer prefab)
	{
		while (prefab)
		{
			BaseContainerList comps = prefab.GetObjectArray("components");
			if (comps)
			{
				for (int i = 0, n = comps.Count(); i < n; i++)
				{
					BaseContainer comp = comps.Get(i);
					if (comp && comp.GetClassName() == "MeshObject")
						return prefab.GetResourceName();
				}
			}

			prefab = prefab.GetAncestor();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, string file)
	{
		Resource res = Resource.Load(MARKER_DIR + file);
		ctx.True(res && res.IsValid(), file + " loads");
		if (!res || !res.IsValid())
			return;

		BaseResourceObject obj = res.GetResource();
		BaseContainer prefab;
		if (obj)
			prefab = obj.ToBaseContainer();

		ctx.True(prefab != null, file + " has a container");
		if (!prefab)
			return;

		string mesh = FindMesh(prefab);
		ctx.True(mesh.IsEmpty(), file + " carries no mesh (found one in " + mesh + ")");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Check(ctx, "LF_EventMarker_Waypoint.et");
		Check(ctx, "LF_EventMarker_Spawn.et");
		Check(ctx, "LF_EventMarker_Target.et");
		Check(ctx, "LF_EventMarker_Item.et");
	}
}
