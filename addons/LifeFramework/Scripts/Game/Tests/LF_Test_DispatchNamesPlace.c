// red-proof: (#5147) `--red LF_Test_DispatchNamesPlace` reads the site spot through LF_NamedPlaces.Nearest (the old DescribePlace), which names the sawmill label and fails "a site label beside the crime does not name it". Earlier: 2026-11-18 (#3092), with the sign scan back ahead of LF_NamedPlaces in DescribePlace `--only LF_Test_DispatchNamesPlace` failed "a sign beside the crime does not hide the town (expected '#AR-MapLocation_Arleville', got '#LF-Sign_TestOrg_Title')". Earlier, 2026-11-17, main guard red at b622741c (#2249 grew the grid): "no sign near: the report names the location (expected '#AR-MapLocation_Arleville', got 'free')" with the fixtures at z 120; at z 320 green.
//! police/dispatch-names-place: a report's place (#1944) is the nearest map location's name
//! (LF_NamedPlaces), else the nearest place sign's title, else the 100 m grid square. A report beside
//! a test-placed location names the location, and a sign beside the crime does not hide it (#3092:
//! on the island the bank's fascia sign stood nearer than the town label and the officer's call read
//! "Bank"). With no location within the radius the sign names the place, and far from both it falls
//! back to the grid square. The fixtures are the vanilla Arleville label and the test organisation
//! sign, placed north of the annex: its grid cell label signs ("free" and the cell ids, x 22..322,
//! z 22..106) are place signs too and win inside the place radius (80 m) when no label is near, so
//! the spots must stay 200 m clear of the lattice (the grid grew east to x 328 in #2249 and put a
//! label 45 m from an older spot). The far sign spot sits 800 m from the label, inside the 1500 m
//! settlement radius, so it reads "<sign>, <town>" too (#5147).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DispatchNamesPlace : LF_Test
{
	protected static const ResourceName LOCATION = "{A99A1ADAF98F2D48}Prefabs/World/Locations/Arland/Arleville.et";
	protected static const ResourceName SITE_LOCATION = "{213BE463B7949C47}Prefabs/World/Locations/Common/Urban/LocationUrbanGeneric_Sawmill.et";
	protected static const ResourceName SIGN = "{256EE1573D428340}Prefabs/Props/Signs/LF_Sign_TestOrg.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/dispatch-names-place";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(340, 320);
		IEntity location = LF_Utils.SpawnEntityPrefab(LOCATION, Vector(340, groundY, 320));
		ctx.NotNull(location, "the location label spawned");
		if (!location)
			return;

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(location.FindComponent(SCR_EditableEntityComponent));
		string locationName;
		if (editable)
			locationName = editable.GetDisplayName();
		ctx.False(locationName.IsEmpty(), "the label has a name");

		vector beside = Vector(345, groundY, 325);
		ctx.EqualStr(locationName, LF_WantedReports.DescribePlace(beside), "no sign near: the report names the location");

		// #5147: a site label (a sawmill, "Factory" on the island) nearer than the town never names the place; the town does.
		float siteY = GetGame().GetWorld().GetSurfaceY(340, 360);
		IEntity siteLabel = LF_Utils.SpawnEntityPrefab(SITE_LOCATION, Vector(340, siteY, 360));
		ctx.NotNull(siteLabel, "the site label spawned");
		if (siteLabel)
		{
			vector besideSite = Vector(342, siteY, 362);
			string siteRead = LF_WantedReports.DescribePlace(besideSite);
			if (RedProof())
				siteRead = LF_NamedPlaces.Nearest(besideSite);

			ctx.EqualStr(locationName, siteRead, "a site label beside the crime does not name it");
			SCR_EntityHelper.DeleteEntityAndChildren(siteLabel);
		}

		IEntity sign = LF_Utils.SpawnEntityPrefab(SIGN, beside);
		ctx.NotNull(sign, "the place sign spawned");
		if (sign)
		{
			// The site's own name and its town, joined; the board reads "Site, Town".
			string placed = LF_WantedReports.DescribePlace(beside);
			ctx.EqualStr("#LF-Sign_TestOrg_Title" + LF_WantedReports.PLACE_JOIN + locationName, placed, "a sign beside the crime names the site and keeps the town");
			string read = LF_WantedBoard.ResolvePlace(placed);
			ctx.True(read.Contains(", "), "the board reads the site and the town as one line: " + read);
			ctx.False(read.StartsWith("#"), "the joined place is localized on both sides: " + read);
		}

		// A sign far from the label (800 m, inside the 1500 m settlement radius) still names the place with its town.
		float farY = GetGame().GetWorld().GetSurfaceY(340, 1120);
		vector signOnly = Vector(345, farY, 1125);
		IEntity farSign = LF_Utils.SpawnEntityPrefab(SIGN, signOnly);
		ctx.NotNull(farSign, "the far place sign spawned");
		if (farSign)
			ctx.EqualStr("#LF-Sign_TestOrg_Title" + LF_WantedReports.PLACE_JOIN + locationName, LF_WantedReports.DescribePlace(signOnly), "a sign 800 m out is still in the town (settlement radius 1500 m): site and town");

		// Far from every sign and every label: the grid square, as before.
		vector far = Vector(9000, groundY, 9000);
		ctx.True(LF_WantedReports.DescribePlace(far).StartsWith(LF_WantedReports.PLACE_GRID_KEY), "far from both the report falls back to the grid square");

		if (farSign)
			SCR_EntityHelper.DeleteEntityAndChildren(farSign);

		if (sign)
			SCR_EntityHelper.DeleteEntityAndChildren(sign);

		SCR_EntityHelper.DeleteEntityAndChildren(location);
	}
}
