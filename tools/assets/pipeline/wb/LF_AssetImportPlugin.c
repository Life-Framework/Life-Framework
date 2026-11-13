// Headless model import for tools\cli asset (stage "import"). Launched as
//   Workbench -wbModule=ResourceManager -plugin=LF_AssetImportPlugin -lfAsset0=<abs source> -lfAsset1=...
// Sources are the .tif textures first and the .fbx last. The CLI has already written every .meta,
// so each file is a registered resource and only needs a rebuild. (RegisterResourceFile on a new
// .fbx opens a modal dialog and hangs a headless Workbench; see docs/research/asset-pipeline.md.)
// Workbench-only script: tools/assets/pipeline/import.mjs copies it to Scripts/WorkbenchGame/,
// which is gitignored, so it is not part of the game build.
[WorkbenchPluginAttribute(name: "LF Asset Import", description: "Rebuild registered asset sources (command line: -lfAsset0=<abs path> ...)", wbModules: { "ResourceManager" })]
class LF_AssetImportPlugin : WorkbenchPlugin
{
	override void RunCommandline()
	{
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		if (!rm)
		{
			Print("LF_ASSET_IMPORT FAIL no ResourceManager module", LogLevel.ERROR);
			return;
		}

		// One RebuildResourceFiles call: a second RebuildResourceFile issued while the first is
		// still building is dropped without a log line.
		array<string> paths = {};
		for (int i = 0; i < 16; i++)
		{
			string path;
			if (!rm.GetCmdLine("-lfAsset" + i.ToString(), path) || path == "")
				break;

			Print("LF_ASSET_IMPORT rebuilding " + path);
			paths.Insert(path);
		}

		rm.RebuildResourceFiles(paths, "PC");
		Print("LF_ASSET_IMPORT done " + paths.Count().ToString());
	}
}
