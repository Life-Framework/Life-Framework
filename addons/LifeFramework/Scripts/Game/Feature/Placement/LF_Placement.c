//------------------------------------------------------------------------------------------------
//! Placement foundation (#927; design in docs/roadmap/building-foundation.md). The verdict one spot
//! gets, which the client's ghost shows and the server's confirm enforces, and the ghost itself.
//! A feature (the spike strip first, #928) measures a candidate spot into an LF_PlacementProbe; the
//! same probe and the same Judge run on both sides, so the colour a player sees is the answer the
//! server gives. Only VALID places. Nothing here persists.
//------------------------------------------------------------------------------------------------

enum LF_EPlacementVerdict
{
	VALID,
	NO_GROUND,
	TOO_STEEP,
	BLOCKED
}

//------------------------------------------------------------------------------------------------
//! What a feature measured at one candidate spot.
class LF_PlacementProbe
{
	//! Ground was found under every sample point.
	bool m_bGround;
	//! Lowest and highest ground height over the footprint.
	float m_fLowest;
	float m_fHighest;
	//! What stands inside the footprint's clearance box; empty when it is clear.
	string m_sBlocker;
}

//------------------------------------------------------------------------------------------------
//! The pure rules (LOGIC-tested by LF_Test_Placement) and the world probes both sides share.
class LF_PlacementRules
{
	static const string FEATURE = "Placement";

	//------------------------------------------------------------------------------------------------
	//! The verdict for a probe: ground first (nothing to stand on), then the step along the
	//! footprint (a kerb, a slope), then anything standing in it.
	static LF_EPlacementVerdict Judge(LF_PlacementProbe probe, float maxStep)
	{
		if (!probe || !probe.m_bGround)
			return LF_EPlacementVerdict.NO_GROUND;

		if (probe.m_fHighest - probe.m_fLowest > maxStep)
			return LF_EPlacementVerdict.TOO_STEEP;

		if (!probe.m_sBlocker.IsEmpty())
			return LF_EPlacementVerdict.BLOCKED;

		return LF_EPlacementVerdict.VALID;
	}

	//------------------------------------------------------------------------------------------------
	//! The reason for a log line or a refusal; empty for VALID.
	static string Reason(LF_EPlacementVerdict verdict, LF_PlacementProbe probe, float maxStep)
	{
		if (verdict == LF_EPlacementVerdict.NO_GROUND)
			return "no ground under it";

		if (verdict == LF_EPlacementVerdict.TOO_STEEP)
			return string.Format("the ground rises %1 m along it (most %2)", probe.m_fHighest - probe.m_fLowest, maxStep);

		if (verdict == LF_EPlacementVerdict.BLOCKED)
			return string.Format("%1 is in the way", probe.m_sBlocker);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Short lower-case name of a verdict for logs and the harness ("valid", "blocked").
	static string Name(LF_EPlacementVerdict verdict)
	{
		// ToLower changes the string in place.
		string name = typename.EnumToString(LF_EPlacementVerdict, verdict);
		name.ToLower();
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the first entity inside the oriented box `mins`..`maxs` in `transform`'s
	//! frame (the vanilla item placement's space check, SCR_ItemPlacementComponent.ValidatePlacement),
	//! described for a log line; empty when the box is clear. Terrain is not an entity, so a box
	//! lifted off the ground only meets what stands on it.
	static string FindBlocker(vector transform[4], vector mins, vector maxs, array<IEntity> exclude)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "";

		TraceOBB trace = new TraceOBB();
		Math3D.MatrixIdentity3(trace.Mat);
		trace.Mat[0] = transform[0];
		trace.Mat[1] = transform[1];
		trace.Mat[2] = transform[2];
		trace.Start = transform[3];
		trace.Mins = mins;
		trace.Maxs = maxs;
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (exclude)
			trace.ExcludeArray = exclude;

		world.TracePosition(trace, null);
		if (!trace.TraceEnt)
			return "";

		return Describe(trace.TraceEnt);
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: whether a thing may lie on `surface` (the entity a ground probe hit, null
	//! for terrain). Not on anything that moves (a car, a dropped item: dynamic physics on it or on
	//! its root, the vanilla SCR_ItemPlacementComponent.ValidateEntity rule), nor on an entity with
	//! one of the `notGround` components (a placed thing of the same kind). Returns what it would
	//! lie on, described for a log line, or empty when the surface is ground.
	static string SurfaceBlocker(IEntity surface, array<typename> notGround)
	{
		if (!surface)
			return "";

		array<IEntity> entities = {surface, surface.GetRootParent()};
		foreach (IEntity entity : entities)
		{
			if (!entity)
				continue;

			Physics physics = entity.GetPhysics();
			if (physics && physics.IsDynamic())
				return Describe(entity);

			if (!notGround)
				continue;

			foreach (typename component : notGround)
			{
				if (entity.FindComponent(component))
					return Describe(entity);
			}
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! An entity for a log line: its prefab file name, else its class.
	static string Describe(IEntity entity)
	{
		if (!entity)
			return "nothing";

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData)
		{
			string prefab = prefabData.GetPrefabName();
			if (!prefab.IsEmpty())
				return FilePath.StripExtension(FilePath.StripPath(prefab));
		}

		return entity.ClassName();
	}
}

//------------------------------------------------------------------------------------------------
//! Client: one ghost of a prefab's mesh in Conflict's build colours (the vanilla CanBuild and
//! CannotBuild materials SCR_ItemPlacementComponent uses), spawned on this machine only and never
//! replicated. One at a time: showing another prefab replaces it.
class LF_PlacementGhost
{
	static const ResourceName VALID_MATERIAL = "{56EBF5038622AC95}Assets/Conflict/CanBuild.emat";
	static const ResourceName INVALID_MATERIAL = "{14A9DCEA57D1C381}Assets/Conflict/CannotBuild.emat";

	protected static IEntity s_Ghost;
	protected static ResourceName s_sPrefab;
	protected static bool s_bValidShown;

	//------------------------------------------------------------------------------------------------
	//! Shows (or moves) the ghost of `prefab` at `transform`, green when valid, red when not.
	//! \return false with an error logged when the prefab has no mesh to show.
	static bool Show(ResourceName prefab, vector transform[4], bool valid)
	{
		if (!s_Ghost || s_sPrefab != prefab)
		{
			Hide();
			if (!Spawn(prefab))
				return false;

			s_bValidShown = !valid;
		}

		s_Ghost.SetWorldTransform(transform);
		s_Ghost.Update();
		if (valid != s_bValidShown)
		{
			if (valid)
				SCR_Global.SetMaterial(s_Ghost, VALID_MATERIAL);
			else
				SCR_Global.SetMaterial(s_Ghost, INVALID_MATERIAL);

			s_bValidShown = valid;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsShown()
	{
		return s_Ghost != null;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetGhost()
	{
		return s_Ghost;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (s_Ghost)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Ghost);

		s_Ghost = null;
		s_sPrefab = ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Spawn(ResourceName prefab)
	{
		BaseContainer meshComponent = LF_Utils.GetPrefabMeshComponent(prefab);
		ResourceName meshPath;
		if (meshComponent)
			meshComponent.Get("Object", meshPath);

		Resource mesh;
		if (!meshPath.IsEmpty())
			mesh = Resource.Load(meshPath);

		if (!mesh || !mesh.IsValid())
		{
			LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("ghost skipped: %1 has no mesh to show", prefab));
			return false;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		s_Ghost = GetGame().SpawnEntity(GenericEntity, world);
		if (!s_Ghost)
		{
			LF_Debug.Error(LF_PlacementRules.FEATURE, "ghost skipped: the entity did not spawn");
			return false;
		}

		s_Ghost.SetObject(mesh.GetResource().ToVObject(), "");
		s_sPrefab = prefab;
		return true;
	}
}
