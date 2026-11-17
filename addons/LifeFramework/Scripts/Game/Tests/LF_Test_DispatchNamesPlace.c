// tier: WORLD
// red-proof: 2026-11-17, main guard red at b622741c (#2249 grew the grid): `--only LF_Test_DispatchNamesPlace` failed "no sign near: the report names the location (expected '#AR-MapLocation_Arleville', got 'free')" with the fixtures at z 120; at z 320 green. Earlier, 2026-11-16, on the unfixed DescribePlace `--only LF_Test_DispatchNamesPlace` failed "no sign near: the report names the location (expected '#AR-MapLocation_Arleville', got '#LF-Wanted_PlaceGrid 003-001')". With LF_NamedPlaces wired in, green.
//! police/dispatch-names-place: a report's place (#1944) is the nearest place sign's title, else the
//! nearest map location's name (LF_NamedPlaces), else the 100 m grid square. A report beside a
//! test-placed location with no sign names the location; once a sign stands beside it the sign wins;
//! far from both it falls back to the grid square. The fixtures are the vanilla Arleville label and
//! the test organisation sign, placed north of the annex: its grid cell label signs ("free" and the cell ids,
//! x 22..322, z 22..106) are place signs too and win inside the place radius (80 m), so the spot must stay
//! 200 m clear of the lattice (the grid grew east to x 328 in #2249 and put a label 45 m from the old spot).
class LF_Test_DispatchNamesPlace : LF_Test
{
	protected static const ResourceName LOCATION = "{A99A1ADAF98F2D48}Prefabs/World/Locations/Arland/Arleville.et";
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

		IEntity sign = LF_Utils.SpawnEntityPrefab(SIGN, beside);
		ctx.NotNull(sign, "the place sign spawned");
		if (sign)
			ctx.EqualStr("#LF-Sign_TestOrg_Title", LF_WantedReports.DescribePlace(beside), "a sign beside the crime still names the sign");

		// Far from every sign and every label: the grid square, as before.
		vector far = Vector(9000, groundY, 9000);
		ctx.True(LF_WantedReports.DescribePlace(far).StartsWith(LF_WantedReports.PLACE_GRID_KEY), "far from both the report falls back to the grid square");

		if (sign)
			SCR_EntityHelper.DeleteEntityAndChildren(sign);

		SCR_EntityHelper.DeleteEntityAndChildren(location);
	}
}
