/**
 * EMCP_WB_Eval.c - Compile and run a snippet of Enforce Script in Workbench
 *
 * Wraps the caller's code in a string-returning function, compiles it as a
 * child ScriptModule of the WorkbenchGame module, calls it and returns the
 * string.
 *
 * Memory: the engine never frees a compiled module. Measured 2026-11-14: each
 * CompileScript keeps about 11 MB of private memory whether the module is
 * deleted, set to null or kept (550 MB per 50 evals; 50 wb_state calls added
 * nothing). So identical code reuses its cached module, and fresh compiles
 * stop at MAX_COMPILES per script session (about 1.1 GB) until Workbench
 * restarts.
 *
 * A VME inside the snippet is not caught: Call never returns and Workbench
 * wedges until restart (measured 2026-11-14). Compile errors are safe.
 *
 * Inside the snippet these locals are ready (any may be null):
 *   WorldEditor we; WorldEditorAPI api; ResourceManager rm;
 *
 * Local tooling patch (tools/mcp-patches/); never part of addons/.
 * Called via NET API TCP protocol: APIFunc = "EMCP_WB_Eval"
 */

class EMCP_WB_EvalRequest : JsonApiStruct
{
	string code;

	void EMCP_WB_EvalRequest()
	{
		RegV("code");
	}
}

class EMCP_WB_EvalResponse : JsonApiStruct
{
	string status;
	string message;
	string result;
	string errorText;
	int errorLine;
	bool cached;
	int compiles;

	void EMCP_WB_EvalResponse()
	{
		RegV("status");
		RegV("message");
		RegV("result");
		RegV("errorText");
		RegV("errorLine");
		RegV("cached");
		RegV("compiles");
	}
}

class EMCP_WB_Eval : NetApiHandler
{
	// Matches the zod limit in src/tools/wb-eval.ts.
	static const int MAX_CODE_LENGTH = 32768;
	static const string ENTRY = "EMCP_WB_EvalEntry";
	// Lines in the prelude below before the caller's first line.
	static const int PRELUDE_LINES = 6;
	static const int MAX_COMPILES = 100;
	static const int MAX_CACHED = 32;

	// Compiled snippets by exact code; reused so a repeated audit costs nothing.
	static ref map<string, ref ScriptModule> s_mCache = new map<string, ref ScriptModule>();
	static int s_iCompiles;

	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetRequest()
	{
		return new EMCP_WB_EvalRequest();
	}

	//------------------------------------------------------------------------------------------------
	static string Wrap(string code)
	{
		string src = "string " + ENTRY + "()\n";
		src += "{\n";
		src += "WorldEditor we = Workbench.GetModule(WorldEditor);\n";
		src += "WorldEditorAPI api; if (we) api = we.GetApi();\n";
		src += "ResourceManager rm = Workbench.GetModule(ResourceManager);\n";
		src += "if (we || api || rm) {}\n";
		src += code + "\n";
		src += "return \"\";\n";
		src += "}\n";
		return src;
	}

	//------------------------------------------------------------------------------------------------
	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		EMCP_WB_EvalRequest req = EMCP_WB_EvalRequest.Cast(request);
		EMCP_WB_EvalResponse resp = new EMCP_WB_EvalResponse();

		if (!req || req.code.IsEmpty())
		{
			resp.status = "error";
			resp.message = "code is required";
			return resp;
		}
		if (req.code.Length() > MAX_CODE_LENGTH)
		{
			resp.status = "error";
			resp.message = "code is " + req.code.Length().ToString() + " chars; the limit is " + MAX_CODE_LENGTH.ToString();
			return resp;
		}

		ScriptModule parent = Workbench.GetWBGameScriptModule();
		if (!parent)
			parent = Workbench.GetWBScriptModule();
		if (!parent)
		{
			resp.status = "error";
			resp.message = "no Workbench script module to compile against";
			return resp;
		}

		ScriptModule mod = s_mCache.Get(req.code);
		resp.cached = mod != null;
		if (!mod)
		{
			if (s_iCompiles >= MAX_COMPILES)
			{
				resp.status = "error";
				resp.compiles = s_iCompiles;
				resp.message = "wb_eval compiled " + s_iCompiles.ToString() + " snippets in this Workbench session; each keeps about 11 MB the engine never frees. Restart Workbench to reset, or rerun an identical snippet (cached)";
				return resp;
			}
			string errorText;
			int errorLine;
			s_iCompiles++;
			mod = ScriptModule.CompileScript(parent, Wrap(req.code), errorText, errorLine);
			if (!mod)
			{
				resp.status = "compile_error";
				resp.compiles = s_iCompiles;
				resp.errorText = errorText;
				// Report the caller's line; 0 means the error is in the wrapper.
				resp.errorLine = errorLine - PRELUDE_LINES;
				if (resp.errorLine < 0)
					resp.errorLine = 0;
				resp.message = "compile failed at snippet line " + resp.errorLine.ToString() + ": " + errorText;
				return resp;
			}
			if (s_mCache.Count() < MAX_CACHED)
				s_mCache.Insert(req.code, mod);
		}
		resp.compiles = s_iCompiles;

		string returned;
		bool ok = mod.Call(null, ENTRY, false, returned);
		if (!ok)
		{
			resp.status = "runtime_error";
			resp.message = "the snippet failed at run time; read the Workbench log (SCRIPT (E) / VME lines) for the reason";
			return resp;
		}

		resp.status = "ok";
		resp.result = returned;
		return resp;
	}
}
