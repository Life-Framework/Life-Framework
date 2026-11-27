// red-proof: 2026-11-27, with NearestSettlement delegating to the plain nearest label, `--only LF_Test_NamedPlaces` failed "the settlement name skips the site label". Earlier: 2026-11-16, with LF_NamedPlaces.Nearest returning empty, `--only LF_Test_NamedPlaces` failed "a position beside the label is named after it" and "a wider radius reaches the label". Restored, green.
//! world/named-place: LF_NamedPlaces.Nearest (#1943) names a position after the nearest map
//! location. A position next to a test-placed location returns that location's name; a position
//! beyond the radius returns empty. The fixture is the vanilla Arleville label placed in the annex.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NamedPlaces : LF_Test
{
	protected static const ResourceName LOCATION = "{A99A1ADAF98F2D48}Prefabs/World/Locations/Arland/Arleville.et";

	protected static const ResourceName SITE_LOCATION = "{213BE463B7949C47}Prefabs/World/Locations/Common/Urban/LocationUrbanGeneric_Sawmill.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "world/named-place";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(340, 120);
		IEntity location = LF_Utils.SpawnEntityPrefab(LOCATION, Vector(340, groundY, 120));
		ctx.NotNull(location, "the location label spawned");
		if (!location)
			return;

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(location.FindComponent(SCR_EditableEntityComponent));
		ctx.NotNull(editable, "the label is an editable entity");
		string expected;
		if (editable)
			expected = editable.GetDisplayName();
		ctx.False(expected.IsEmpty(), "the label has a name");

		vector beside = Vector(345, groundY, 125);
		ctx.EqualStr(expected, LF_NamedPlaces.Nearest(beside, 50), "a position beside the label is named after it");

		vector far = Vector(340, groundY, 120 + 200);
		ctx.EqualStr("", LF_NamedPlaces.Nearest(far, 50), "a position beyond the radius has no name");
		ctx.EqualStr(expected, LF_NamedPlaces.Nearest(far, 400), "a wider radius reaches the label");

		// A site label (a sawmill) nearer than the settlement names the position for Nearest but not for
		// NearestSettlement: a place beside a factory still sits in its town.
		float siteY = GetGame().GetWorld().GetSurfaceY(340, 160);
		IEntity site = LF_Utils.SpawnEntityPrefab(SITE_LOCATION, Vector(340, siteY, 160));
		ctx.NotNull(site, "the site label spawned");
		if (site)
		{
			vector besideSite = Vector(342, siteY, 162);
			ctx.False(LF_NamedPlaces.Nearest(besideSite, 50) == expected, "a site label is the nearest name beside it");
			ctx.EqualStr(expected, LF_NamedPlaces.NearestSettlement(besideSite, 400), "the settlement name skips the site label");
			SCR_EntityHelper.DeleteEntityAndChildren(site);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(location);
	}
}
