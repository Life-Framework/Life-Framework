// LF_AgentHarness_PerfStress.c - harness `perfstress`: counts and a synthetic load for the building
// performance budget (docs/architecture/performance.md "Buildings").
//
//   perfstress count <x> <y> <z> <r>   entities, lights, shadow casters, physics bodies, replicated
//                                      and per-frame entities inside the sphere (this client's world)
//   perfstress count <name> <r>        the same around a named world entity, with its origin and yaw
//   perfstress at <x> <y> <z> <p|s>    the stress centre (floor height) and light type: point or spot
//   perfstress spawn <L> <S> <K>       L local lights over a 10 x 10 m square 3 m up, the first S casting
//                                      shadows, and K local cardboard boxes on the floor (16 x 16 a layer)
//   perfstress server <K>              the server spawns K local boxes at the centre (0 removes them):
//                                      its frame cost, read from -logStats, never sent to clients
//   perfstress clear                   removes this client's lights and boxes
//
// Everything spawned is local (never replicated) and only reachable through the harness. Read the
// cost with `perf <seconds> <label>` on a session with `--levers uncapped,slog`.

class LF_PerfStress
{
	static const string FEATURE = "PerfStress";
	static const ResourceName PROP = "{1284F9C5D25E263E}Prefabs/Props/Crates/CardboardBox/CardboardBox_01.et";
	static const float AREA_M = 10; // lf-fixed: the stress room's side, a large shop floor
	static const float LIGHT_HEIGHT_M = 3; // lf-fixed: ceiling height of the stress room
	static const float PROP_STEP_M = 0.6; // lf-fixed: box spacing, one box per cell
	static const int PROPS_PER_ROW = 16; // lf-fixed: 16 x 16 boxes a layer over the 10 m square
	static const int MAX_SPAWN = 5000; // lf-fixed: harness probe bound

	protected static vector s_vCentre;
	protected static bool s_bSpot = true;
	protected static ref array<EntityID> s_aSpawned = {};

	//------------------------------------------------------------------------------------------------
	static void SetCentre(vector centre, bool spot)
	{
		s_vCentre = centre;
		s_bSpot = spot;
	}

	//------------------------------------------------------------------------------------------------
	static vector GetCentre()
	{
		return s_vCentre;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes this machine's stress entities.
	static void Clear()
	{
		DeleteAll(s_aSpawned);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes each entity that still exists and empties the list.
	static void DeleteAll(array<EntityID> ids)
	{
		BaseWorld world = GetGame().GetWorld();
		foreach (EntityID id : ids)
		{
			IEntity e;
			if (world)
				e = world.FindEntityByID(id);
			if (e)
				SCR_EntityHelper.DeleteEntityAndChildren(e);
		}
		ids.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The ceiling grid position of light i of n over the square.
	static vector LightPos(vector centre, int i, int n)
	{
		int side = Math.Ceil(Math.Sqrt(n));
		if (side < 1)
			side = 1;
		float step = AREA_M / side;
		int row = i / side;
		int col = i % side;
		float x = centre[0] - AREA_M * 0.5 + step * (col + 0.5);
		float z = centre[2] - AREA_M * 0.5 + step * (row + 0.5);
		return Vector(x, centre[1] + LIGHT_HEIGHT_M, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Floor position of box i: 16 x 16 a layer, layers stacked 0.6 m apart.
	static vector PropPos(vector centre, int i)
	{
		int perLayer = PROPS_PER_ROW * PROPS_PER_ROW;
		int layer = i / perLayer;
		int k = i % perLayer;
		int row = k / PROPS_PER_ROW;
		int col = k % PROPS_PER_ROW;
		float x = centre[0] - PROP_STEP_M * PROPS_PER_ROW * 0.5 + PROP_STEP_M * col;
		float z = centre[2] - PROP_STEP_M * PROPS_PER_ROW * 0.5 + PROP_STEP_M * row;
		return Vector(x, centre[1] + PROP_STEP_M * layer, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns lights (the first `shadows` cast shadows) and boxes. Returns what was made.
	static string Spawn(int lights, int shadows, int props)
	{
		if (lights < 0 || props < 0 || lights + props > MAX_SPAWN)
			return "bad_count";

		Clear();
		for (int i = 0; i < lights; i++)
		{
			LightFlags flags = 0;
			if (i < shadows)
				flags = LightFlags.CASTSHADOW;
			LightEntity light;
			if (s_bSpot)
			{
				// The fitted shops' ceiling spot: radius 7, 140 degrees, LV 5.5, warm white.
				light = LightEntity.CreateLight(LightType.SPOT, flags, 7, Color.FromRGBA(255, 219, 163, 255), 5.5, LightPos(s_vCentre, i, lights), "0 -1 0");
				if (light)
					light.SetConeAngle(140);
			}
			else
			{
				light = LightEntity.CreateLight(LightType.POINT, flags, 6, Color.FromRGBA(255, 219, 163, 255), 5, LightPos(s_vCentre, i, lights));
			}
			if (light)
				s_aSpawned.Insert(light.GetID());
		}

		Resource prop = Resource.Load(PROP);
		if (props > 0 && (!prop || !prop.IsValid()))
		{
			LF_Debug.Error(FEATURE, "prop prefab did not load: " + PROP);
			return "no_prop";
		}
		for (int j = 0; j < props; j++)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = PropPos(s_vCentre, j);
			IEntity e = GetGame().SpawnEntityPrefabLocal(prop, GetGame().GetWorld(), params);
			if (e)
				s_aSpawned.Insert(e.GetID());
		}

		string made = string.Format("lights=%1 shadows=%2 props=%3 spawned=%4", lights, Math.Min(shadows, lights), props, s_aSpawned.Count());
		LF_Debug.Info(FEATURE, made);
		return made;
	}
}

//------------------------------------------------------------------------------------------------
//! One sphere count: what a building or a street puts in the world.
class LF_PerfStressCount : Managed
{
	int m_iEntities;
	int m_iStatic;
	int m_iWithMesh;
	int m_iLights;
	int m_iLightsOn;
	int m_iShadow;
	int m_iPhysics;
	int m_iPhysicsDynamic;
	int m_iReplicated;
	int m_iFrame;
	protected vector m_vCentre;
	protected float m_fRadiusSq;

	//------------------------------------------------------------------------------------------------
	string Run(vector centre, float radius)
	{
		m_vCentre = centre;
		m_fRadiusSq = radius * radius;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "no_world";

		world.QueryEntitiesBySphere(centre, radius, AddRoot, null, EQueryEntitiesFlags.ALL);
		return string.Format("entities=%1 static=%2 mesh=%3 lights=%4 lightsOn=%5 shadow=%6 physics=%7 physicsDynamic=%8 rpl=%9", m_iEntities, m_iStatic, m_iWithMesh, m_iLights, m_iLightsOn, m_iShadow, m_iPhysics, m_iPhysicsDynamic, m_iReplicated) + string.Format(" frame=%1", m_iFrame);
	}

	//------------------------------------------------------------------------------------------------
	//! Each hierarchy is counted once, from its root, so a light child of a lamp prefab is found even
	//! when the sphere query does not return it on its own.
	protected bool AddRoot(IEntity e)
	{
		if (e.GetParent())
			return true;

		Walk(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Walk(IEntity e)
	{
		if (vector.DistanceSq(e.GetOrigin(), m_vCentre) <= m_fRadiusSq)
			Tally(e);

		IEntity child = e.GetChildren();
		while (child)
		{
			Walk(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tally(IEntity e)
	{
		m_iEntities++;
		if (e.GetFlags() & EntityFlags.STATIC)
			m_iStatic++;
		if (e.GetVObject())
			m_iWithMesh++;
		if (e.GetEventMask() & EntityEvent.FRAME)
			m_iFrame++;
		if (e.FindComponent(RplComponent))
			m_iReplicated++;

		Physics physics = e.GetPhysics();
		if (physics)
		{
			m_iPhysics++;
			if (physics.IsDynamic())
				m_iPhysicsDynamic++;
		}

		LightEntity light = LightEntity.Cast(e);
		if (light)
		{
			m_iLights++;
			if (light.IsEnabled())
				m_iLightsOn++;
			if (light.IsCastShadow())
				m_iShadow++;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Server half: boxes that exist only on the server, so its frame time carries their cost.
class LF_PerfStressServer
{
	protected static ref array<EntityID> s_aProps = {};

	//------------------------------------------------------------------------------------------------
	static string Run(vector centre, int props, bool allowed)
	{
		if (!Replication.IsServer())
			return "not_server";
		if (!allowed)
		{
			LF_Debug.Warn(LF_PerfStress.FEATURE, "server props refused: not a harness build");
			return "not_allowed";
		}

		LF_PerfStress.DeleteAll(s_aProps);

		if (props < 0 || props > LF_PerfStress.MAX_SPAWN)
			return "bad_count";

		Resource prop = Resource.Load(LF_PerfStress.PROP);
		if (props > 0 && (!prop || !prop.IsValid()))
		{
			LF_Debug.Error(LF_PerfStress.FEATURE, "server prop prefab did not load");
			return "no_prop";
		}
		for (int i = 0; i < props; i++)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = LF_PerfStress.PropPos(centre, i);
			IEntity e = GetGame().SpawnEntityPrefabLocal(prop, GetGame().GetWorld(), params);
			if (e)
				s_aProps.Insert(e.GetID());
		}

		string made = string.Format("server props=%1", s_aProps.Count());
		LF_Debug.Info(LF_PerfStress.FEATURE, made);
		return made;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskPerfStressServer(vector centre, int props)
	{
		if (Replication.IsServer())
			RpcAsk_LF_PerfStressServer(centre, props);
		else
			Rpc(RpcAsk_LF_PerfStressServer, centre, props);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PerfStressServer(vector centre, int props)
	{
		// Harness builds only (CheckHarnessRequest also lets an administrator through on a dev world).
		bool allowed = LF_DevGate.CheckHarnessRequest(GetControlledEntity(), "perfstress server");
		LF_PerfStressServer.Run(centre, props, allowed);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "perfstress")
			return super.Handle(cmd);

		if (cmd.Arg1 == "count")
		{
			LF_PerfStressCount counter = new LF_PerfStressCount();
			if (cmd.Arg4.IsEmpty())
			{
				// count <name> <r>: around a named world entity (a building's layer name).
				IEntity named = GetGame().GetWorld().FindEntityByName(cmd.Arg2);
				if (!named)
				{
					LF_AgentHarness.Answer(cmd.Id, "error", "no_entity " + cmd.Arg2);
					return true;
				}
				vector at = named.GetOrigin();
				string where = string.Format("at=%1,%2,%3 yaw=%4 ", at[0], at[1], at[2], named.GetYawPitchRoll()[0]);
				LF_AgentHarness.Answer(cmd.Id, "ok", where + counter.Run(at, cmd.Arg3.ToFloat()));
				return true;
			}
			vector centre = Vector(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), cmd.Arg4.ToFloat());
			LF_AgentHarness.Answer(cmd.Id, "ok", counter.Run(centre, cmd.Arg5.ToFloat()));
			return true;
		}
		if (cmd.Arg1 == "at")
		{
			LF_PerfStress.SetCentre(Vector(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), cmd.Arg4.ToFloat()), cmd.Arg5 != "p");
			LF_AgentHarness.Answer(cmd.Id, "ok", "centre " + LF_PerfStress.GetCentre().ToString());
			return true;
		}
		if (cmd.Arg1 == "spawn")
		{
			string made = LF_PerfStress.Spawn(cmd.Arg2.ToInt(), cmd.Arg3.ToInt(), cmd.Arg4.ToInt());
			if (made.StartsWith("lights="))
				LF_AgentHarness.Answer(cmd.Id, "ok", made);
			else
				LF_AgentHarness.Answer(cmd.Id, "error", made);
			return true;
		}
		if (cmd.Arg1 == "server")
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (!controller)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
				return true;
			}
			controller.LF_AskPerfStressServer(LF_PerfStress.GetCentre(), cmd.Arg2.ToInt());
			LF_AgentHarness.Answer(cmd.Id, "ok", "asked server props=" + cmd.Arg2);
			return true;
		}
		if (cmd.Arg1 == "clear")
		{
			LF_PerfStress.Clear();
			LF_AgentHarness.Answer(cmd.Id, "ok", "cleared");
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "need count|at|spawn|server|clear");
		return true;
	}
}
