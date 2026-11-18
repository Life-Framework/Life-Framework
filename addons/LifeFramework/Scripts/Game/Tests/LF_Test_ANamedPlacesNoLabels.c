// red-proof: 2026-11-18, `tools\cli test --no-build --only LF_Test_ANamedPlacesNoLabels` on the unfixed LF_NamedPlaces (core.FindNearestEntity): the server crashed with a VME `NULL pointer to instance. Variable '#array' during call 'set<SCR_EditableEntityComponent>.Count'` in SCR_EditableEntityCore.FindNearestEntity, `SUMMARY test fail ... (crash)`; with the null-safe scan it passes.

//! world/named-place-no-labels: LF_NamedPlaces.Nearest (#1943) in a world where no editable entity
//! ever registered answers empty instead of crashing. The base game's FindNearestEntity reads its
//! private entity set without a null check, and that set stays null until the first label registers.
//! The name sorts early on purpose: any test that registers a label first would hide the bug, and
//! LF_Test_ChainTrace (through LF_WantedReports.DescribePlace) hit it as soon as tests ran name-sorted.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ANamedPlacesNoLabels : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "world/named-place-no-labels";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_NamedPlaces.Nearest(Vector(0, 0, 0)), "a world with no map labels names nothing");
		ctx.EqualStr("", LF_NamedPlaces.Nearest(Vector(500, 0, 500), 10), "a small radius in an unlabelled world names nothing");
	}
}
