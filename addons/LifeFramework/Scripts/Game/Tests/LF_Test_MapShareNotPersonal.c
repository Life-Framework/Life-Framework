// red-proof: 2026-11-18, cli test --no-build --red (batched with the other review-map tests): RED-OK, "only the dialog mark counts toward the cap and the upload (expected 1, got 3)". Green with IsPersonal skipping read-only marks.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! map/share-not-personal (review 2026-11-17, docs/reviews/2026-11-17-map.md): a shared mark arrives as a
//! local PLACED_CUSTOM marker with id -1, the same shape as a personal mark. Counted as personal it took
//! a slot of the 20 cap (a player with 20 marks got no dispatch or org marks) and was uploaded and saved
//! as the player's own mark, so an org or dispatch mark stayed on the map after leaving the org or duty.
//! Pure script: marker objects shaped as LF_MapShareComponent and the marker dialog make them.
class LF_Test_MapShareNotPersonal : LF_Test
{
	override string GetName()
	{
		return "map/share-not-personal";
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_MapMarkerBase Make(int x, bool removable)
	{
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		marker.SetWorldPos(x, x);
		if (!RedProof())
			marker.SetCanBeRemovedByOwner(removable);

		return marker;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_MapMarkerBase personal = Make(10, true);
		SCR_MapMarkerBase shared = Make(20, false);
		SCR_MapMarkerBase sharedOff = Make(30, false);

		ctx.True(LF_MapMarksClient.IsPersonal(personal), "a dialog mark is personal");
		ctx.False(LF_MapMarksClient.IsPersonal(shared), "a read-only shared mark is not personal");

		array<SCR_MapMarkerBase> visible = { personal, shared };
		array<SCR_MapMarkerBase> disabled = { sharedOff };
		array<SCR_MapMarkerBase> marks = {};
		ctx.Equal(1, LF_MapMarksClient.CollectPersonal(visible, disabled, marks), "only the dialog mark counts toward the cap and the upload");
		ctx.False(marks.Contains(shared), "a shared mark on screen is never uploaded as a personal mark");
		ctx.False(marks.Contains(sharedOff), "a shared mark off screen is never uploaded as a personal mark");
	}
}
