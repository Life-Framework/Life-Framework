// red-proof: with the README still opened only through $profile: and a cwd-relative
// path, `tools\cli test --no-build --tier fast` printed
// `[ELTEST]   - crafting README can be read` (tier=fast passed=74 failed=1).
// tier: LOGIC
class EL_Test_CraftingAssets : EL_Test
{
	override string GetName()
	{
		return "crafting/assets";
	}

	protected bool ReadTextFile(string path, out string text)
	{
		text = "";
		if (!FileIO.FileExists(path))
			return false;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
			return false;

		// ReadLine returns 0 on a blank line and a negative value at EOF.
		// Stopping on (> 0) drops every line after the first blank line.
		string line;
		for (int i = 0; i < 400; i++)
		{
			int got = handle.ReadLine(line);
			if (got < 0)
				break;
			text = text + line + "\n";
		}

		handle.Close();
		return true;
	}

	protected string RepoRootFromProfile(string profile)
	{
		string back = "server\\profile\\test";
		int at = profile.IndexOf(back);
		if (at <= 0)
		{
			string fwd = "server/profile/test";
			at = profile.IndexOf(fwd);
		}
		if (at <= 0)
			return "";

		return profile.Substring(0, at);
	}

	protected bool LoadReadme(out string text, out string rootPrefix)
	{
		text = "";
		rootPrefix = "";

		array<string> candidates = {
			"$profile:crafting-addon/Prefabs/Crafting/README.md"
		};
		string profile;
		if (System.GetCLIParam("profile", profile))
		{
			string root = RepoRootFromProfile(profile);
			if (root.Length() > 0)
			{
				candidates.Insert(root + "addons\\LifeFramework\\Prefabs\\Crafting\\README.md");
				candidates.Insert(root + "addons/LifeFramework/Prefabs/Crafting/README.md");
			}
		}

		candidates.Insert("$profile:../../../addons/LifeFramework/Prefabs/Crafting/README.md");
		candidates.Insert("addons/LifeFramework/Prefabs/Crafting/README.md");

		foreach (string path : candidates)
		{
			if (!ReadTextFile(path, text))
				continue;

			int marker = path.IndexOf("Prefabs/Crafting/README.md");
			if (marker < 0)
				marker = path.IndexOf("Prefabs\\Crafting\\README.md");
			if (marker < 0)
				continue;

			rootPrefix = path.Substring(0, marker);
			return true;
		}

		return false;
	}

	protected string JoinAddon(string rootPrefix, string prefab)
	{
		if (rootPrefix.IndexOf("\\") < 0)
			return rootPrefix + prefab;

		string back = "";
		for (int i = 0; i < prefab.Length(); i++)
		{
			string ch = prefab.Substring(i, 1);
			if (ch == "/")
				back = back + "\\";
			else
				back = back + ch;
		}
		return rootPrefix + back;
	}

	protected void CollectPrefabPaths(string readme, array<string> paths)
	{
		array<string> lines = {};
		readme.Split("\n", lines, true);
		foreach (string raw : lines)
		{
			string line = raw;
			line.TrimInPlace();
			int start = line.IndexOf("Prefabs/Crafting/");
			if (start < 0)
				continue;

			string path = line.Substring(start, line.Length() - start);
			int et = path.IndexOf(".et");
			if (et < 0)
				continue;

			paths.Insert(path.Substring(0, et + 3));
		}
	}

	override void Run(EL_TestContext ctx)
	{
		string readme;
		string rootPrefix;
		bool loaded = LoadReadme(readme, rootPrefix);
		ctx.True(loaded && readme.Length() > 0, "crafting README can be read");
		if (!loaded)
			return;

		array<string> names = {
			"lathe",
			"sewing machine",
			"boiler",
			"fireplace",
			"vice",
			"charcoal",
			"sulfur",
			"gunpowder",
			"linen"
		};
		foreach (string name : names)
			ctx.True(readme.IndexOf(name) >= 0, "README names " + name);

		array<string> paths = {};
		CollectPrefabPaths(readme, paths);
		ctx.True(paths.Count() > 0, "README lists prefab paths");
		foreach (string prefab : paths)
			ctx.True(FileIO.FileExists(JoinAddon(rootPrefix, prefab)), "prefab path exists: " + prefab);
	}
}
