// LF_VisualShotsPlugin.c - World Editor shots without a game session (tools\cli look).
//
// tools/visual/look.mjs copies this file into the checkout's git-ignored
// Scripts/WorkbenchGame/LFVisual/ and starts a hidden Workbench with
//   -wbModule=WorldEditor -plugin=LF_VisualShotsPlugin -forceUpdate -profile <dir>
// RunCommandline opens the world, then for every `shot` line of
// $profile:lf-visual/plan.txt picks a camera, lets the view settle, and writes the
// viewport with System.MakeScreenshot.
//
// mode edit: the World Editor view (WorldEditorAPI.SetCamera). Shows what the layers
//   place (mesh, material, scale, position) with the editor's own helper shapes on top.
//   Nothing a script spawns at run time exists, and no UI.
// mode play: Workbench play mode with a free camera, the way SCR_FPSDiagnosticPlugin
//   does it. A clean frame, and run-time spawns are there. Workbench play captures no UI.
//
// plan.txt, one directive per line:
//   world <resource path>            opened when no world is loaded
//   mode edit|play
//   hour <0-24>                      local solar time of day, optional (weather is forced Clear with it)
//   park <x> <z>                     where the editor camera waits while play starts: play
//                                    mode spawns a player body at the editor camera
//   settle <ms>                      wait between camera move and shot
//   first <ms>                       extra wait before the first shot (streaming, shaders)
//   shot <name> <tx> <ty|-> <tz> <lookUp> <accept> <az:dist:up>[,<az:dist:up>...]
//     target = (tx, ty or the ground height, tz) + lookUp. Each candidate camera stands
//     `dist` metres away on compass heading az (0 = +z, 90 = +x), `up` metres above ty,
//     never under the ground. A ray from the camera to the target measures how much of
//     the way is clear (1 = nothing between); the first candidate at or above `accept`
//     wins, else the clearest one.
//   item <name> ground|inventory <x> <z> <9 axes> <ox oy oz> <pitch yaw roll> <dist> <fov> <cx cy cz> <prefab>
//     spawns the prefab (play mode only), poses it, takes the picture, deletes it. ground: the item's three
//     axes in the world (RestingUP faces up), lowest point on the ground, then a close-up. inventory: model
//     orientation, frozen, seen from the prefab's PreviewRenderAttributes camera (tools/visual/look-item.mjs).
//   close <name> <entity|-> <x> <y> <z> <az>
//     a close-up of the entity a layer placed there (found by name, else the nearest
//     entity with a mesh within 1 m). Framed from its world bounds, from the clearest of
//     eight headings, starting in front of it (its +z axis; az when it was not found).
//     Its status line carries size=<x>,<y>,<z> in metres.
// status.txt gets one line per step (`<ms> ok shot <name> <file> pick= clear= cam=`,
// `<ms> error <reason>`) and a final `done <shots> <failed>`; Node reads it after
// Workbench exits.
//
// Server mode (tools\cli look serve, tools/visual/look-serve.mjs): with $profile:lf-visual/serve.txt
// present, RunCommandline does not exit after one plan. It rewrites alive.txt every 3 s while idle;
// for each go.txt (request id, then reload none|reopen|close) it leaves play mode, reopens the world
// from disk when asked (reopen picks up a changed .layer or .et; a changed script must never be
// reopened under a live Workbench: it crashed 2 of 2 times), shoots plan.txt with the request id as
// the first status line, and writes done.txt with the id. stop.txt, or 10 min with no request, ends it.

[WorkbenchPluginAttribute(name: "LF Visual Shots", description: "Takes the shots of $profile:lf-visual/plan.txt (tools\\cli look)", wbModules: { "WorldEditor" }, category: "Life Framework")]
class LF_VisualShotsPlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-visual";
	protected static const int SHOT_TIMEOUT_MS = 5000;
	protected static const int PLAY_TIMEOUT_MS = 90000;
	protected static const int SERVE_IDLE_MS = 600000;
	protected static const int SERVE_BEAT_MS = 3000;
	//! Server mode: play mode gets this long in all; SwitchToGameMode is asked again every SERVE_PLAY_RETRY_MS
	//! while it has not taken (right after leaving play it is ignored once in a while; probe 2026-11-14).
	protected static const int SERVE_PLAY_TIMEOUT_MS = 40000;
	protected static const int SERVE_PLAY_RETRY_MS = 8000;
	protected static const float VERTICAL_FOV = 60;
	// The wide lens also gets a fan of level rays across its view. A thin pole right in front of the
	// lens (a lamp post, a sign) misses the one centre ray and ruins the picture (#814). The step is
	// small enough that a 0.2 m pole 3 m away is still hit; tools/visual/look.mjs selftest checks it.
	protected static const float FAN_STEP_DEG = 3;
	protected static const float FAN_HALF_DEG = 42;
	protected static const float FAN_REACH = 3.5;

	protected ref array<string> m_aStatus = {};
	protected int m_iStart;
	protected bool m_bServe;
	protected bool m_bPlay;
	protected WorldEditorAPI m_Api;
	protected BaseWorld m_World;
	protected CameraBase m_Camera;
	protected float m_fHour = -1;
	protected IEntity m_Nearest;
	//! An item the plan spawned: TakeClose frames this one instead of searching the layers.
	protected IEntity m_ForceEntity;
	protected float m_fNearest;
	protected vector m_vNearest;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		m_iStart = System.GetTickCount();
		if (FileIO.FileExists(DIR + "/serve.txt"))
		{
			m_bServe = true;
			Serve();
			Workbench.Exit(0);
			return;
		}

		int failed = Shoot();
		int code = 0;
		if (failed != 0)
			code = 1;

		Workbench.Exit(code);
	}

	//------------------------------------------------------------------------------------------------
	//! Server mode (tools\cli look serve): this Workbench stays open and takes one plan after another.
	//! Node writes plan.txt, then go.txt (line 1: request id, line 2: how to reload the world). The
	//! answer is status.txt plus done.txt holding the id. stop.txt, or SERVE_IDLE_MS without a
	//! request, ends it. alive.txt is rewritten every few seconds so Node can tell a dead server.
	protected void Serve()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
			return;

		int idle;
		int beat = SERVE_BEAT_MS;
		while (!FileIO.FileExists(DIR + "/stop.txt"))
		{
			if (beat >= SERVE_BEAT_MS)
			{
				beat = 0;
				WriteLine(DIR + "/alive.txt", System.GetTickCount().ToString());
			}

			if (!FileIO.FileExists(DIR + "/go.txt"))
			{
				if (idle > SERVE_IDLE_MS)
					break;

				Sleep(200);
				idle += 200;
				beat += 200;
				continue;
			}

			idle = 0;
			string id;
			string reload;
			FileHandle go = FileIO.OpenFile(DIR + "/go.txt", FileMode.READ);
			if (go)
			{
				go.ReadLine(id);
				go.ReadLine(reload);
				go.Close();
			}

			FileIO.DeleteFile(DIR + "/go.txt");
			id.TrimInPlace();
			reload.TrimInPlace();
			m_aStatus.Clear();
			m_iStart = System.GetTickCount();
			m_Camera = null;
			m_ForceEntity = null;
			// The first status line names the request, so Node never reads an older request's answer as this one's.
			Note("ok request " + id + " reload=" + reload);
			LeavePlay(worldEditor);
			if (reload == "close")
				Note("ok closed " + worldEditor.Close());

			Shoot(reload == "reopen");
			// The heartbeat first: a caller that sees done.txt finds the server healthy, not stale from the request.
			WriteLine(DIR + "/alive.txt", System.GetTickCount().ToString());
			beat = 0;
			WriteLine(DIR + "/done.txt", id);
		}

		FileIO.DeleteFile(DIR + "/alive.txt");
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteLine(string path, string text)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.WRITE);
		if (!f)
			return;

		f.WriteLine(text);
		f.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void LeavePlay(notnull WorldEditor worldEditor)
	{
		WorldEditorAPI api = worldEditor.GetApi();
		if (!api || !api.IsGameMode())
			return;

		worldEditor.SwitchToEditMode();
		int waited;
		while (api.IsGameMode() && waited < SERVE_PLAY_TIMEOUT_MS)
		{
			Sleep(100);
			waited += 100;
		}

		// Leaving play reloads the game ("Workbench Reload Game"); give it a moment before the next SwitchToGameMode.
		Sleep(1500);
		Note("ok edit waited=" + waited);
	}

	//------------------------------------------------------------------------------------------------
	protected void Note(string line)
	{
		string row = string.Format("%1 %2", System.GetTickCount() - m_iStart, line);
		m_aStatus.Insert(row);
		Print("[LFVisual] " + row, LogLevel.NORMAL);

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
	protected string Rest(string line, int from)
	{
		return line.Substring(from, line.Length() - from);
	}

	//------------------------------------------------------------------------------------------------
	//! \return the number of failed steps (0 = every shot is on disk)
	protected int Shoot(bool reopen = false)
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
		{
			Note("error no_world_editor");
			return 1;
		}

		FileHandle plan = FileIO.OpenFile(DIR + "/plan.txt", FileMode.READ);
		if (!plan)
		{
			Note("error no_plan " + DIR + "/plan.txt");
			return 1;
		}

		string world;
		float hour = -1;
		int settleMs = 400;
		int firstMs = 1500;
		string park;
		array<string> shots = {};
		string line;
		while (plan.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (line.StartsWith("world "))
				world = Rest(line, 6);
			else if (line.StartsWith("mode "))
				m_bPlay = Rest(line, 5) == "play";
			else if (line.StartsWith("hour "))
				hour = Rest(line, 5).ToFloat();
			else if (line.StartsWith("settle "))
				settleMs = Rest(line, 7).ToInt();
			else if (line.StartsWith("first "))
				firstMs = Rest(line, 6).ToInt();
			else if (line.StartsWith("park "))
				park = Rest(line, 5);
			else if (line.StartsWith("shot ") || line.StartsWith("close ") || line.StartsWith("item "))
				shots.Insert(line);
		}

		plan.Close();

		m_Api = worldEditor.GetApi();
		string opened;
		if (m_Api)
			m_Api.GetWorldPath(opened);

		if (opened.IsEmpty() || reopen)
		{
			if (world.IsEmpty() || !worldEditor.SetOpenedResource(world))
			{
				Note("error world_not_opened " + world);
				return 1;
			}

			m_Api = worldEditor.GetApi();
		}

		if (!m_Api || !m_Api.GetWorld())
		{
			Note("error no_world_api");
			return 1;
		}

		m_World = m_Api.GetWorld();
		m_Api.GetWorldPath(opened);
		Note(string.Format("ok world %1 view=%2x%3 entities=%4", opened, m_Api.GetScreenWidth(), m_Api.GetScreenHeight(), m_Api.GetEditorEntityCount()));

		if (m_bPlay && !EnterPlay(worldEditor, park))
			return 1;

		m_fHour = hour;
		SetHour();

		int failed;
		bool first = true;
		foreach (string shot : shots)
		{
			if (!TakeShot(shot, settleMs, first, firstMs))
				failed++;

			first = false;
		}

		Note(string.Format("done %1 %2", shots.Count(), failed));
		return failed;
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
			m_Api.SetCamera(Vector(px, m_World.GetSurfaceY(px, pz) + 2, pz), "0 -0.5 0.86");
			Sleep(100);
		}

		worldEditor.SwitchToGameMode();

		int timeout = PLAY_TIMEOUT_MS;
		if (m_bServe)
			timeout = SERVE_PLAY_TIMEOUT_MS;

		int waited;
		int asks = 1;
		while (!m_Api.IsGameMode())
		{
			if (waited > timeout)
			{
				Note("error play_not_started asks=" + asks);
				return false;
			}

			if (m_bServe && waited > 0 && waited % SERVE_PLAY_RETRY_MS == 0)
			{
				worldEditor.SwitchToGameMode();
				asks++;
			}

			Sleep(100);
			waited += 100;
		}

		ArmaReforgerScripted game = GetGame();
		while (!game || !game.GetWorld() || !game.GetCameraManager())
		{
			if (waited > timeout)
			{
				Note("error play_no_world");
				return false;
			}

			Sleep(100);
			waited += 100;
			game = GetGame();
		}

		m_World = game.GetWorld();
		m_Camera = CameraBase.Cast(game.SpawnEntity(CameraBase));
		if (!m_Camera)
		{
			Note("error play_no_camera");
			return false;
		}

		game.GetCameraManager().SetCamera(m_Camera);
		Note("ok play waited=" + waited + " asks=" + asks);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Local solar time, the way the harness `time` verb sets it (LF_AgentHarness_Character).
	protected void SetHour()
	{
		if (m_fHour < 0)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(m_World);
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (!clock)
		{
			Note("warn no_clock");
			return;
		}

		float game = m_fHour - clock.GetTimeZoneOffset() - clock.GetCurrentLongitude() / 15;
		while (game < 0)
			game += 24;

		while (game >= 24)
			game -= 24;

		clock.SetTimeOfTheDay(game, true);
		clock.ForceWeatherTo(clock.IsWeatherLooping(), "Clear", 0, 3600);
		Note(string.Format("ok hour local=%1 game=%2", m_fHour, game));
	}

	//------------------------------------------------------------------------------------------------
	protected void Place(vector cam, vector target, float fov)
	{
		vector dir = target - cam;
		dir.Normalize();
		if (!m_bPlay)
		{
			m_Api.SetCamera(cam, dir);
			return;
		}

		// A level camera: right = up x forward, up = forward x right. Straight down has no level
		// right, so north (+z) is the top of the frame there.
		vector right = Vector(dir[2], 0, -dir[0]);
		if (right.Length() < 0.02)
			right = "1 0 0";

		right.Normalize();
		vector up = Vector(dir[1] * right[2] - dir[2] * right[1], dir[2] * right[0] - dir[0] * right[2], dir[0] * right[1] - dir[1] * right[0]);
		vector mat[4];
		mat[0] = right;
		mat[1] = up;
		mat[2] = dir;
		mat[3] = cam;
		m_Camera.SetTransform(mat);
		m_Camera.SetVerticalFOV(fov);
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the way from the camera to the target with nothing in it (1 = clear).
	protected float Clear(vector cam, vector target)
	{
		TraceParam trace = new TraceParam();
		trace.Start = cam;
		trace.End = target;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		return m_World.TraceMove(trace, null);
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the way to the target at which the first thing inside the lens fan stands (1 = nothing).
	//! Only the first FAN_REACH metres count, or half the distance when the camera is close.
	protected float ClearFan(vector cam, vector target)
	{
		vector flat = target - cam;
		float dist = flat.Length();
		flat[1] = 0;
		if (dist < 1 || flat.Length() < 0.01)
			return 1;

		flat.Normalize();
		float reach = Math.Min(FAN_REACH, dist * 0.5);
		float heading = Math.Atan2(flat[0], flat[2]);
		float worst = 1;
		for (float deg = -FAN_HALF_DEG; deg <= FAN_HALF_DEG + 0.01; deg += FAN_STEP_DEG)
		{
			float az = heading + deg * Math.DEG2RAD;
			float free = Clear(cam, cam + Vector(Math.Sin(az), 0, Math.Cos(az)) * reach);
			if (free >= 1)
				continue;

			float share = free * reach / dist;
			if (share < worst)
				worst = share;
		}

		return worst;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TakeShot(string shot, int settleMs, bool first, int firstMs)
	{
		array<string> p = {};
		shot.Split(" ", p, true);
		if (p[0] == "close")
			return TakeClose(p, shot, settleMs, first, firstMs);

		if (p[0] == "item")
			return TakeItem(p, shot, settleMs, first, firstMs);

		if (p.Count() < 8)
		{
			Note("error bad_shot " + shot);
			return false;
		}

		string name = p[1];
		float tx = p[2].ToFloat();
		float tz = p[4].ToFloat();
		float ty = m_World.GetSurfaceY(tx, tz);
		if (p[3] != "-")
			ty = p[3].ToFloat();

		vector target = Vector(tx, ty + p[5].ToFloat(), tz);
		float accept = p[6].ToFloat();
		bool wide = name.Contains("__wide");

		array<string> candidates = {};
		p[7].Split(",", candidates, true);
		vector cam;
		float clear = -1;
		int pick = -1;
		foreach (int i, string candidate : candidates)
		{
			array<string> c = {};
			candidate.Split(":", c, false);
			if (c.Count() < 3)
				continue;

			float az = c[0].ToFloat() * Math.DEG2RAD;
			float dist = c[1].ToFloat();
			vector at = Vector(tx + Math.Sin(az) * dist, ty + c[2].ToFloat(), tz + Math.Cos(az) * dist);
			float ground = m_World.GetSurfaceY(at[0], at[2]);
			if (at[1] < ground + 0.4)
				at[1] = ground + 0.4;

			float free = Clear(at, target);
			// Only the wide is taken from the ground; an oblique looks down over the trees.
			if (wide)
			{
				float fan = ClearFan(at, target);
				if (fan < free)
					free = fan;
			}

			if (free > clear)
			{
				clear = free;
				cam = at;
				pick = i;
			}

			if (free >= accept)
				break;
		}

		if (pick < 0)
		{
			Note("error bad_shot " + shot);
			return false;
		}

		return Capture(name, cam, target, VERTICAL_FOV, string.Format("pick=%1 clear=%2", pick, clear), settleMs, first, firstMs);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNearest(IEntity entity)
	{
		if (!entity || !entity.GetVObject())
			return true;

		float d = vector.Distance(entity.GetOrigin(), m_vNearest);
		if (d < m_fNearest)
		{
			m_fNearest = d;
			m_Nearest = entity;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TakeClose(notnull array<string> p, string shot, int settleMs, bool first, int firstMs)
	{
		if (p.Count() < 7)
		{
			Note("error bad_shot " + shot);
			return false;
		}

		string name = p[1];
		vector pos = Vector(p[3].ToFloat(), p[4].ToFloat(), p[5].ToFloat());
		IEntity entity = m_ForceEntity;
		if (!entity && p[2] != "-")
			entity = m_World.FindEntityByName(p[2]);

		if (!entity)
		{
			m_Nearest = null;
			m_fNearest = 1;
			m_vNearest = pos;
			m_World.QueryEntitiesBySphere(pos, 1, CollectNearest, null, EQueryEntitiesFlags.ALL);
			entity = m_Nearest;
			m_Nearest = null;
		}

		vector target = pos + "0 0.6 0";
		vector size = "1.2 1.2 1.2";
		string found = "found=0";
		float az0 = p[6].ToFloat();
		if (entity)
		{
			// An entity faces along its +z axis (the layer yaw): the first camera stands in front of it.
			vector mat[4];
			entity.GetWorldTransform(mat);
			if (Math.AbsFloat(mat[2][0]) + Math.AbsFloat(mat[2][2]) > 0.01)
				az0 = Math.Atan2(mat[2][0], mat[2][2]) * Math.RAD2DEG;

			vector mins, maxs;
			entity.GetWorldBounds(mins, maxs);
			size = maxs - mins;
			if (size.Length() > 0.05)
			{
				target = (mins + maxs) * 0.5;
				found = string.Format("found=1 size=%1,%2,%3", size[0], size[1], size[2]);
			}
			else
			{
				size = "1.2 1.2 1.2";
			}
		}

		float radius = size.Length() * 0.5;
		float dist = Math.Clamp(radius * 2.3, 1.6, 16);
		float up = Math.Clamp(radius * 0.8, 0.5, 6);
		array<float> offsets = { 0, 45, -45, 90, -90, 135, -135, 180 };
		vector cam;
		float best = -10;
		float clear;
		foreach (int i, float offset : offsets)
		{
			float az = (az0 + offset) * Math.DEG2RAD;
			vector at = Vector(target[0] + Math.Sin(az) * dist, target[1] + up, target[2] + Math.Cos(az) * dist);
			float ground = m_World.GetSurfaceY(at[0], at[2]);
			if (at[1] < ground + 0.4)
				at[1] = ground + 0.4;

			float free = Clear(at, target);
			// The entity's own surface stops the ray; among equally clear headings the earlier one wins.
			float score = free - 0.03 * i;
			// Something close behind the entity as this camera sees it (the wall a box hangs on, the shelf
			// a till stands against): this is the side people look from.
			vector away = target - at;
			away[1] = 0;
			away.Normalize();
			if (Clear(target + away * (radius * 0.5 + 0.05), target + away * (radius * 0.5 + 0.9)) < 1)
				score += 0.25;

			if (score > best)
			{
				best = score;
				clear = free;
				cam = at;
			}
		}

		// The entity and about its own width of surroundings on each side, whatever its size.
		float fov = Math.Clamp(2 * Math.Atan2(radius * 2.4, dist) * Math.RAD2DEG, 24, VERTICAL_FOV);
		return Capture(name, cam, target, fov, string.Format("clear=%1 %2", clear, found), settleMs, first, firstMs);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnItem(Resource res, vector mat[4])
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = mat;
		return GetGame().SpawnEntityPrefab(res, m_World, params);
	}

	//------------------------------------------------------------------------------------------------
	//! item <name> <mode> <x> <z> <ix iy iz jx jy jz kx ky kz> <ox oy oz> <pitch yaw roll> <dist> <fov> <cx cy cz> <prefab>
	//! Spawns the prefab on the ground at x z (play mode only), poses it, takes the picture, deletes it.
	//! ground: the item's axes are the nine numbers (RestingUP faces up), its lowest point on the ground, then a
	//!   close-up. inventory: model orientation, frozen at 1.5 m, the camera of PreviewRenderAttributes.
	protected bool TakeItem(notnull array<string> p, string shot, int settleMs, bool first, int firstMs)
	{
		if (!m_bPlay || p.Count() < 26)
		{
			Note("error bad_item " + shot);
			return false;
		}

		string name = p[1];
		string mode = p[2];
		float x = p[3].ToFloat();
		float z = p[4].ToFloat();
		Resource res = Resource.Load(p[25]);
		if (!res || !res.IsValid())
		{
			Note("error item_not_loaded " + p[25]);
			return false;
		}

		float ground = m_World.GetSurfaceY(x, z);
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		if (mode == "ground")
		{
			for (int axis = 0; axis < 3; axis++)
				mat[axis] = Vector(p[5 + axis * 3].ToFloat(), p[6 + axis * 3].ToFloat(), p[7 + axis * 3].ToFloat());
		}

		mat[3] = Vector(x, ground + 1.5, z);
		IEntity item = SpawnItem(res, mat);
		if (!item)
		{
			Note("error item_not_spawned " + p[25]);
			return false;
		}

		// A placed pose, not a fall: nothing moves it while the picture is taken.
		Physics physics = item.GetPhysics();
		if (physics)
			physics.ChangeSimulationState(SimulationState.NONE);

		vector mins, maxs;
		item.GetWorldBounds(mins, maxs);
		if (mode == "ground")
		{
			// Lowest point on the ground, then the prefab's nudge (RestingAdditiveOffsetLS, in world axes).
			// Moving a spawned item did not take; the bounds do not depend on where it stands, so spawn it again at its place.
			mat[3] = Vector(x + p[14].ToFloat(), mat[3][1] + ground - mins[1] + p[15].ToFloat(), z + p[16].ToFloat());
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			item = SpawnItem(res, mat);
			if (!item)
			{
				Note("error item_not_spawned " + p[25]);
				return false;
			}

			physics = item.GetPhysics();
			if (physics)
				physics.ChangeSimulationState(SimulationState.NONE);

			m_ForceEntity = item;
			string close = string.Format("close %1 - %2 %3 %4 180", name, x, ground, z);
			array<string> cp = {};
			close.Split(" ", cp, true);
			bool okGround = TakeClose(cp, close, settleMs, first, firstMs);
			m_ForceEntity = null;
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return okGround;
		}

		vector center = (mins + maxs) * 0.5;
		float pitch = p[17].ToFloat();
		float yaw = p[18].ToFloat() * Math.DEG2RAD;
		float dist = p[20].ToFloat();
		float fov = p[21].ToFloat();
		vector target = center + Vector(p[22].ToFloat(), p[23].ToFloat(), p[24].ToFloat());
		// Orbit: yaw round the item from its front (+z), a negative pitch looks down on it.
		float el = -pitch * Math.DEG2RAD;
		vector cam = target + Vector(Math.Sin(yaw) * Math.Cos(el), Math.Sin(el), Math.Cos(yaw) * Math.Cos(el)) * dist;
		bool ok = Capture(name, cam, target, fov, string.Format("clear=1 found=1 size=%1,%2,%3", maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]), settleMs, first, firstMs);
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Capture(string name, vector cam, vector target, float fov, string detail, int settleMs, bool first, int firstMs)
	{
		Place(cam, target, fov);
		if (first)
		{
			Sleep(firstMs);
			// A game mode can set its own clock while play starts.
			SetHour();
		}

		Sleep(settleMs);
		// The editor camera eases; set it again so the frame we keep is the framed one.
		Place(cam, target, fov);
		Sleep(60);

		string path = DIR + "/" + name;
		System.MakeScreenshot(path);

		int waited;
		string found;
		while (waited < SHOT_TIMEOUT_MS)
		{
			Sleep(50);
			waited += 50;
			if (FileIO.FileExists(path + ".png"))
				found = path + ".png";
			else if (FileIO.FileExists(path + ".bmp"))
				found = path + ".bmp";

			if (!found.IsEmpty())
				break;
		}

		if (found.IsEmpty())
		{
			Note("error no_file " + name);
			return false;
		}

		Note(string.Format("ok shot %1 %2 %3 cam=%4,%5,%6", name, FilePath.StripPath(found), detail, cam[0], cam[1], cam[2]));
		return true;
	}
}
