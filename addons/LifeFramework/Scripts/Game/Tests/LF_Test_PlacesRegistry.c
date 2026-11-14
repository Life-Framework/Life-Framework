// red-proof: 2026-11-14, with LF_Places.FindOrigin zeroing origin before the lookup,
//   cli test --no-build --only LF_Test_PlacesRegistry:
//   [LFTEST]   - an unplaced origin leaves the fallback alone
// Restored; green.
// tier: LOGIC
//! places/registry: the LF_Places guards (#961). A register without a kind, id or entity is refused;
//! a lookup of a place nobody placed is null and leaves the caller's fallback position alone.
class LF_Test_PlacesRegistry : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/registry";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_Places.Register("", "places-logic", null), "a place with no kind is refused");
		ctx.False(LF_Places.Register("PlacesLogic", "", null), "a place with no id is refused");
		ctx.False(LF_Places.Register("PlacesLogic", "places-logic", null), "a place with no entity is refused");

		ctx.True(LF_Places.Find("PlacesLogic", "places-logic") == null, "an unplaced kind finds nothing");
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, "places-logic-missing") == null, "an unplaced id of a known kind finds nothing");

		vector origin = "7 8 9";
		ctx.False(LF_Places.FindOrigin("PlacesLogic", "places-logic", origin), "an unplaced origin reports not found");
		ctx.True(vector.Distance(origin, "7 8 9") < 0.001, "an unplaced origin leaves the fallback alone");

		array<IEntity> all = {};
		ctx.Equal(0, LF_Places.FindAll("PlacesLogic", all), "an unplaced kind lists nothing");
	}
}
