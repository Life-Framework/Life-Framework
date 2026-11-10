/**
 * EMCP_WB_Capture.c - Capture a screenshot in the running Workbench game.
 *
 * Two-phase protocol because System.MakeScreenshot writes asynchronously:
 *   action="start"  -> builds an LF_ScreenshotShot and kicks off the async
 *                      capture on LF_ScreenshotCapture. Returns immediately
 *                      with the target $logs: path.
 *   action="poll"   -> reports busy/done/error + the final result/path.
 *
 * Works in play mode (renderer active). On a renderless instance the capture
 * degrades to a clean error (see LF_ScreenshotCapture fail-safe).
 * Called via NET API TCP protocol: APIFunc = "EMCP_WB_Capture"
 */

class EMCP_WB_CaptureRequest : JsonApiStruct
{
	string action;
	string prefab;
	string position;
	string cameraOffset;
	bool aimAtTarget;
	string name;
	string outPath;
	string settleTime;
	string timeOfDay;
	string weather;

	void EMCP_WB_CaptureRequest()
	{
		RegV("action");
		RegV("prefab");
		RegV("position");
		RegV("cameraOffset");
		RegV("aimAtTarget");
		RegV("name");
		RegV("outPath");
		RegV("settleTime");
		RegV("timeOfDay");
		RegV("weather");
		aimAtTarget = true;
	}
}

class EMCP_WB_CaptureResponse : JsonApiStruct
{
	string status;
	string result;
	string path;
	string message;

	void EMCP_WB_CaptureResponse()
	{
		RegV("status");
		RegV("result");
		RegV("path");
		RegV("message");
	}
}

class EMCP_WB_Capture : NetApiHandler
{
	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetRequest()
	{
		return new EMCP_WB_CaptureRequest();
	}

	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		EMCP_WB_CaptureRequest req = EMCP_WB_CaptureRequest.Cast(request);
		EMCP_WB_CaptureResponse resp = new EMCP_WB_CaptureResponse();

		string action = req.action;
		if (action == "")
			action = "start";

		if (action == "poll")
		{
			if (LF_ScreenshotCapture.IsBusy())
			{
				resp.status = "busy";
				resp.message = "Capture in progress";
				return resp;
			}

			string result = LF_ScreenshotCapture.GetLastResult();
			resp.result = result;
			resp.path = LF_ScreenshotCapture.GetLastPath();
			if (result.StartsWith("error:"))
			{
				resp.status = "error";
				resp.message = "Capture failed: " + result;
			}
			else
			{
				resp.status = "done";
				resp.message = "Capture complete";
			}
			return resp;
		}

		if (action != "start")
		{
			resp.status = "error";
			resp.result = "error:bad_action";
			resp.message = "Unknown action '" + action + "' (expected start|poll)";
			return resp;
		}

		LF_ScreenshotShot shot = new LF_ScreenshotShot();
		if (!req.position.IsEmpty())
			shot.TargetPosition = ParseVector(req.position);
		if (!req.cameraOffset.IsEmpty())
			shot.CameraOffset = ParseVector(req.cameraOffset);
		shot.AimAtTarget = req.aimAtTarget;
		shot.CaptureName = req.name;
		shot.OutPath = req.outPath;
		shot.SpawnPrefab = req.prefab;
		if (!req.settleTime.IsEmpty())
			shot.SettleTime = req.settleTime.ToFloat();
		if (!req.timeOfDay.IsEmpty())
			shot.TimeOfDay24h = req.timeOfDay.ToFloat();
		shot.WeatherStateName = req.weather;

		if (LF_ScreenshotCapture.StartCapture(shot))
		{
			resp.status = "started";
			resp.path = LF_ScreenshotCapture.GetActivePath();
			resp.message = "Capture started";
		}
		else
		{
			resp.status = "error";
			resp.result = LF_ScreenshotCapture.GetLastResult();
			resp.message = "Capture rejected: " + resp.result;
		}

		return resp;
	}

	//------------------------------------------------------------------------------------------------
	static vector ParseVector(string str)
	{
		vector result = "0 0 0";
		if (str == "")
			return result;

		array<string> parts = {};
		str.Split(" ", parts, true);
		if (parts.Count() >= 3)
		{
			result[0] = parts[0].ToFloat();
			result[1] = parts[1].ToFloat();
			result[2] = parts[2].ToFloat();
		}
		return result;
	}
}