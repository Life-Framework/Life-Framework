// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapMarksOffscreen: RED PROOF OK (the old shown-list-only count: "expected 3, got 1"). Green with the disabled list.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! map/marks-offscreen (#2236, the map proof pass): the marker manager moves a static marker that is off
//! the shown map frame into its disabled list every map frame, so the player's personal marks are the
//! shown ones plus the disabled ones. The in-game proof counted 0 marks after placing 1 off screen, so
//! the 21st mark was never refused and an upload dropped every off-screen mark from the save.
//! Pure script: marker objects in two lists, no manager and no player needed.
class LF_Test_MapMarksOffscreen : LF_Test
{
	override string GetName()
	{
		return "map/marks-offscreen";
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_MapMarkerBase Make(SCR_EMapMarkerType type, int x)
	{
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(type);
		marker.SetWorldPos(x, x);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_MapMarkerBase shown = Make(SCR_EMapMarkerType.PLACED_CUSTOM, 10);
		SCR_MapMarkerBase offCustom = Make(SCR_EMapMarkerType.PLACED_CUSTOM, 20);
		SCR_MapMarkerBase offMilitary = Make(SCR_EMapMarkerType.PLACED_MILITARY, 30);
		SCR_MapMarkerBase offScripted = Make(SCR_EMapMarkerType.SIMPLE, 40);

		array<SCR_MapMarkerBase> visible = { shown };
		array<SCR_MapMarkerBase> disabled = { offCustom, offMilitary, offScripted };

		array<SCR_MapMarkerBase> marks = {};
		int added;
		if (RedProof())
			added = LF_MapMarksClient.CollectPersonal(visible, null, marks); // the old count: shown list only
		else
			added = LF_MapMarksClient.CollectPersonal(visible, disabled, marks);

		ctx.Equal(3, added, "a personal mark off screen still counts");
		ctx.Equal(3, marks.Count(), "the shown mark and both off-screen personal marks are collected");
		ctx.True(marks.Contains(offCustom), "an off-screen custom mark is collected (it would be uploaded)");
		ctx.True(marks.Contains(offMilitary), "an off-screen military mark is collected");
		ctx.False(marks.Contains(offScripted), "a scripted marker is not a personal mark");

		// A marker in both lists (mid-move between them) counts once.
		array<SCR_MapMarkerBase> twice = { shown };
		array<SCR_MapMarkerBase> again = {};
		ctx.Equal(1, LF_MapMarksClient.CollectPersonal(twice, twice, again), "a marker in both lists counts once");

		// Null lists (a manager with nothing disabled yet) collect nothing and do not crash.
		array<SCR_MapMarkerBase> none = {};
		ctx.Equal(0, LF_MapMarksClient.CollectPersonal(null, null, none), "no lists, no marks");
	}
}
