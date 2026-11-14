/**
 * EMCP_WB_Preview.c - Edit-mode camera and Resource Manager preview capture.
 *
 * Non-blocking by design: no Sleep() in a NetApiHandler (it raises "Cannot stop
 * main thread" and blocks the render thread that writes screenshots). The
 * TypeScript side (wb_preview) sequences the phases and waits in Node:
 *   action="setCamera"   -> WorldEditorAPI.SetCamera(pos, normalised dir)
 *   action="open"        -> ResourceManager.SetOpenedResource(resource)
 *   action="screenshot"  -> System.MakeScreenshot(path); returns after queuing
 * Called via NET API TCP protocol: APIFunc = "EMCP_WB_Preview"
 */

class EMCP_WB_PreviewRequest : JsonApiStruct
{
	string action;
	string position;
	string direction;
	string resource;
	string path;

	void EMCP_WB_PreviewRequest()
	{
		RegV("action");
		RegV("position");
		RegV("direction");
		RegV("resource");
		RegV("path");
	}
}

class EMCP_WB_PreviewResponse : JsonApiStruct
{
	string status;
	string message;
	string position;
	string path;

	void EMCP_WB_PreviewResponse()
	{
		RegV("status");
		RegV("message");
		RegV("position");
		RegV("path");
	}
}

class EMCP_WB_Preview : NetApiHandler
{
	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetRequest()
	{
		return new EMCP_WB_PreviewRequest();
	}

	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		EMCP_WB_PreviewRequest req = EMCP_WB_PreviewRequest.Cast(request);
		EMCP_WB_PreviewResponse resp = new EMCP_WB_PreviewResponse();

		if (req.action == "setCamera")
			return SetCamera(req, resp);
		if (req.action == "open")
			return OpenResource(req, resp);
		if (req.action == "screenshot")
			return Screenshot(req, resp);

		resp.status = "error";
		resp.message = "Unknown action '" + req.action + "' (expected setCamera|open|screenshot)";
		return resp;
	}

	//------------------------------------------------------------------------------------------------
	protected JsonApiStruct SetCamera(EMCP_WB_PreviewRequest req, EMCP_WB_PreviewResponse resp)
	{
		if (req.position.IsEmpty() || req.direction.IsEmpty())
		{
			resp.status = "error";
			resp.message = "setCamera needs position and direction ('x y z')";
			return resp;
		}

		vector pos = ParseVector(req.position);
		vector dir = ParseVector(req.direction);
		if (dir.Length() < 0.0001)
		{
			resp.status = "error";
			resp.message = "direction must be non-zero";
			return resp;
		}
		dir.Normalize();

		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
		{
			resp.status = "error";
			resp.message = "WorldEditor module not available";
			return resp;
		}

		WorldEditorAPI api = worldEditor.GetApi();
		if (!api)
		{
			resp.status = "error";
			resp.message = "WorldEditorAPI not available (in game mode?)";
			return resp;
		}

		api.SetCamera(pos, dir);

		// Read back what the viewport now holds so the caller can compare.
		BaseWorld world = api.GetWorld();
		if (world)
		{
			vector camMat[4];
			world.GetCurrentCamera(camMat);
			vector got = camMat[3];
			resp.position = got[0].ToString() + " " + got[1].ToString() + " " + got[2].ToString();
		}
		resp.status = "ok";
		resp.message = "Camera set";
		return resp;
	}

	//------------------------------------------------------------------------------------------------
	protected JsonApiStruct OpenResource(EMCP_WB_PreviewRequest req, EMCP_WB_PreviewResponse resp)
	{
		if (req.resource.IsEmpty())
		{
			resp.status = "error";
			resp.message = "open needs resource ({GUID}path or $Addon:path)";
			return resp;
		}

		ResourceManager rm = Workbench.GetModule(ResourceManager);
		if (!rm)
		{
			resp.status = "error";
			resp.message = "ResourceManager module not available";
			return resp;
		}

		if (!rm.SetOpenedResource(req.resource))
		{
			resp.status = "error";
			resp.message = "SetOpenedResource rejected '" + req.resource + "'";
			return resp;
		}

		resp.status = "ok";
		resp.message = "Resource opened";
		return resp;
	}

	//------------------------------------------------------------------------------------------------
	protected JsonApiStruct Screenshot(EMCP_WB_PreviewRequest req, EMCP_WB_PreviewResponse resp)
	{
		if (req.path.IsEmpty())
		{
			resp.status = "error";
			resp.message = "screenshot needs path (stem, e.g. $logs:lfp_shot1)";
			return resp;
		}

		// Queues the capture; the render thread writes the file later.
		if (!System.MakeScreenshot(req.path))
		{
			resp.status = "error";
			resp.message = "System.MakeScreenshot returned false for '" + req.path + "'";
			return resp;
		}

		resp.status = "queued";
		resp.path = req.path;
		resp.message = "Screenshot queued";
		return resp;
	}

	//------------------------------------------------------------------------------------------------
	static vector ParseVector(string str)
	{
		vector result = "0 0 0";
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
