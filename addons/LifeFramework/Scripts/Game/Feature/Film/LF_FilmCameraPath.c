// LF_FilmCameraPath.c - keyframed camera pass for the filming harness.
//
// Dev-only, like LF_AgentHarness: a rendering session flies a camera along
// timed keys while tools/film records the window. The path math is pure
// (LF_FilmCameraPath.Sample) so the fast suite covers it; LF_FilmCameraRunner
// is the rendering half and does nothing useful on a headless server.
//
// Engine calls used here are the ones LF_ScreenshotCapture already compiles
// against (CameraManager, CameraBase.SetTransform, Math3D.DirectionAndUpMatrix)
// plus CameraBase.SetVerticalFOV, SCR_ManualCamera.SetInputEnabled and
// SCR_HUDManagerComponent.SetVisible (api_search, 2026-11-07).

class LF_FilmKey
{
	float Time;      // seconds from path start
	vector Position; // camera position
	vector LookAt;   // point the camera aims at
	float Fov;       // vertical field of view, degrees
};

class LF_FilmCameraPath : Managed
{
	protected static const string FEATURE = "Film";

	protected ref array<ref LF_FilmKey> m_aKeys = {};

	//------------------------------------------------------------------------------------------------
	//! Append a key. Times must strictly increase; an out-of-order key is
	//! rejected (logged) so a typo in a showcase script cannot fold the path.
	bool Add(float time, vector position, vector lookAt, float fov = 60)
	{
		if (time < 0 || (!m_aKeys.IsEmpty() && time <= m_aKeys[m_aKeys.Count() - 1].Time))
		{
			LF_Debug.Error(FEATURE, string.Format("key rejected: time %1 is not after the previous key", time));
			return false;
		}

		LF_FilmKey key = new LF_FilmKey();
		key.Time = time;
		key.Position = position;
		key.LookAt = lookAt;
		key.Fov = fov;
		m_aKeys.Insert(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aKeys.Count();
	}

	//------------------------------------------------------------------------------------------------
	float GetDuration()
	{
		if (m_aKeys.IsEmpty())
			return 0;
		return m_aKeys[m_aKeys.Count() - 1].Time;
	}

	//------------------------------------------------------------------------------------------------
	//! An orbit round point: radius m out, height m up, looking at the point + lookHeight. The arc is centred on
	//! azimuthDeg (0 = the +z side, 90 = +x, 180 = -z, 270 = -x) and spans arcDeg, so a front facing any way
	//! can be circled. A negative arcDeg sweeps the other way.
	static LF_FilmCameraPath BuildOrbit(vector point, float radius, float seconds, float azimuthDeg = 0, float arcDeg = 140, float height = 5, float lookHeight = 1.2, float fov = 60)
	{
		LF_FilmCameraPath path = new LF_FilmCameraPath();
		int steps = 12;
		for (int i = 0; i <= steps; i++)
		{
			float a = (azimuthDeg - arcDeg * 0.5 + i * arcDeg / steps) * Math.DEG2RAD;
			path.Add(i * seconds / steps, point + Vector(Math.Sin(a) * radius, height, Math.Cos(a) * radius), point + Vector(0, lookHeight, 0), fov);
		}
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! A fixed camera: position and look-at held for the whole shot (two identical keys, so the runner has a duration).
	static LF_FilmCameraPath BuildStatic(vector position, vector lookAt, float seconds, float fov = 50)
	{
		LF_FilmCameraPath path = new LF_FilmCameraPath();
		path.Add(0, position, lookAt, fov);
		path.Add(seconds, position, lookAt, fov);
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! A drone move between two poses, eased at both ends. The look-at point travels too.
	static LF_FilmCameraPath BuildDrone(vector from, vector lookFrom, vector to, vector lookTo, float seconds, float fov = 60)
	{
		LF_FilmCameraPath path = new LF_FilmCameraPath();
		path.Add(0, from, lookFrom, fov);
		path.Add(seconds, to, lookTo, fov);
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! Smoothstep ease so each segment starts and stops without a jolt.
	//! One step of a first-order low-pass: the share of the way from the smoothed point to the target to move in
	//! dt seconds with time constant tau. A followed car is driven in 100 ms velocity steps and its origin
	//! jitters; the camera follows the smoothed point instead, so the pan is steady (tau <= 0 or dt <= 0: no lag).
	static float SmoothAlpha(float dt, float tau)
	{
		if (dt <= 0)
			return 0;
		if (tau <= 0)
			return 1;
		return dt / (tau + dt);
	}

	//------------------------------------------------------------------------------------------------
	static float Ease(float u)
	{
		if (u <= 0)
			return 0;
		if (u >= 1)
			return 1;
		return u * u * (3 - 2 * u);
	}

	//------------------------------------------------------------------------------------------------
	//! Camera state at time t (seconds). Clamps to the first and last key.
	//! Returns false on an empty path.
	bool Sample(float t, out vector position, out vector lookAt, out float fov)
	{
		int count = m_aKeys.Count();
		if (count == 0)
			return false;

		LF_FilmKey first = m_aKeys[0];
		if (count == 1 || t <= first.Time)
		{
			position = first.Position;
			lookAt = first.LookAt;
			fov = first.Fov;
			return true;
		}

		LF_FilmKey last = m_aKeys[count - 1];
		if (t >= last.Time)
		{
			position = last.Position;
			lookAt = last.LookAt;
			fov = last.Fov;
			return true;
		}

		// Two keys (static, drone): one eased move.
		if (count == 2)
		{
			float u = Ease((t - first.Time) / (last.Time - first.Time));
			position = first.Position + (last.Position - first.Position) * u;
			lookAt = first.LookAt + (last.LookAt - first.LookAt) * u;
			fov = first.Fov + (last.Fov - first.Fov) * u;
			return true;
		}

		// More keys (orbit, follow): ease the pass as a whole, then run a Catmull-Rom curve through the keys. Easing
		// each segment instead stopped the camera at every key, a visible pulse once a second in a 12-key orbit.
		float span = last.Time - first.Time;
		float tp = first.Time + Ramp((t - first.Time) / span) * span;
		for (int i = 1; i < count; i++)
		{
			LF_FilmKey b = m_aKeys[i];
			if (tp > b.Time)
				continue;

			LF_FilmKey a = m_aKeys[i - 1];
			LF_FilmKey before = m_aKeys[Math.Max(i - 2, 0)];
			LF_FilmKey after = m_aKeys[Math.Min(i + 1, count - 1)];
			float s = (tp - a.Time) / (b.Time - a.Time);
			position = CatmullRom(before.Position, a.Position, b.Position, after.Position, s);
			lookAt = CatmullRom(before.LookAt, a.LookAt, b.LookAt, after.LookAt, s);
			fov = a.Fov + (b.Fov - a.Fov) * s;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the pass covered at u (0..1) with a steady speed in the middle and a quarter of the time spent
	//! speeding up at the start and slowing down at the end (a trapezoid speed profile).
	static float Ramp(float u)
	{
		if (u <= 0)
			return 0;
		if (u >= 1)
			return 1;
		float r = 0.25;
		float top = 1 / (1 - r);
		if (u < r)
			return top * u * u / (2 * r);
		if (u > 1 - r)
			return 1 - top * (1 - u) * (1 - u) / (2 * r);
		return top * (r * 0.5 + u - r);
	}

	//------------------------------------------------------------------------------------------------
	//! Uniform Catmull-Rom point between b (s = 0) and c (s = 1), with a and d the neighbouring keys.
	static vector CatmullRom(vector a, vector b, vector c, vector d, float s)
	{
		float s2 = s * s;
		float s3 = s2 * s;
		return ((b * 2) + (c - a) * s + (a * 2 - b * 5 + c * 4 - d) * s2 + (b * 3 - a - c * 3 + d) * s3) * 0.5;
	}
};

//------------------------------------------------------------------------------------------------
//! Plays one LF_FilmCameraPath on a rendering instance: spawns a manual
//! camera with player input off, hides the HUD, moves the camera every frame,
//! then restores the previous camera and the HUD. Not yet wired to an
//! agent-box verb (see tools/film/README.md, "What remains").
class LF_FilmCameraRunner
{
	protected static const string FEATURE = "Film";
	protected static const ResourceName CAMERA_PREFAB = "{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et";

	protected static ref LF_FilmCameraPath s_Path;
	protected static float s_fStartMs;
	protected static EntityID s_CameraId = EntityID.INVALID;
	protected static EntityID s_PreviousCameraId = EntityID.INVALID;
	protected static bool s_bHudWasVisible;
	protected static EntityID s_FollowId = EntityID.INVALID;
	protected static vector s_vFollowLast;
	protected static float s_fFollowMs;
	protected static const float FOLLOW_SMOOTH_TAU = 0.25;   // seconds of lag on the followed point

	//------------------------------------------------------------------------------------------------
	static bool IsPlaying()
	{
		return s_Path != null;
	}

	//------------------------------------------------------------------------------------------------
	//! With followId set, the path's positions and look-at points are offsets from that entity.
	static bool Play(LF_FilmCameraPath path, EntityID followId = EntityID.INVALID)
	{
		if (s_Path)
		{
			LF_Debug.Error(FEATURE, "play rejected: a camera pass is already running");
			return false;
		}
		if (!path || path.Count() == 0)
		{
			LF_Debug.Error(FEATURE, "play rejected: empty path");
			return false;
		}

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
		{
			LF_Debug.Error(FEATURE, "play rejected: no world");
			return false;
		}
		CameraManager cameraManager = game.GetCameraManager();
		if (!cameraManager)
		{
			LF_Debug.Error(FEATURE, "play rejected: no camera manager (headless?)");
			return false;
		}

		vector position;
		vector lookAt;
		float fov;
		path.Sample(0, position, lookAt, fov);
		s_FollowId = followId;
		if (followId != EntityID.INVALID)
		{
			IEntity followed = game.GetWorld().FindEntityByID(followId);
			if (!followed)
			{
				LF_Debug.Error(FEATURE, "play rejected: follow entity not found");
				s_FollowId = EntityID.INVALID;
				return false;
			}
			s_vFollowLast = followed.GetOrigin();
			s_fFollowMs = game.GetWorld().GetWorldTime();
			position += s_vFollowLast;
			lookAt += s_vFollowLast;
		}

		IEntity spawned = LF_Utils.SpawnEntityPrefab(CAMERA_PREFAB, position, "0 0 0", false);
		CameraBase camera = CameraBase.Cast(spawned);
		if (!camera)
		{
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			LF_Debug.Error(FEATURE, "play rejected: camera prefab did not spawn a CameraBase");
			return false;
		}

		SCR_ManualCamera manual = SCR_ManualCamera.Cast(camera);
		if (manual)
			manual.SetInputEnabled(false);

		CameraBase previous = cameraManager.CurrentCamera();
		if (previous)
			s_PreviousCameraId = previous.GetID();
		s_CameraId = camera.GetID();
		cameraManager.SetCamera(camera);

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		s_bHudWasVisible = false;
		if (hud)
		{
			s_bHudWasVisible = hud.IsVisible();
			hud.SetVisible(false);
		}

		s_Path = path;
		s_fStartMs = game.GetWorld().GetWorldTime();
		Apply(camera, position, lookAt, fov);
		game.GetCallqueue().CallLater(Tick, 0, true);
		LF_Debug.Log(FEATURE, string.Format("camera pass started: %1 keys, %2 s", path.Count(), path.GetDuration()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (!s_Path)
			return;
		Finish("stopped");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_Path)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Finish("error:no_world");
			return;
		}
		CameraBase camera = CameraBase.Cast(world.FindEntityByID(s_CameraId));
		if (!camera)
		{
			Finish("error:camera_gone");
			return;
		}

		float t = (world.GetWorldTime() - s_fStartMs) / 1000.0;
		vector position;
		vector lookAt;
		float fov;
		if (s_Path.Sample(t, position, lookAt, fov))
		{
			if (s_FollowId != EntityID.INVALID)
			{
				IEntity followed = world.FindEntityByID(s_FollowId);
				float nowMs = world.GetWorldTime();
				if (followed)
				{
					float alpha = LF_FilmCameraPath.SmoothAlpha((nowMs - s_fFollowMs) / 1000.0, FOLLOW_SMOOTH_TAU);
					s_vFollowLast = s_vFollowLast + (followed.GetOrigin() - s_vFollowLast) * alpha;
				}
				s_fFollowMs = nowMs;
				position += s_vFollowLast;
				lookAt += s_vFollowLast;
			}
			Apply(camera, position, lookAt, fov);
		}

		if (t >= s_Path.GetDuration())
			Finish("done");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Apply(CameraBase camera, vector position, vector lookAt, float fov)
	{
		vector dir = lookAt - position;
		if (dir.LengthSq() < 0.0001)
			dir = "0 0 1";

		vector mat[4];
		Math3D.DirectionAndUpMatrix(dir.Normalized(), vector.Up, mat);
		mat[3] = position;
		camera.SetTransform(mat);
		camera.SetVerticalFOV(fov);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(string result)
	{
		GetGame().GetCallqueue().Remove(Tick);

		BaseWorld world = GetGame().GetWorld();
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (world && cameraManager && s_PreviousCameraId != EntityID.INVALID)
		{
			CameraBase previous = CameraBase.Cast(world.FindEntityByID(s_PreviousCameraId));
			if (previous)
				cameraManager.SetCamera(previous);
		}
		if (world && s_CameraId != EntityID.INVALID)
		{
			IEntity camera = world.FindEntityByID(s_CameraId);
			if (camera)
				SCR_EntityHelper.DeleteEntityAndChildren(camera);
		}

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud && s_bHudWasVisible)
			hud.SetVisible(true);

		s_Path = null;
		s_FollowId = EntityID.INVALID;
		s_CameraId = EntityID.INVALID;
		s_PreviousCameraId = EntityID.INVALID;
		LF_Debug.Log(FEATURE, string.Format("camera pass %1", result));
	}
};

//------------------------------------------------------------------------------------------------
//! Clean frame for a first-person shot (#392): HUD off and any hint pop-up kept down until Show. A hint
//! (the arrival note, a tip) can come back at any time, so a repeating call hides it again.
class LF_FilmClean
{
	protected static const string FEATURE = "Film";
	protected static const int HINT_MS = 400;
	protected static bool s_bHidden;
	protected static bool s_bWasVisible;

	//------------------------------------------------------------------------------------------------
	static bool IsHidden()
	{
		return s_bHidden;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (s_bHidden)
			return;
		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		s_bWasVisible = false;
		if (hud)
		{
			s_bWasVisible = hud.IsVisible();
			hud.SetVisible(false);
		}
		SCR_HintManagerComponent.HideHint();
		s_bHidden = true;
		GetGame().GetCallqueue().CallLater(KeepHintsDown, HINT_MS, true);
		LF_Debug.Info(FEATURE, "clean frame: HUD and hints off");
	}

	//------------------------------------------------------------------------------------------------
	static void Show()
	{
		if (!s_bHidden)
			return;
		GetGame().GetCallqueue().Remove(KeepHintsDown);
		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud && s_bWasVisible)
			hud.SetVisible(true);
		s_bHidden = false;
		LF_Debug.Info(FEATURE, "clean frame: HUD back");
	}

	//------------------------------------------------------------------------------------------------
	protected static void KeepHintsDown()
	{
		SCR_HintManagerComponent.HideHint();
	}
};

//------------------------------------------------------------------------------------------------
//! Harness `camera ...`: ready-made passes for the runner (world metres, degrees, seconds).
//!   orbit  x y z radius seconds [azimuth arc height] : an arc round a point; azimuth 0 = +z side, 90 = +x, 180 = -z,
//!          270 = -x (defaults 0 140 5, the original +z sweep).
//!   follow x y z distance seconds : find the vehicle nearest x y z (client side, ids differ from the server),
//!          then swing around it at `distance` m, 3 m up, 160 degrees over the pass.
//!   static px py pz lx ly lz seconds [fov] : a fixed camera at p looking at l.
//!   drone  px py pz lx ly lz qx qy qz mx my mz seconds [fov] : move from p (looking at l) to q (looking at m), eased.
//!   hud off|on : clean frame for a first-person shot (HUD and hints).
//!   stop : end the running pass.
class LF_FilmCamera
{
	protected static const string FEATURE = "Film";

	//------------------------------------------------------------------------------------------------
	//! Split text on spaces into numbers; false when fewer than count, or any token is not a number.
	static bool Numbers(string text, int count, out array<float> values)
	{
		values = {};
		array<string> parts = {};
		text.Split(" ", parts, true);
		if (parts.Count() < count)
			return false;
		foreach (string part : parts)
		{
			// IsNumber has no sign; a coordinate or an azimuth can be negative.
			string digits = part;
			if (digits.StartsWith("-"))
				digits = digits.Substring(1, digits.Length() - 1);
			if (!LF_FilmSceneEnv.IsNumber(digits))
				return false;
			values.Insert(part.ToFloat());
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Modes that carry all their numbers in `args` (static, drone, hud, stop). orbit and follow keep their
	//! positional form and go through Start.
	static string Command(string mode, string args)
	{
		array<float> v;
		if (mode == "stop")
		{
			LF_FilmCameraRunner.Stop();
			return "stopped";
		}
		if (mode == "hud")
		{
			if (args == "off")
				LF_FilmClean.Hide();
			else if (args == "on")
				LF_FilmClean.Show();
			else
				return "error:hud_on_or_off";
			return "hud " + args;
		}
		if (mode == "static")
		{
			if (!Numbers(args, 7, v))
				return "error:static_needs_7_numbers";
			float fovS = 50;
			if (v.Count() > 7)
				fovS = v[7];
			if (v[6] < 1)
				return "error:bad_seconds";
			if (!LF_FilmCameraRunner.Play(LF_FilmCameraPath.BuildStatic(Vector(v[0], v[1], v[2]), Vector(v[3], v[4], v[5]), v[6], fovS)))
				return "error:play_rejected";
			return "static started";
		}
		if (mode == "drone")
		{
			if (!Numbers(args, 13, v))
				return "error:drone_needs_13_numbers";
			float fovD = 60;
			if (v.Count() > 13)
				fovD = v[13];
			if (v[12] < 1)
				return "error:bad_seconds";
			LF_FilmCameraPath drone = LF_FilmCameraPath.BuildDrone(Vector(v[0], v[1], v[2]), Vector(v[3], v[4], v[5]), Vector(v[6], v[7], v[8]), Vector(v[9], v[10], v[11]), v[12], fovD);
			if (!LF_FilmCameraRunner.Play(drone))
				return "error:play_rejected";
			return "drone started";
		}
		return "error:bad_camera_mode";
	}

	//------------------------------------------------------------------------------------------------
	static string Start(string mode, vector point, float size, float seconds, string extra = "")
	{
		if (seconds < 1)
			return "error:bad_seconds";

		int i;
		if (mode == "orbit")
		{
			float azimuth = 0;
			float arc = 140;
			float height = 5;
			array<float> v;
			if (!extra.IsEmpty())
			{
				if (!Numbers(extra, 1, v))
					return "error:orbit_extras_not_numbers";
				azimuth = v[0];
				if (v.Count() > 1)
					arc = v[1];
				if (v.Count() > 2)
					height = v[2];
			}
			if (!LF_FilmCameraRunner.Play(LF_FilmCameraPath.BuildOrbit(point, size, seconds, azimuth, arc, height)))
				return "error:play_rejected";
			return "orbit started";
		}

		if (mode != "follow")
			return "error:bad_camera_mode";

		LF_FilmCameraPath path = new LF_FilmCameraPath();
		int steps = 12;
		array<IEntity> near = LF_Utils.GetNearbyVehicles(point, 30);
		if (near.IsEmpty())
			return "error:no_vehicle_near_point";

		IEntity target = near[0];
		float best = vector.DistanceSq(target.GetOrigin(), point);
		foreach (IEntity candidate : near)
		{
			float d = vector.DistanceSq(candidate.GetOrigin(), point);
			if (d < best)
			{
				best = d;
				target = candidate;
			}
		}
		float start = 20 * Math.DEG2RAD;
		for (i = 0; i <= steps; i++)
		{
			float b = start + i * (160 * Math.DEG2RAD) / steps;
			path.Add(i * seconds / steps, Vector(Math.Sin(b) * size, 3, Math.Cos(b) * size), Vector(0, 1, 0), 55);
		}
		if (!LF_FilmCameraRunner.Play(path, target.GetID()))
			return "error:play_rejected";
		return "follow started";
	}
};
