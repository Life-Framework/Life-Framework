// Headless texture import for tools/cli ui-image. Launched as
//   Workbench -wbModule=ResourceManager -plugin=LF_ImageImportPlugin -lfImage=<abs .tif>
// The CLI has already written <name>.edds.meta; this rebuilds the runtime
// .edds from the source .tif. (RegisterResourceFile on a new unsuffixed texture
// opens a modal dialog and hangs a headless Workbench.)
// Workbench-only script: it is not part of the game build.
[WorkbenchPluginAttribute(name: "LF Image Import", description: "Rebuild a registered texture (command line: -lfImage=<abs path>)", wbModules: { "ResourceManager" })]
class LF_ImageImportPlugin : WorkbenchPlugin
{
	override void RunCommandline()
	{
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		if (!rm)
		{
			Print("LF_IMAGE_IMPORT FAIL no ResourceManager module", LogLevel.ERROR);
			return;
		}

		string path;
		if (!rm.GetCmdLine("-lfImage", path) || path == "")
		{
			Print("LF_IMAGE_IMPORT FAIL no -lfImage=<abs path> given", LogLevel.ERROR);
			return;
		}

		array<string> platforms = { "PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS" };
		foreach (string p : platforms)
		{
			Resource r = Resource.Load("Configs/System/ResourceTypes/" + p + "/TextureUI.conf");
			if (r && r.IsValid())
				Print("LF_IMAGE_IMPORT conf " + p + " " + r.GetResource().GetResourceName());
		}

		Print("LF_IMAGE_IMPORT rebuilding " + path);
		rm.RebuildResourceFile(path, "PC", false);
		Print("LF_IMAGE_IMPORT done " + path);
	}
}
