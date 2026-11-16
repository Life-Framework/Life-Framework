// red-proof: 2026-11-14, with the Roadmap hideout door's m_bOrganizationHideout set to 0, cli test --no-build --only LF_Test_PlacesOrgHideout: [LFTEST]   - the organization hideout keeps its saved key (expected 'Roadmap_Hideout', got ''). Restored; green.
// red-proof (#1892, 2026-11-16): `tools\cli test --no-build --only <the 7 slice-1892 tests> --world Worlds/GridWorld/GridWorld.ent` before Grid_orgs_vehicles.layer failed all 7 (no org hideout door, no flagged lot, no store, no impound pad); green with it.
// tier: WORLD
//! places/org-hideout: the organization hideout and the film till are found by component, not by a
//! baked id or coordinate (#1055). The hideout the desk leases is the door marked as the
//! organization hideout, and its id is the key the state is saved under (the door's own hideout id; the
//! town's is still "Roadmap_Hideout"). A door placed in a test is not the organization hideout. The
//! film street scene finds its till by kind and id. GridWorld places both in Grid_orgs_vehicles.layer.
class LF_Test_PlacesOrgHideout : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/org-hideout";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity door = LF_Places.Find(LF_Hideout.PLACE_KIND_ORG_HIDEOUT, LF_Hideout.PLACE_ID_ORG_HIDEOUT);
		ctx.NotNull(door, "the organization hideout door is placed");
		if (!door)
			return;

		LF_HideoutDoorComponent doorComponent = LF_Component<LF_HideoutDoorComponent>.Find(door);
		ctx.NotNull(doorComponent, "the organization hideout door carries LF_HideoutDoorComponent");
		if (!doorComponent)
			return;

		string savedKey = doorComponent.GetHideoutIdAttribute();
		ctx.False(savedKey.IsEmpty(), "the organization hideout door has a saved key");
		ctx.EqualStr(savedKey, LF_Hideout.OrgHideoutId(), "the organization hideout keeps its saved key");
		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, savedKey) == door, "it is the door bound under the saved key");

		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_ORG_HIDEOUT, "other") == null, "no second organization hideout is placed");
		ctx.False(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, LF_TestAnnex.HIDEOUT_ID) == door, "the annex hideout is not the organization hideout");

		IEntity till = LF_Places.Find("FilmTill", "street");
		ctx.NotNull(till, "the film street till is placed");
		if (till)
			ctx.NotNull(LF_Component<BaseActionsManagerComponent>.Find(till), "the film till carries actions");
	}
}
