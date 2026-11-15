//------------------------------------------------------------------------------------------------
//! "LF Fast Play" in the World Editor's Plugins menu and toolbar (Ctrl+Shift+F5): play the open world
//! as a dev character, already registered and in town, with no rules quiz and no form R-89.
//! The plugin only writes a request to $profile:lf-fastplay/request.txt and enters play mode; the
//! game reads it once (LF_WorkbenchPlay.IsFastPlay) and marks it used, so the ordinary Play button
//! keeps the full onboarding. This folder is the only committed one under Scripts/WorkbenchGame
//! (the rest is injected tooling, .gitignore); Workbench is the only thing that compiles it.
//! docs/tooling/fast-play.md
[WorkbenchPluginAttribute(name: "LF Fast Play", description: "Play this world as a registered dev character: no rules quiz, no registration form", shortcut: "Ctrl+Shift+F5", wbModules: { "WorldEditor" }, category: "Life Framework", awesomeFontCode: 0xF04B)]
class LF_FastPlayPlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-fastplay";
	protected static const string REQUEST_FILE = "$profile:lf-fastplay/request.txt";

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor || !worldEditor.GetApi())
		{
			Print("[LF Fast Play] open a world in the World Editor first", LogLevel.WARNING);
			return;
		}

		if (worldEditor.GetApi().IsGameMode())
		{
			Print("[LF Fast Play] already playing; stop play first", LogLevel.WARNING);
			return;
		}

		FileIO.MakeDirectory(DIR);
		FileHandle file = FileIO.OpenFile(REQUEST_FILE, FileMode.WRITE);
		if (!file)
		{
			Print("[LF Fast Play] could not write " + REQUEST_FILE, LogLevel.ERROR);
			return;
		}

		file.WriteLine("fast");
		file.Close();
		worldEditor.SwitchToGameMode();
	}
}
