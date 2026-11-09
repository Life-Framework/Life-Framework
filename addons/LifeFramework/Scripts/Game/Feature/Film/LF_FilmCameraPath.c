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
	//! Smoothstep ease so each segment starts and stops without a jolt.
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

		for (int i = 1; i < count; i++)
		{
			LF_FilmKey b = m_aKeys[i];
			if (t > b.Time)
				continue;

			LF_FilmKey a = m_aKeys[i - 1];
			float u = Ease((t - a.Time) / (b.Time - a.Time));
			position = a.Position + (b.Position - a.Position) * u;
			lookAt = a.LookAt + (b.LookAt - a.LookAt) * u;
			fov = a.Fov + (b.Fov - a.Fov) * u;
			return true;
		}

		return false;
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
	protected static const ResourceName CAMERA_PREFAB = "Prefabs/Editor/Camera/ManualCameraBase.et";

	protected static ref LF_FilmCameraPath s_Path;
	protected static float s_fStartMs;
	protected static EntityID s_CameraId = EntityID.INVALID;
	protected static EntityID s_PreviousCameraId = EntityID.INVALID;
	protected static bool s_bHudWasVisible;

	//------------------------------------------------------------------------------------------------
	static bool IsPlaying()
	{
		return s_Path != null;
	}

	//------------------------------------------------------------------------------------------------
	static bool Play(LF_FilmCameraPath path)
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
			Apply(camera, position, lookAt, fov);

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
		s_CameraId = EntityID.INVALID;
		s_PreviousCameraId = EntityID.INVALID;
		LF_Debug.Log(FEATURE, string.Format("camera pass %1", result));
	}
};
