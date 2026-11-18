// LF_PlaceSettlePlugin.c - physics settle for loose dynamic props (#2957).
//
// Trace snap (LF_PlaceSnap, `place settle`) puts a static prop's bottom on its support. A dynamic rigid
// body (a log, a sack, a bottle on its side) is simulated by every client, so a pose that is only
// "touching" can still roll or fall over (the logging stand's logs, placement-audit.md "Limits"). This
// plugin drops the body in Workbench play mode, steps physics until it rests, and writes the pose it came
// to rest in (tilt included) back to the layer, so what the layer places is what a client's physics leaves.
//
// tools/visual/place-settle.mjs copies this file into the checkout's git-ignored
// Scripts/WorkbenchGame/LFVisual/ and starts a hidden Workbench with
//   -wbModule=WorldEditor -plugin=LF_PlaceSettlePlugin -profile <dir>
//
// $profile:lf-settle/plan.txt, one directive per line:
//   world <resource path>                 opened when another world is loaded
//   park <x> <z>                          where the editor camera waits while play starts
//   rest <speed> <spin> <hold ms> <max ms>  at rest = |v| under speed m/s and |w| under spin rad/s for hold ms
//                                         (or the body went to sleep); max ms gives up (default 0.02 0.05 800 15000)
//   drop <name> <x> <y> <z> <angleX> <angleY> <angleZ> <prefab>
//                                         spawns the prefab there (layer angles, degrees), simulates it to rest,
//                                         then spawns it again at the rest pose and simulates that (`stable`).
//   settle <entity name>                  a layer entity: found in the play world by name, woken, simulated to
//                                         rest; after play its source gets coords and angleX/Y/Z of the rest
//                                         pose and the world is saved (unless `dry`).
//   lift <m>                              settle starts the body this far over its layer pose (default 0.02)
//   dry                                   settle measures only, writes nothing
//
// $profile:lf-settle/status.txt gets one line per step and a final `done <rows> <failed>`:
//   <ms> rest <name> pos=<x,y,z> ang=<ax,ay,az> moved=<m> tilt=<deg> ms=<t> slept=<0|1>
//   <ms> stable <name> drift=<m> turn=<deg>      the rest pose simulated again (drop only)
//   <ms> wrote <name> coords=<x y z> angles=<ax ay az> err=<deg>   the editor entity read back after the write
//   <ms> error <reason>

[WorkbenchPluginAttribute(name: "LF Place Settle", description: "Simulates loose dynamic props to rest and writes the pose to the layer (#2957)", wbModules: { "WorldEditor" }, category: "Life Framework")]
class LF_PlaceSettlePlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-settle";
	protected static const int PLAY_TIMEOUT_MS = 90000;
	protected static const int TICK_MS = 50;
	//! A rest pose that moves less than this when simulated again counts as stable.
	protected static const float STABLE_DRIFT_M = 0.01;
	protected static const float STABLE_TURN_DEG = 2;

	protected int m_iStart;
	protected ref array<string> m_aStatus = {};
	protected WorldEditorAPI m_Api;
	protected BaseWorld m_World;
	protected float m_fSpeed = 0.02;
	protected float m_fSpin = 0.05;
	protected int m_iHoldMs = 800;
	protected int m_iMaxMs = 15000;
	protected float m_fLift = 0.02;
	protected int m_iFailed;
	protected int m_iRows;

	// settle rows, filled in play and written after it
	protected ref array<string> m_aSettleNames = {};
	protected ref array<vector> m_aSettlePos = {};
	protected ref array<vector> m_aSettleAng = {};

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		m_iStart = System.GetTickCount();
		RunPlan();
		Note(string.Format("done %1 %2", m_iRows, m_iFailed));
		int code = 0;
		if (m_iFailed > 0)
			code = 1;

		Workbench.Exit(code);
	}

	//------------------------------------------------------------------------------------------------
	protected void Note(string line)
	{
		string row = string.Format("%1 %2", System.GetTickCount() - m_iStart, line);
		m_aStatus.Insert(row);
		Print("[LFSettle] " + row, LogLevel.NORMAL);
		// Rewritten after every step, so a killed run still says how far it got.
		FileHandle f = FileIO.OpenFile(DIR + "/status.txt", FileMode.WRITE);
		if (!f)
			return;

		foreach (string saved : m_aStatus)
		{
			f.WriteLine(saved);
		}

		f.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string reason)
	{
		m_iFailed++;
		Note("error " + reason);
	}

	//------------------------------------------------------------------------------------------------
	protected string Rest(string line, int from)
	{
		return line.Substring(from, line.Length() - from);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPlan()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
		{
			Fail("no_world_editor");
			return;
		}

		FileHandle plan = FileIO.OpenFile(DIR + "/plan.txt", FileMode.READ);
		if (!plan)
		{
			Fail("no_plan " + DIR + "/plan.txt");
			return;
		}

		string world;
		string park;
		bool dry;
		array<string> drops = {};
		array<string> settles = {};
		string line;
		while (plan.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			array<string> p = {};
			line.Split(" ", p, true);
			if (p.IsEmpty())
				continue;

			if (p[0] == "world")
				world = Rest(line, 6);
			else if (p[0] == "park")
				park = Rest(line, 5);
			else if (p[0] == "dry")
				dry = true;
			else if (p[0] == "lift" && p.Count() >= 2)
				m_fLift = p[1].ToFloat();
			else if (p[0] == "rest" && p.Count() >= 5)
			{
				m_fSpeed = p[1].ToFloat();
				m_fSpin = p[2].ToFloat();
				m_iHoldMs = p[3].ToInt();
				m_iMaxMs = p[4].ToInt();
			}
			else if (p[0] == "drop")
				drops.Insert(line);
			else if (p[0] == "settle" && p.Count() >= 2)
				settles.Insert(p[1]);
		}

		plan.Close();

		m_Api = worldEditor.GetApi();
		string opened;
		if (m_Api)
			m_Api.GetWorldPath(opened);

		if (!world.IsEmpty() && !WorldIsOpen(opened, world))
		{
			if (!worldEditor.SetOpenedResource(world))
			{
				Fail("world_not_opened " + world);
				return;
			}

			m_Api = worldEditor.GetApi();
		}

		if (!m_Api || !m_Api.GetWorld())
		{
			Fail("no_world_api");
			return;
		}

		m_Api.GetWorldPath(opened);
		Note("ok world " + opened);
		if (!EnterPlay(worldEditor, park))
			return;

		foreach (string drop : drops)
		{
			Drop(drop);
		}

		foreach (string name : settles)
		{
			SettleNamed(name);
		}

		LeavePlay(worldEditor);
		if (m_aSettleNames.IsEmpty())
			return;

		if (dry)
		{
			Note("ok dry wrote=0");
			return;
		}

		int wrote = WriteSettled();
		if (wrote > 0 && !worldEditor.Save())
			Fail("save_failed");
		else
			Note("ok saved wrote=" + wrote);
	}

	//------------------------------------------------------------------------------------------------
	protected bool WorldIsOpen(string opened, string wanted)
	{
		array<string> parts = {};
		wanted.Split("/", parts, true);
		if (opened.IsEmpty() || parts.IsEmpty())
			return false;

		string file = parts[parts.Count() - 1];
		file.ToLower();
		string low = opened;
		low.ToLower();
		return low.IndexOf(file) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnterPlay(notnull WorldEditor worldEditor, string park)
	{
		array<string> p = {};
		park.Split(" ", p, true);
		if (p.Count() >= 2)
		{
			float px = p[0].ToFloat();
			float pz = p[1].ToFloat();
			m_Api.SetCamera(Vector(px, m_Api.GetWorld().GetSurfaceY(px, pz) + 2, pz), "0 -0.5 0.86");
			Sleep(100);
		}

		worldEditor.SwitchToGameMode();
		int waited;
		while (!m_Api.IsGameMode() || !GetGame() || !GetGame().GetWorld())
		{
			if (waited > PLAY_TIMEOUT_MS)
			{
				Fail("play_not_started");
				return false;
			}

			Sleep(100);
			waited += 100;
		}

		m_World = GetGame().GetWorld();
		// A few frames for the physics world to come up before anything is dropped.
		Sleep(1000);
		Note("ok play waited=" + waited);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LeavePlay(notnull WorldEditor worldEditor)
	{
		if (!m_Api.IsGameMode())
			return;

		worldEditor.SwitchToEditMode();
		int waited;
		while (m_Api.IsGameMode() && waited < PLAY_TIMEOUT_MS)
		{
			Sleep(100);
			waited += 100;
		}

		Sleep(1500);
		Note("ok edit waited=" + waited);
	}

	//------------------------------------------------------------------------------------------------
	//! Layer angles (angleX pitch, angleY yaw, angleZ roll, degrees) to a rotation; Math3D takes yaw pitch roll.
	protected void AnglesToMatrix(vector layerAngles, out vector mat[4])
	{
		Math3D.AnglesToMatrix(Vector(layerAngles[1], layerAngles[0], layerAngles[2]), mat);
	}

	//------------------------------------------------------------------------------------------------
	protected vector MatrixToLayerAngles(vector mat[4])
	{
		vector rot[3] = { mat[0], mat[1], mat[2] };
		vector ypr = Math3D.MatrixToAngles(rot);
		return Vector(ypr[1], ypr[0], ypr[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Degrees between two rotations' axes (the worst of the three).
	protected float TurnDeg(vector a[4], vector b[4])
	{
		float worst;
		for (int i = 0; i < 3; i++)
		{
			float d = Math.Clamp(vector.Dot(a[i].Normalized(), b[i].Normalized()), -1, 1);
			worst = Math.Max(worst, Math.Acos(d) * Math.RAD2DEG);
		}

		return worst;
	}

	//------------------------------------------------------------------------------------------------
	//! Simulates `body` until it rests (or m_iMaxMs). \return elapsed ms, -1 when it never rested.
	protected int SimulateToRest(notnull IEntity body, out bool slept)
	{
		Physics physics = body.GetPhysics();
		physics.ChangeSimulationState(SimulationState.SIMULATION);
		physics.SetActive(ActiveState.ACTIVE);
		int elapsed;
		int calm;
		slept = false;
		while (elapsed < m_iMaxMs)
		{
			Sleep(TICK_MS);
			elapsed += TICK_MS;
			if (!physics.IsActive() && elapsed >= 300)
			{
				slept = true;
				return elapsed;
			}

			if (physics.GetVelocity().Length() < m_fSpeed && physics.GetAngularVelocity().Length() < m_fSpin)
				calm += TICK_MS;
			else
				calm = 0;

			if (calm >= m_iHoldMs)
				return elapsed;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Null (and a logged error) when the entity has no dynamic rigid body: trace snap is its tool.
	protected Physics DynamicOf(IEntity body, string name)
	{
		if (!body)
		{
			Fail("not_found " + name);
			return null;
		}

		Physics physics = body.GetPhysics();
		if (!physics || !physics.IsDynamic())
		{
			Fail("not_dynamic " + name + " (static props settle by trace snap: tools\\cli place settle)");
			return null;
		}

		return physics;
	}

	//------------------------------------------------------------------------------------------------
	//! Simulates the body to rest and notes the rest row. \return false when it never came to rest.
	protected bool RestRow(notnull IEntity body, string name, vector start[4], out vector rest[4])
	{
		bool slept;
		int ms = SimulateToRest(body, slept);
		body.GetWorldTransform(rest);
		if (ms < 0)
		{
			Fail(string.Format("no_rest %1 after=%2 v=%3", name, m_iMaxMs, body.GetPhysics().GetVelocity().Length()));
			return false;
		}

		// A body can go to sleep while it still rocks (a log on its curve). Rest is a fixed point: wake it and
		// simulate again until a pass moves it less than half the stable bar, at most six passes.
		int passes = 1;
		while (passes < 6)
		{
			vector before[4];
			body.GetWorldTransform(before);
			bool again;
			int more = SimulateToRest(body, again);
			body.GetWorldTransform(rest);
			passes++;
			if (more < 0)
			{
				Fail(string.Format("no_rest %1 pass=%2", name, passes));
				return false;
			}

			ms += more;
			slept = again;
			if (vector.Distance(before[3], rest[3]) <= STABLE_DRIFT_M * 0.5 && TurnDeg(before, rest) <= STABLE_TURN_DEG * 0.5)
				break;
		}

		float tilt = Math.Acos(Math.Clamp(rest[1].Normalized()[1], -1, 1)) * Math.RAD2DEG;
		vector ang = MatrixToLayerAngles(rest);
		int sleptFlag = 0;
		if (slept)
			sleptFlag = 1;

		// string.Format takes nine arguments at most.
		string row = string.Format("rest %1 pos=%2,%3,%4 ang=%5,%6,%7", name, rest[3][0], rest[3][1], rest[3][2], ang[0], ang[1], ang[2]);
		Note(row + string.Format(" moved=%1 tilt=%2 ms=%3 slept=%4 passes=%5", vector.Distance(start[3], rest[3]), tilt, ms, sleptFlag, passes));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(Resource res, vector mat[4])
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = mat;
		return GetGame().SpawnEntityPrefab(res, m_World, params);
	}

	//------------------------------------------------------------------------------------------------
	//! drop <name> <x> <y> <z> <angleX> <angleY> <angleZ> <prefab>
	protected void Drop(string line)
	{
		m_iRows++;
		array<string> p = {};
		line.Split(" ", p, true);
		if (p.Count() < 9)
		{
			Fail("bad_drop " + line);
			return;
		}

		string name = p[1];
		Resource res = Resource.Load(p[8]);
		if (!res || !res.IsValid())
		{
			Fail("prefab_not_loaded " + p[8]);
			return;
		}

		vector start[4];
		AnglesToMatrix(Vector(p[5].ToFloat(), p[6].ToFloat(), p[7].ToFloat()), start);
		start[3] = Vector(p[2].ToFloat(), p[3].ToFloat(), p[4].ToFloat());
		IEntity body = Spawn(res, start);
		if (!DynamicOf(body, name))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return;
		}

		// A drop starts clear of the ground: a long body tilted into the terrain is shot out of it by the
		// depenetration (a log pitched 70 degrees left at 16 m/s). Spawn it again with its lowest point at `y`.
		vector mins, maxs;
		body.GetWorldBounds(mins, maxs);
		float ground = m_World.GetSurfaceY(start[3][0], start[3][2]);
		float floorY = Math.Max(start[3][1], ground + m_fLift);
		if (mins[1] < floorY)
		{
			start[3] = start[3] + Vector(0, floorY - mins[1], 0);
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			body = Spawn(res, start);
			if (!DynamicOf(body, name))
				return;
		}

		vector rest[4];
		bool rested = RestRow(body, name, start, rest);
		SCR_EntityHelper.DeleteEntityAndChildren(body);
		if (!rested)
			return;

		// What a client does with the written pose: spawn it there and let physics run. It must stay put.
		// The pose goes through the layer's own angles first, so the check also covers the conversion.
		vector again[4];
		AnglesToMatrix(MatrixToLayerAngles(rest), again);
		again[3] = rest[3];
		IEntity second = Spawn(res, again);
		if (!DynamicOf(second, name))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(second);
			return;
		}

		bool slept;
		SimulateToRest(second, slept);
		// Simulate at least one hold window even when it slept at once.
		Sleep(m_iHoldMs);
		vector after[4];
		second.GetWorldTransform(after);
		SCR_EntityHelper.DeleteEntityAndChildren(second);
		float drift = vector.Distance(again[3], after[3]);
		float turn = TurnDeg(again, after);
		Note(string.Format("stable %1 drift=%2 turn=%3", name, drift, turn));
		if (drift > STABLE_DRIFT_M || turn > STABLE_TURN_DEG)
			Fail(string.Format("unstable %1 drift=%2 turn=%3", name, drift, turn));
	}

	//------------------------------------------------------------------------------------------------
	//! A layer entity in the play world: lifted m_fLift, woken, simulated to rest; the pose is kept for WriteSettled.
	protected void SettleNamed(string name)
	{
		m_iRows++;
		IEntity body = m_World.FindEntityByName(name);
		if (!DynamicOf(body, name))
			return;

		vector start[4];
		body.GetWorldTransform(start);
		start[3] = start[3] + Vector(0, m_fLift, 0);
		body.SetWorldTransform(start);
		vector rest[4];
		if (!RestRow(body, name, start, rest))
			return;

		m_aSettleNames.Insert(name);
		m_aSettlePos.Insert(rest[3]);
		m_aSettleAng.Insert(MatrixToLayerAngles(rest));
	}

	//------------------------------------------------------------------------------------------------
	//! Writes each settled pose to its entity source (edit mode). \return how many were written.
	protected int WriteSettled()
	{
		int wrote;
		m_Api.BeginEntityAction("LF place settle physics");
		foreach (int i, string name : m_aSettleNames)
		{
			IEntitySource src = m_Api.FindEntityByName(name);
			if (!src)
			{
				Fail("source_not_found " + name);
				continue;
			}

			// Layer coords are relative to a parent; the plugin only writes top-level props.
			if (src.GetParent())
			{
				Fail("has_parent " + name + " (settle top-level props only)");
				continue;
			}

			vector pos = m_aSettlePos[i];
			vector ang = m_aSettleAng[i];
			m_Api.SetVariableValue(src, null, "coords", string.Format("%1 %2 %3", pos[0], pos[1], pos[2]));
			m_Api.SetVariableValue(src, null, "angleX", ang[0].ToString());
			m_Api.SetVariableValue(src, null, "angleY", ang[1].ToString());
			m_Api.SetVariableValue(src, null, "angleZ", ang[2].ToString());
			wrote++;
		}

		m_Api.EndEntityAction("LF place settle physics");

		// Read the editor entities back: the written angles must give the simulated rotation.
		foreach (int j, string again : m_aSettleNames)
		{
			IEntitySource back = m_Api.FindEntityByName(again);
			IEntity ent;
			if (back)
				ent = m_Api.SourceToEntity(back);

			if (!ent)
				continue;

			vector want[4];
			AnglesToMatrix(m_aSettleAng[j], want);
			want[3] = m_aSettlePos[j];
			vector got[4];
			ent.GetWorldTransform(got);
			float err = TurnDeg(want, got);
			vector p = m_aSettlePos[j];
			vector a = m_aSettleAng[j];
			Note(string.Format("wrote %1 coords=%2 %3 %4 angles=%5 %6 %7 err=%8 off=%9", again, p[0], p[1], p[2], a[0], a[1], a[2], err, vector.Distance(want[3], got[3])));
			if (err > 1 || vector.Distance(want[3], got[3]) > 0.005)
				Fail("readback " + again + " err=" + err);
		}

		return wrote;
	}
}
