// red-proof: 2026-11-14, with the extra-addons check flipped to expect one (extra.Count() == 0), a
//   core-only run, cli test --no-build --core-only --only LF_Test_CoreOnlyBoot --local:
//   [LFTEST]   - core-only run loads only the core and the base game (loaded: ...)
// Restored; green.
// tier: LOGIC
//! core/only-boot: Test 1 of the red line (#326). On a core-only run (tools\cli test --core-only) the
//! engine has loaded the core and the base game addons and nothing else, so the suite proves what the
//! core does alone. A normal run has the same core loaded and may have more; this test then only
//! checks the core is there. The pack is never named: anything outside the known base set counts.
class LF_Test_CoreOnlyBoot : LF_Test
{
	protected static const string CORE_GUID = "79636E668EA37AC9";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "core/only-boot";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);

		string list = "";
		bool coreLoaded = false;
		array<string> extra = {};
		foreach (string guid : loaded)
		{
			list += guid + " ";
			if (guid == CORE_GUID)
				coreLoaded = true;
			else if (guid != "5614BBCCBB55ED1C" && guid != "58D0FB3206B6F859")
				extra.Insert(guid);
		}

		ctx.True(coreLoaded, "the core addon is loaded (loaded: " + list + ")");
		if (LF_TestManager.CoreOnlyRun())
			ctx.True(extra.Count() == 0, "core-only run loads only the core and the base game (loaded: " + list + ")");
	}
}
