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
//   close <name> <entity|-> <x> <y> <z> <az>
//     a close-up of the entity a layer placed there (found by name, else the nearest
//     entity with a mesh within 1 m). Framed from its world bounds, from the clearest of
//     eight headings, starting in front of it (its +z axis; az when it was not found).
//     Its status line carries size=<x>,<y>,<z> in metres.
// status.txt gets one line per step (`<ms> ok shot <name> <file> pick= clear= cam=`,
// `<ms> error <reason>`) and a final `done <shots> <failed>`; Node reads it after
// Workbench exits.

[WorkbenchPluginAttribute(name: "LF Visual Shots", description: "Takes the shots of $profile:lf-visual/plan.txt (tools\\cli look)", wbModules: { "WorldEditor" }, category: "Life Framework")]
class LF_VisualShotsPlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-visual";
	protected static const int SHOT_TIMEOUT_MS = 5000;
	protected static const int PLAY_TIMEOUT_MS = 90000;
	protected static const float VERTICAL_FOV = 60;

	protected ref array<string> m_aStatus = {};
	protected int m_iStart;
	protected bool m_bPlay;
	protected WorldEditorAPI m_Api;
	protected BaseWorld m_World;
	protected CameraBase m_Camera;
	protected float m_fHour = -1;
	protected IEntity m_Nearest;
	protected float m_fNearest;
	protected vector m_vNearest;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		m_iStart = System.GetTickCount();
		int failed = Shoot();
		int code = 0;
		if (failed != 0)
			code = 1;

		Workbench.Exit(code);
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
	protected int Shoot()
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
			else if (line.StartsWith("shot ") || line.StartsWith("close "))
				shots.Insert(line);
		}

		plan.Close();

		m_Api = worldEditor.GetApi();
		string opened;
		if (m_Api)
			m_Api.GetWorldPath(opened);

		if (opened.IsEmpty())
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

		int waited;
		while (!m_Api.IsGameMode())
		{
			if (waited > PLAY_TIMEOUT_MS)
			{
				Note("error play_not_started");
				return false;
			}

			Sleep(100);
			waited += 100;
		}

		ArmaReforgerScripted game = GetGame();
		while (!game || !game.GetWorld() || !game.GetCameraManager())
		{
			if (waited > PLAY_TIMEOUT_MS)
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
		Note("ok play waited=" + waited);
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
	protected bool TakeShot(string shot, int settleMs, bool first, int firstMs)
	{
		array<string> p = {};
		shot.Split(" ", p, true);
		if (p[0] == "close")
			return TakeClose(p, shot, settleMs, first, firstMs);

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
		IEntity entity;
		if (p[2] != "-")
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
