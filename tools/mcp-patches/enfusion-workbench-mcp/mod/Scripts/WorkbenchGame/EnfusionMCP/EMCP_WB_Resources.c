/**
 * EMCP_WB_Resources.c - Resource operations handler
 *
 * Actions: register, rebuild, open, browse
 * Uses the ResourceManager Workbench module and the live ResourceDatabase.
 * Called via NET API TCP protocol: APIFunc = "EMCP_WB_Resources"
 *
 * LF patch (tools/mcp-patches, spec docs/tooling/workbench-tooling-spec.md items 1-2):
 *  - browse: live ResourceDatabase.SearchResources, so freshly registered and generated
 *    resources show up with their real {GUID}path.
 *  - register / rebuild: batch `paths` ('|' separated), `configuration`, and a WaitForFile
 *    acknowledgement per processed path (`waitMs`, 0 skips).
 * List parameters travel as '|' separated strings: nothing is parsed from JSON arrays.
 */

class EMCP_WB_ResourcesRequest : JsonApiStruct
{
	string action;
	string path;
	bool buildRuntime;
	string paths;
	string configuration;
	int waitMs;
	string rootPath;
	string extensions;
	string filters;
	bool recursive;
	int limit;

	void EMCP_WB_ResourcesRequest()
	{
		RegV("action");
		RegV("path");
		RegV("buildRuntime");
		RegV("paths");
		RegV("configuration");
		RegV("waitMs");
		RegV("rootPath");
		RegV("extensions");
		RegV("filters");
		RegV("recursive");
		RegV("limit");
		// Defaults when the field is absent from the JSON.
		waitMs = 5000;
		recursive = true;
		limit = 200;
	}
}

class EMCP_WB_ResourceEntry
{
	string m_sName;
	string m_sPath;
	string m_sType;
	string m_sResourceName;
}

class EMCP_WB_ResourceOutcome
{
	string m_sPath;
	string m_sReason;
}

class EMCP_WB_ResourcesResponse : JsonApiStruct
{
	string status;
	string message;
	string action;
	string path;
	string configuration;
	int entryCount;
	int count;
	int limit;
	bool truncated;
	int processedCount;
	int failedCount;
	ref array<ref EMCP_WB_ResourceEntry> m_aEntries;
	ref array<ref EMCP_WB_ResourceOutcome> m_aProcessed;
	ref array<ref EMCP_WB_ResourceOutcome> m_aFailed;

	void EMCP_WB_ResourcesResponse()
	{
		RegV("status");
		RegV("message");
		RegV("action");
		RegV("path");
		RegV("configuration");
		RegV("entryCount");
		RegV("count");
		RegV("limit");
		RegV("truncated");
		RegV("processedCount");
		RegV("failedCount");
		m_aEntries = {};
		m_aProcessed = {};
		m_aFailed = {};
	}

	override void OnPack()
	{
		if (m_aEntries.Count() > 0)
		{
			StartArray("entries");
			for (int i = 0; i < m_aEntries.Count(); i++)
			{
				EMCP_WB_ResourceEntry e = m_aEntries[i];
				StartObject("");
				StoreString("name", e.m_sName);
				StoreString("path", e.m_sPath);
				StoreString("type", e.m_sType);
				StoreString("resourceName", e.m_sResourceName);
				EndObject();
			}
			EndArray();
		}
		if (m_aProcessed.Count() > 0)
		{
			StartArray("processed");
			for (int p = 0; p < m_aProcessed.Count(); p++)
			{
				StartObject("");
				StoreString("path", m_aProcessed[p].m_sPath);
				StoreString("reason", m_aProcessed[p].m_sReason);
				EndObject();
			}
			EndArray();
		}
		if (m_aFailed.Count() > 0)
		{
			StartArray("failed");
			for (int f = 0; f < m_aFailed.Count(); f++)
			{
				StartObject("");
				StoreString("path", m_aFailed[f].m_sPath);
				StoreString("reason", m_aFailed[f].m_sReason);
				EndObject();
			}
			EndArray();
		}
	}
}

class EMCP_WB_Resources : NetApiHandler
{
	// Collector for one browse call. Filled by the search callback, cleared right after the
	// response is built, so nothing leaks into the next call.
	protected static ref array<ref EMCP_WB_ResourceEntry> s_aBrowseHits;
	protected static int s_iBrowseLimit;
	protected static int s_iBrowseSeen;

	override JsonApiStruct GetRequest()
	{
		return new EMCP_WB_ResourcesRequest();
	}

	//------------------------------------------------------------------------------------------------
	//! SearchResourcesCallback: false halts the enumeration once the limit is reached.
	static bool OnBrowseResult(ResourceName resName, string exactPath = "")
	{
		if (!s_aBrowseHits)
			return false;

		s_iBrowseSeen++;
		if (s_aBrowseHits.Count() >= s_iBrowseLimit)
			return false;

		EMCP_WB_ResourceEntry e = new EMCP_WB_ResourceEntry();
		e.m_sResourceName = resName;
		e.m_sPath = exactPath;
		if (e.m_sPath == "")
			e.m_sPath = resName.GetPath();
		e.m_sName = FilePath.StripPath(e.m_sPath);
		string ext;
		FilePath.StripExtension(e.m_sName, ext);
		e.m_sType = ext;
		s_aBrowseHits.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! '|' separated list to trimmed, non-empty parts.
	protected static void SplitList(string list, array<string> outParts)
	{
		string remaining = list;
		while (remaining != "")
		{
			int idx = remaining.IndexOf("|");
			string part;
			if (idx >= 0)
			{
				part = remaining.Substring(0, idx);
				remaining = remaining.Substring(idx + 1, remaining.Length() - idx - 1);
			}
			else
			{
				part = remaining;
				remaining = "";
			}
			part.Trim();
			if (part != "")
				outParts.Insert(part);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves "$Addon:relative" or a verified absolute path to an absolute file path.
	//! Returns false (reason set) for anything malformed or not on disk.
	protected static bool ResolveAbsolute(string input, out string absPath, out string reason)
	{
		absPath = "";
		reason = "";

		if (input.StartsWith("$"))
		{
			if (!input.Contains(":") || input.IndexOf(":") < 2)
			{
				reason = "malformed addon path (expected $Addon:relative/path)";
				return false;
			}
			if (!Workbench.GetAbsolutePath(input, absPath, true) || absPath == "")
			{
				reason = "could not resolve (missing file or addon not loaded)";
				absPath = "";
				return false;
			}
			return true;
		}

		if (FilePath.IsAbsolutePath(input))
		{
			string verified;
			if (!Workbench.GetAbsolutePath(input, verified, true) || verified == "")
			{
				reason = "absolute path does not exist";
				return false;
			}
			absPath = verified;
			return true;
		}

		reason = "relative path needs an $Addon: prefix";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectPaths(EMCP_WB_ResourcesRequest req, array<string> outPaths)
	{
		SplitList(req.paths, outPaths);
		if (req.path != "" && outPaths.Find(req.path) < 0)
			outPaths.Insert(req.path);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoBrowse(EMCP_WB_ResourcesRequest req, EMCP_WB_ResourcesResponse resp)
	{
		int limit = req.limit;
		if (limit <= 0)
			limit = 200;
		if (limit > 1000)
			limit = 1000;
		resp.limit = limit;

		SearchResourcesFilter filter = new SearchResourcesFilter();
		filter.rootPath = req.rootPath;
		filter.recursive = req.recursive;
		filter.fileExtensions = {};
		filter.searchStr = {};
		SplitList(req.extensions, filter.fileExtensions);
		SplitList(req.filters, filter.searchStr);
		// Accept ".et" as well as "et".
		for (int i = 0; i < filter.fileExtensions.Count(); i++)
		{
			string ext = filter.fileExtensions[i];
			if (ext.StartsWith("."))
				filter.fileExtensions[i] = ext.Substring(1, ext.Length() - 1);
		}

		s_aBrowseHits = new array<ref EMCP_WB_ResourceEntry>();
		s_iBrowseLimit = limit;
		s_iBrowseSeen = 0;

		ResourceDatabase.SearchResources(filter, OnBrowseResult);

		foreach (EMCP_WB_ResourceEntry hit : s_aBrowseHits)
			resp.m_aEntries.Insert(hit);
		resp.count = resp.m_aEntries.Count();
		resp.entryCount = resp.count;
		resp.truncated = s_iBrowseSeen > resp.count;

		// Clear straight away so the next call starts empty.
		s_aBrowseHits = null;
		s_iBrowseLimit = 0;
		s_iBrowseSeen = 0;

		resp.status = "ok";
		resp.message = "";
		if (resp.count == 0)
			resp.message = "No resources matched (an unknown rootPath also returns none).";
	}

	//------------------------------------------------------------------------------------------------
	protected void DoBatch(ResourceManager resMgr, EMCP_WB_ResourcesRequest req, EMCP_WB_ResourcesResponse resp, bool isRegister)
	{
		string configuration = req.configuration;
		if (configuration == "")
			configuration = "PC";
		resp.configuration = configuration;

		array<string> inputs = {};
		CollectPaths(req, inputs);
		if (inputs.IsEmpty())
		{
			resp.status = "error";
			resp.message = "paths (or path) parameter required";
			return;
		}

		array<string> absPaths = {};
		foreach (string input : inputs)
		{
			string absPath;
			string reason;
			if (!ResolveAbsolute(input, absPath, reason))
			{
				EMCP_WB_ResourceOutcome bad = new EMCP_WB_ResourceOutcome();
				bad.m_sPath = input;
				bad.m_sReason = reason;
				resp.m_aFailed.Insert(bad);
				continue;
			}
			absPaths.Insert(absPath);
		}

		array<string> doneAbs = {};
		if (isRegister)
		{
			foreach (string absFile : absPaths)
			{
				EMCP_WB_ResourceOutcome o = new EMCP_WB_ResourceOutcome();
				o.m_sPath = absFile;
				if (resMgr.RegisterResourceFile(absFile, req.buildRuntime))
				{
					resp.m_aProcessed.Insert(o);
					doneAbs.Insert(absFile);
				}
				else
				{
					o.m_sReason = "RegisterResourceFile returned false (already registered, or not a registrable file)";
					resp.m_aFailed.Insert(o);
				}
			}
		}
		else if (!absPaths.IsEmpty())
		{
			// One call for the whole batch.
			resMgr.RebuildResourceFiles(absPaths, configuration);
			foreach (string abs2 : absPaths)
			{
				EMCP_WB_ResourceOutcome o2 = new EMCP_WB_ResourceOutcome();
				o2.m_sPath = abs2;
				resp.m_aProcessed.Insert(o2);
				doneAbs.Insert(abs2);
			}
		}

		// Wait until the Resource Database acknowledges each file, so the next step cannot race it.
		if (req.waitMs > 0)
		{
			foreach (EMCP_WB_ResourceOutcome ok : resp.m_aProcessed)
			{
				if (resMgr.WaitForFile(ok.m_sPath, req.waitMs))
					ok.m_sReason = "acknowledged";
				else
					ok.m_sReason = "not acknowledged within " + req.waitMs + " ms";
			}
		}

		resp.processedCount = resp.m_aProcessed.Count();
		resp.failedCount = resp.m_aFailed.Count();
		resp.status = "ok";
		if (isRegister)
			resp.message = "Registered " + resp.processedCount + ", failed " + resp.failedCount;
		else
			resp.message = "Rebuild initiated for " + resp.processedCount + ", failed " + resp.failedCount;
	}

	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		EMCP_WB_ResourcesRequest req = EMCP_WB_ResourcesRequest.Cast(request);
		EMCP_WB_ResourcesResponse resp = new EMCP_WB_ResourcesResponse();
		resp.action = req.action;
		resp.path = req.path;

		if (req.action == "browse")
		{
			// Live ResourceDatabase search: works with no path (empty rootPath = every addon).
			DoBrowse(req, resp);
			return resp;
		}

		if (req.path == "" && req.paths == "")
		{
			resp.status = "error";
			resp.message = "path parameter required";
			return resp;
		}

		ResourceManager resMgr = Workbench.GetModule(ResourceManager);
		if (!resMgr)
		{
			resp.status = "error";
			resp.message = "ResourceManager module not available";
			return resp;
		}

		if (req.action == "register")
		{
			DoBatch(resMgr, req, resp, true);
		}
		else if (req.action == "rebuild")
		{
			DoBatch(resMgr, req, resp, false);
		}
		else if (req.action == "open")
		{
			bool result = resMgr.SetOpenedResource(req.path);
			resp.status = "ok";
			if (result)
				resp.message = "Opened resource: " + req.path;
			else
				resp.message = "SetOpenedResource returned false for: " + req.path;
		}
		else
		{
			resp.status = "error";
			resp.message = "Unknown action: " + req.action + ". Valid: register, rebuild, open, browse";
		}

		return resp;
	}
}
