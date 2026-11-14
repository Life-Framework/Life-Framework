// red-proof: 2026-11-14, with LF_DebugDispatch.PlacePosition back on FindEntityByName(placeId),
//   cli test --no-build --only LF_Test_PlacesDispatch:
//   [LFTEST]   - the call comes from the placed entity by component
// Restored; green.
// tier: WORLD
//! places/dispatch: a debug dispatch call comes from a placed DispatchSource id, never from an
//! entity name (#1045). An unnamed entity placed under an id is where the call comes from; an id
//! that is not placed, or none, falls back to the console. Under the old FindEntityByName lookup an
//! unnamed place was never found.
class LF_Test_PlacesDispatch : LF_Test
{
	protected static const string PLACE_ID = "places-dispatch-source";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/dispatch";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(anchor, "the annex vault is placed, to spawn the stand-in near");
		if (!anchor)
			return;

		vector console = "1 2 3";
		ctx.True(LF_DebugDispatch.PlacePosition("", console) == console, "no place id is the console");
		ctx.True(LF_DebugDispatch.PlacePosition(PLACE_ID, console) == console, "an id that is not placed is the console");

		vector at = anchor.GetOrigin() + "0 -20 0";
		IEntity place = LF_Utils.SpawnEntityPrefab(LF_Bank.BagPrefab(), at, "0 0 0", false);
		ctx.NotNull(place, "an unnamed place spawns");
		if (!place)
			return;

		ctx.EqualStr("", place.GetName(), "the place has no name");
		ctx.True(LF_Places.Register(LF_DebugDispatch.PLACE_KIND, PLACE_ID, place), "the unnamed place is placed under its id");

		vector from = LF_DebugDispatch.PlacePosition(PLACE_ID, console);
		ctx.True(vector.Distance(from, place.GetOrigin()) < 0.01, "the call comes from the placed entity by component");

		SCR_EntityHelper.DeleteEntityAndChildren(place);
		ctx.True(LF_DebugDispatch.PlacePosition(PLACE_ID, console) == console, "a deleted place is the console again");
	}
}
