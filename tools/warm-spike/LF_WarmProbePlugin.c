// LF_WarmProbePlugin.c - warm-runner spike (2026-11-14). Not shipped.
//
// Copied into the run folder's git-ignored Scripts/WorkbenchGame/LFWarm/ and started with
//   -wbModule=WorldEditor -plugin=LF_WarmProbePlugin -profile <dir>
// It serves commands from $profile:lf-warm/go.txt (line 1 id, line 2 verb, line 3 argument) and answers
// in $profile:lf-warm/done.txt (line 1 id, then result lines). Verbs:
//   open <world>        SetOpenedResource
//   reload <a|b|c>      ScriptEditor.ExecuteAction(menu path split on |)
//   call <function>     GetGame().GetScriptModule().Call(null, function) -> string
//   play <ms>           SwitchToGameMode, wait <ms>, SwitchToEditMode
//   quit                Workbench.Exit(0)

[WorkbenchPluginAttribute(name: "LF Warm Probe", description: "warm-runner spike", wbModules: { "WorldEditor" }, category: "Life Framework")]
class LF_WarmProbePlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-warm";

	override void RunCommandline()
	{
		Write(DIR + "/alive.txt", "start " + System.GetTickCount());
		string lastId;
		int idle;
		while (idle < 1800000)
		{
			string id;
			string verb;
			string arg;
			FileHandle go = FileIO.OpenFile(DIR + "/go.txt", FileMode.READ);
			if (go)
			{
				go.ReadLine(id);
				go.ReadLine(verb);
				go.ReadLine(arg);
				go.Close();
				id.TrimInPlace();
				verb.TrimInPlace();
				arg.TrimInPlace();
			}

			if (id.IsEmpty() || id == lastId)
			{
				Sleep(100);
				idle += 100;
				continue;
			}

			lastId = id;
			idle = 0;
			int t0 = System.GetTickCount();
			string result = Do(verb, arg);
			string done = id + "\n" + result + "\nms=" + (System.GetTickCount() - t0);
			Print("[LFWARM] " + verb + " " + arg + " -> " + result + " ms=" + (System.GetTickCount() - t0));
			Write(DIR + "/done.txt", done);
			if (verb == "quit")
				break;
		}

		Workbench.Exit(0);
	}

	protected string Do(string verb, string arg)
	{
		WorldEditor we = Workbench.GetModule(WorldEditor);
		if (verb == "open")
		{
			if (!we)
				return "error no WorldEditor";

			return "open=" + we.SetOpenedResource(arg);
		}

		if (verb == "reload")
		{
			ScriptEditor se = Workbench.GetModule(ScriptEditor);
			if (!se)
			{
				bool opened = Workbench.OpenModule(ScriptEditor);
				se = Workbench.GetModule(ScriptEditor);
				if (!se)
					return "error no ScriptEditor opened=" + opened;
			}

			array<string> path = {};
			arg.Split("|", path, true);
			bool ok = se.ExecuteAction(path, true);
			return "execute=" + ok;
		}

		if (verb == "call")
		{
			ArmaReforgerScripted game = GetGame();
			if (!game)
				return "error no game";

			ScriptModule mod = game.GetScriptModule();
			if (!mod)
				return "error no module";

			string r = "<unset>";
			bool called = mod.Call(null, arg, false, r);
			return "called=" + called + " value=" + r;
		}

		if (verb == "play")
		{
			if (!we)
				return "error no WorldEditor";

			WorldEditorAPI api = we.GetApi();
			we.SwitchToGameMode();
			int waited;
			while (!api.IsGameMode() && waited < 60000)
			{
				if (waited > 0 && waited % 8000 == 0)
					we.SwitchToGameMode();

				Sleep(100);
				waited += 100;
			}

			if (!api.IsGameMode())
				return "error play_not_started";

			int startMs = waited;
			Sleep(arg.ToInt());
			we.SwitchToEditMode();
			int left;
			while (api.IsGameMode() && left < 60000)
			{
				Sleep(100);
				left += 100;
			}

			Sleep(1500);
			return "play_started_ms=" + startMs + " leave_ms=" + left;
		}

		if (verb == "quit")
			return "bye";

		return "error unknown verb " + verb;
	}

	protected void Write(string path, string text)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.WRITE);
		if (!f)
			return;

		f.Write(text);
		f.Close();
	}
}
