// red-proof: 2026-11-16, cli test --no-build --red LF_Test_PlaceLiveState: RED-OK (RedProof()
//   registers the state-2 place under no list, so "does not register under [0]" fails). Green without it.
// tier: WORLD
//! places/live-state: a place of a map state the header does not list stays dark (#2208, D14).
//! Two fixture places, states 0 and 2, under a header that lists only state 0: LF_Places finds the
//! state-0 place and nothing for the state-2 one; once state 2 is listed both are found. A place
//! with the always-live state and a header with no list are live too.
class LF_Test_PlaceLiveState : LF_Test
{
	protected static const string KIND = "PlaceLiveStateTest";
	protected static const string ID_LIVE = "live-state-0";
	protected static const string ID_DARK = "live-state-2";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/live-state";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<int> onlyZero = {0};
		array<int> zeroAndTwo = {0, 2};
		array<int> none = {};

		ctx.True(LF_PlaceLiveState.IsLive(0, onlyZero), "state 0 is live under [0]");
		ctx.False(LF_PlaceLiveState.IsLive(2, onlyZero), "state 2 is dark under [0]");
		ctx.True(LF_PlaceLiveState.IsLive(2, zeroAndTwo), "state 2 is live under [0, 2]");
		ctx.True(LF_PlaceLiveState.IsLive(2, none), "an empty list makes every state live");
		ctx.True(LF_PlaceLiveState.IsLive(2, null), "no list makes every state live");
		ctx.True(LF_PlaceLiveState.IsLive(LF_PlaceLiveState.ALWAYS_LIVE, onlyZero), "the always-live state is live under any list");

		BaseWorld world = GetGame().GetWorld();
		IEntity anchor = world.FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex breach door anchors the fixtures");
		if (!anchor)
			return;

		vector at = anchor.GetOrigin() + "0 -30 0";
		IEntity live = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
		IEntity dark = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at + "3 0 0", "0 0 0", false);
		ctx.True(live && dark, "two fixture places spawn");
		if (!live || !dark)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(live);
			SCR_EntityHelper.DeleteEntityAndChildren(dark);
			return;
		}

		array<int> darkList = onlyZero;
		if (RedProof())
			darkList = null;

		ctx.True(LF_PlaceLiveState.Register(KIND, ID_LIVE, 0, onlyZero, live), "the state-0 place registers under [0]");
		ctx.False(LF_PlaceLiveState.Register(KIND, ID_DARK, 2, darkList, dark), "the state-2 place does not register under [0]");
		ctx.True(LF_Places.Find(KIND, ID_LIVE) == live, "LF_Places finds the state-0 place");
		ctx.True(LF_Places.Find(KIND, ID_DARK) == null, "LF_Places finds nothing for the state-2 place");
		array<IEntity> all = {};
		ctx.Equal(1, LF_Places.FindAll(KIND, all), "only the live place is listed");

		ctx.True(LF_PlaceLiveState.Register(KIND, ID_DARK, 2, zeroAndTwo, dark), "the state-2 place registers once state 2 is listed");
		ctx.True(LF_Places.Find(KIND, ID_DARK) == dark, "LF_Places finds the state-2 place once it is live");
		ctx.True(LF_Places.Find(KIND, ID_LIVE) == live, "the state-0 place is still found");

		SCR_EntityHelper.DeleteEntityAndChildren(live);
		SCR_EntityHelper.DeleteEntityAndChildren(dark);
		ctx.True(LF_Places.Find(KIND, ID_LIVE) == null, "a deleted place is not found");
	}
}
