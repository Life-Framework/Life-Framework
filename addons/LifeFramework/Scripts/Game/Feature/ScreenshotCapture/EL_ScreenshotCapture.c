// EL_ScreenshotCapture.c - capture the current viewport (or aim a camera at a
// world target) and write a BMP via System.MakeScreenshot.
//
// This runs on a RENDERING instance only - Workbench play mode, a game client,
// or a player-hosted server. A headless dedicated server has no viewport, so
// every capture path reports a clean error and never crashes (fail-safe rule).
// The MCP `wb_capture` tool drives this over the Workbench NET API; anything
// with a renderer can call StartCapture() directly and poll IsBusy()/
// GetLastResult().
//
// Output lands in "$logs:el-captures/<name>.bmp" by default - outside the
// addon tree, so a capture never pollutes the repo or the resource database.

class EL_ScreenshotShot
{
	vector TargetPosition = "0 0 0";
	vector CameraOffset = "10 3 10"; // camera placed at target + offset, aimed at target
	string OutPath;                  // "$logs:" alias or relative path; empty = auto
	string CaptureName;              // used for the auto path; must be filesystem-safe
	bool AimAtTarget = true;         // false = leave the current camera, capture as-is
	float SettleTime = 1.0;          // seconds after aiming/spawning before capture
	float CaptureWaitTime = 1.0;     // seconds to wait for the BMP to land on disk
	ResourceName SpawnPrefab;        // optional prefab spawned at TargetPosition first
	vector SpawnOrientation = "0 0 0";
	float TimeOfDay24h = -1;         // < 0 = don't touch the clock
	string WeatherStateName;           // empty = don't touch the weather
};

class EL_ScreenshotCapture : Managed
{
	protected static const string FEATURE = "Screenshot";

	protected static ref EL_ScreenshotShot s_ActiveShot;
	protected static bool s_bBusy;
	protected static string s_sResult = "";
	protected static string s_sLastPath = "";
	protected static vector s_vOriginalPos;
	protected static bool s_bCameraMoved;
	protected static EntityID s_SpawnedEntityId = EntityID.INVALID;
	protected static int s_iCaptureCounter;

	//------------------------------------------------------------------------------------------------
	static bool IsBusy()
	{
		return s_bBusy;
	}

	//------------------------------------------------------------------------------------------------
	//! Machine-readable outcome of the last capture: "ok", "warn:file_not_found",
	//! or "error:<reason>".
	static string GetLastResult()
	{
		return s_sResult;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastPath()
	{
		return s_sLastPath;
	}

	//------------------------------------------------------------------------------------------------
	//! The output path of the in-flight (or just-started) capture, or "" when idle.
	static string GetActivePath()
	{
		if (s_ActiveShot)
			return s_ActiveShot.OutPath;
		return s_sLastPath;
	}

	//------------------------------------------------------------------------------------------------
	//! Start an async capture. Returns false (and leaves a clean error in
	//! GetLastResult) when already busy or when the instance cannot satisfy the
	//! request. On success the capture runs on the callqueue; poll IsBusy().
	static bool StartCapture(EL_ScreenshotShot shot)
	{
		if (s_bBusy)
		{
			EL_Debug.Error(FEATURE, "capture rejected: already busy");
			s_sResult = "error:busy";
			return false;
		}

		s_sResult = "";
		s_sLastPath = "";
		s_bCameraMoved = false;
		s_SpawnedEntityId = EntityID.INVALID;

		if (!shot)
		{
			EL_Debug.Error(FEATURE, "capture rejected: null shot");
			s_sResult = "error:null_shot";
			return false;
		}

		Game game = GetGame();
		if (!game)
		{
			EL_Debug.Error(FEATURE, "capture rejected: no game");
			s_sResult = "error:no_game";
			return false;
		}

		BaseWorld world = game.GetWorld();
		if (!world)
		{
			EL_Debug.Error(FEATURE, "capture rejected: no world");
			s_sResult = "error:no_world";
			return false;
		}

		if (shot.OutPath.IsEmpty())
			shot.OutPath = BuildAutoPath(shot.CaptureName);

		// Create the output directory (best effort - failure is non-fatal).
		FileIO.MakeDirectory(FilePath.StripFileName(shot.OutPath));

		s_ActiveShot = shot;
		s_bBusy = true;

		if (shot.SpawnPrefab)
		{
			IEntity spawned = EL_Utils.SpawnEntityPrefab(shot.SpawnPrefab, shot.TargetPosition, shot.SpawnOrientation);
			if (!spawned)
			{
				Complete("error:spawn_failed", shot.OutPath);
				return false;
			}
			s_SpawnedEntityId = spawned.GetID();
			EL_Debug.Log(FEATURE, string.Format("spawned fixture %1 at %2 %3 %4", shot.SpawnPrefab, shot.TargetPosition[0], shot.TargetPosition[1], shot.TargetPosition[2]));
		}

		SetEnvironment(world, shot);

		if (shot.AimAtTarget)
		{
			if (!AimCamera(shot.TargetPosition, shot.CameraOffset))
			{
				Complete("error:no_camera", shot.OutPath);
				return false;
			}

			// Stream the area in so the target is actually visible at capture time.
			game.BeginPreload(world, shot.TargetPosition, 200);
			EL_Debug.Log(FEATURE, string.Format("aimed camera at %1 %2 %3", shot.TargetPosition[0], shot.TargetPosition[1], shot.TargetPosition[2]));
		}

		int settleMs = Math.Round(shot.SettleTime * 1000);
		if (settleMs < 50)
			settleMs = 50;
		GetGame().GetCallqueue().CallLater(CaptureFrame, settleMs, false);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildAutoPath(string name)
	{
		string safe = name;
		if (safe.IsEmpty())
			safe = "capture";

		string cleaned = "";
		for (int i = 0; i < safe.Length(); i++)
		{
			string ch = safe.Substring(i, 1);
			if (ch == " " || ch == "/" || ch == "\\" || ch == ":" || ch == "." || ch == "*" || ch == "?")
				cleaned += "_";
			else
				cleaned += ch;
		}

		s_iCaptureCounter++;
		return string.Format("$logs:el-captures/%1_%2.bmp", cleaned, s_iCaptureCounter);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CaptureFrame()
	{
		if (!s_ActiveShot)
			return;

		// BMP lands asynchronously over the next few rendered frames.
		EL_Debug.Log(FEATURE, string.Format("capturing to %1", s_ActiveShot.OutPath));
		System.MakeScreenshot(s_ActiveShot.OutPath);

		int waitMs = Math.Round(s_ActiveShot.CaptureWaitTime * 1000);
		if (waitMs < 250)
			waitMs = 250;
		GetGame().GetCallqueue().CallLater(VerifyFrame, waitMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void VerifyFrame()
	{
		if (!s_ActiveShot)
			return;

		string path = s_ActiveShot.OutPath;
		string result;
		if (FileIO.FileExists(path))
		{
			result = "ok";
			EL_Debug.Log(FEATURE, string.Format("captured %1", path));
		}
		else
		{
			EL_Debug.Warn(FEATURE, string.Format("file not found after capture: %1", path));
			result = "warn:file_not_found";
		}

		Complete(result, path);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Complete(string result, string path)
	{
		RestoreCamera();

		if (s_SpawnedEntityId != EntityID.INVALID)
		{
			IEntity spawned = GetGame().GetWorld().FindEntityByID(s_SpawnedEntityId);
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			s_SpawnedEntityId = EntityID.INVALID;
		}

		s_sResult = result;
		s_sLastPath = path;
		s_bBusy = false;
		s_ActiveShot = null;

		EL_Debug.Log(FEATURE, string.Format("done result=%1 path=%2", result, path));
	}

	//------------------------------------------------------------------------------------------------
	//! Reuse a camera already registered with the CameraManager (the proven
	//! Screenshot_Autotest approach) - place it at target + offset, aimed at
	//! target. Returns false when no camera is available (headless server).
	protected static bool AimCamera(vector target, vector offset)
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return false;

		array<CameraBase> cameras = {};
		cameraManager.GetCamerasList(cameras);
		if (cameras.IsEmpty())
			return false;

		CameraBase camera = cameras[0];
		if (!camera)
			return false;

		vector original[4];
		camera.GetTransform(original);
		s_vOriginalPos = original[3];
		s_bCameraMoved = true;

		vector cameraPos = target + offset;
		if (vector.DistanceSq(target, cameraPos) < 0.01)
			cameraPos = target + "10 3 10";

		vector mat[4];
		Math3D.DirectionAndUpMatrix(vector.Direction(cameraPos, target).Normalized(), vector.Up, mat);
		mat[3] = cameraPos;
		camera.SetTransform(mat);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Put the first registered camera back where it was. Re-acquired by list
	//! position rather than held across frames (CameraBase cannot be a strong
	//! ref field, and the camera list is stable during a capture).
	protected static void RestoreCamera()
	{
		if (!s_bCameraMoved)
			return;

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager)
		{
			array<CameraBase> cameras = {};
			cameraManager.GetCamerasList(cameras);
			if (!cameras.IsEmpty())
			{
				CameraBase camera = cameras[0];
				if (camera)
				{
					vector original[4];
					camera.GetTransform(original);
					original[3] = s_vOriginalPos;
					camera.SetTransform(original);
				}
			}
		}

		s_bCameraMoved = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Best-effort time/weather pinning; never fatal when the manager is absent.
	protected static void SetEnvironment(BaseWorld world, EL_ScreenshotShot shot)
	{
		BaseWeatherManagerEntity weather = BaseWeatherManagerEntity.Cast(WeatherManager.GetRegisteredWeatherManagerEntity(world));
		if (!weather)
			return;

		if (shot.TimeOfDay24h >= 0)
		{
			weather.SetTimeOfTheDay(shot.TimeOfDay24h);
			EL_Debug.Log(FEATURE, string.Format("set time of day to %1", shot.TimeOfDay24h));
		}

		if (!shot.WeatherStateName.IsEmpty())
		{
			BaseWeatherStateTransitionManager transitionManager = weather.GetTransitionManager();
			if (transitionManager)
			{
				WeatherStateTransitionNode node = transitionManager.CreateStateTransition(shot.WeatherStateName, 0.1, 0.1);
				if (node)
				{
					transitionManager.EnqueueStateTransition(node, false);
					transitionManager.RequestStateTransitionImmediately(node);
				}
			}
		}
	}
};