// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SystemsPlaces`: with CheckPlaces
// returning 0 before its loop, `- the check switches off the one system with no place (expected 1, got 0)`,
// `- a system with none of its places is off` and `- the reason names the missing kind (expected 'no
// LF_Test_PlaceNone placed on this map', got '')` failed. The early return is gone; green.
//------------------------------------------------------------------------------------------------
//! systems/places (#1047): a system whose catalog entry lists places and finds none of them on the
//! map switches itself off at game start with one reason, and stays off; one placed kind of several
//! keeps it on; a system with no places listed is never touched; a part of a system that went off
//! is off with it. Uses a catalog built in script and a test place kind, so it runs on any map.
// tier: LOGIC
class LF_Test_SystemsPlaces : LF_Test
{
	static const string KIND_HERE = "LF_Test_PlaceHere";
	static const string KIND_NONE = "LF_Test_PlaceNone";

	override string GetName()
	{
		return "systems/places";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetGameMode();
		ctx.True(anchor != null, "a game mode entity stands in for a placed component");
		if (!anchor)
			return;

		ctx.True(LF_Places.Register(KIND_HERE, "here", anchor), "the test place registers");

		LF_SystemCatalog catalog = new LF_SystemCatalog();
		LF_SystemDef placed = catalog.Add("TPlaced", LF_ESystemSwitch.SWITCHABLE);
		placed.m_aPlaces = {KIND_NONE, KIND_HERE};
		LF_SystemDef unplaced = catalog.Add("TUnplaced", LF_ESystemSwitch.SWITCHABLE);
		unplaced.m_aPlaces = {KIND_NONE};
		catalog.Add("TUnplacedPart", LF_ESystemSwitch.SWITCHABLE, "TUnplaced");
		catalog.Add("TPlain", LF_ESystemSwitch.SWITCHABLE);
		LF_Systems.Install(catalog, null);

		ctx.True(LF_Systems.IsEnabled("TUnplaced"), "before the check every system is on");
		ctx.Equal(1, LF_Systems.CheckPlaces(), "the check switches off the one system with no place");
		ctx.True(LF_Systems.IsEnabled("TPlaced"), "one placed kind of two keeps a system on");
		ctx.False(LF_Systems.IsEnabled("TUnplaced"), "a system with none of its places is off");
		ctx.EqualStr("no LF_Test_PlaceNone placed on this map", LF_Systems.WhyOff("TUnplaced"), "the reason names the missing kind");
		ctx.False(LF_Systems.IsEnabled("TUnplacedPart"), "a part of it is off with it");
		ctx.True(LF_Systems.IsEnabled("TPlain"), "a system that lists no places is untouched");
		ctx.True(LF_Systems.Refuses("TUnplaced", "a test request"), "its server requests are refused");

		ctx.Equal(0, LF_Systems.CheckPlaces(), "a second check switches nothing again (one log line)");
		ctx.False(LF_Systems.IsEnabled("TUnplaced"), "and it stays off");

		// The shipped catalog again, without this test's run-time off; boot offs stay as they were.
		LF_Systems.ClearRuntimeOff("TUnplaced");
		LF_Systems.Reload();
	}
}
