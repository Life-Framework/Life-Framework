// red-proof: 2026-11-14, with the Roadmap hideout door's m_bOrganizationHideout set to 0, cli test --no-build --only LF_Test_PlacesOrgHideout: [LFTEST]   - the organization hideout keeps its saved key (expected 'Roadmap_Hideout', got ''). Restored; green.
// tier: WORLD
//! places/org-hideout: the organization hideout and the film till are found by component, not by a
//! baked id or coordinate (#1055). The hideout the desk leases is the door marked as the
//! organization hideout, and its id is the key the state is saved under ("Roadmap_Hideout", unchanged
//! from before). A door placed in a test is not the organization hideout. The film street scene finds
//! its till by kind and id.
class LF_Test_PlacesOrgHideout : LF_Test
{
	protected static const string SAVED_KEY = "Roadmap_Hideout";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/org-hideout";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr(SAVED_KEY, LF_Hideout.OrgHideoutId(), "the organization hideout keeps its saved key");

		IEntity door = LF_Places.Find(LF_Hideout.PLACE_KIND_ORG_HIDEOUT, LF_Hideout.PLACE_ID_ORG_HIDEOUT);
		ctx.NotNull(door, "the organization hideout door is placed");
		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, SAVED_KEY) == door, "it is the door bound under the saved key");

		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_ORG_HIDEOUT, "other") == null, "no second organization hideout is placed");
		ctx.False(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, LF_TestAnnex.HIDEOUT_ID) == door, "the annex hideout is not the organization hideout");

		IEntity till = LF_Places.Find("FilmTill", "street");
		ctx.NotNull(till, "the film street till is placed");
		if (till)
			ctx.NotNull(LF_Component<BaseActionsManagerComponent>.Find(till), "the film till carries actions");
	}
}
