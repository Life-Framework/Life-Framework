// red-proof: 2026-11-18, cli test --no-build --only LF_Test_EveronLiveState2a on the unfixed header (no m_aLF_LiveStates row): FAIL everon/live-state-2a at "the header lists its live states (an empty list would make every state live)"; green with the row `m_aLF_LiveStates { 0 }`.
//! everon/live-state-2a (#2694, W5 of #2644): the pack's header names its live map states, and state 2a
//! (Saint-Philippe dock, the districts, the road to Meaux and Meaux; m6-outline.md Q1) is not among them:
//! a place tagged 2a does not register in LF_Places while the header leaves 2a out, and registers once the
//! header lists it. The cutover (#2212) turns 2a live by adding its id to the header's list.
//! A header with no list makes every state live (LF_PlaceLiveState), so the row IS the declaration.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronLiveState2a : LF_Test
{
	//! The pack's id for state 2a: state 2 split into sub-states 2a to 2d, ids 21 to 24 (the digit is the letter's place).
	static const int STATE_2A = 21;
	//! The square, state 0: live today.
	static const int STATE_SQUARE = 0;

	protected static const string KIND = "EveronLiveState2aTest";
	protected static const string ID = "live-state-2a";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/live-state-2a";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (!header)
			return;

		array<int> live = header.m_aLF_LiveStates;
		ctx.True(live && !live.IsEmpty(), "the header lists its live states (an empty list would make every state live)");
		if (!live || live.IsEmpty())
			return;

		ctx.True(live.Contains(STATE_SQUARE), "state 0, the square, is live");
		ctx.False(live.Contains(STATE_2A), "state 2a is declared dark: its id is not in the header's live list");
		ctx.True(LF_PlaceLiveState.IsLive(STATE_SQUARE, live), "a state-0 place is live under the pack header");
		ctx.False(LF_PlaceLiveState.IsLive(STATE_2A, live), "state 2a is dark under the pack's live list");
		ctx.True(LF_PlaceLiveState.IsLive(LF_PlaceLiveState.ALWAYS_LIVE, live), "an always-live place stays live under the pack header");

		array<int> opened = {};
		opened.Copy(live);
		opened.Insert(STATE_2A);
		ctx.True(LF_PlaceLiveState.IsLive(STATE_2A, opened), "listing the 2a id on the header is what opens it");

		vector at = "0 0 0";
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		if (anchor)
			at = anchor.GetOrigin() + "0 -30 0";

		IEntity place = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
		ctx.NotNull(place, "the fixture place spawns");
		if (!place)
			return;

		// The same call LF_PlaceComponent makes with its map state and the running header's list.
		ctx.False(LF_PlaceLiveState.Register(KIND, ID, STATE_2A, LF_PlaceLiveState.HeaderStates(), place), "a place tagged 2a does not register while 2a is dark");
		ctx.True(LF_Places.Find(KIND, ID) == null, "LF_Places finds nothing for the 2a place while 2a is dark");

		ctx.True(LF_PlaceLiveState.Register(KIND, ID, STATE_2A, opened, place), "the 2a place registers once 2a is listed");
		ctx.True(LF_Places.Find(KIND, ID) == place, "LF_Places finds the 2a place once 2a is live");

		SCR_EntityHelper.DeleteEntityAndChildren(place);
		ctx.True(LF_Places.Find(KIND, ID) == null, "a deleted place is not found");
	}
}
