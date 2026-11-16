// red-proof: observed 2026-11-16 (#1939) with `--only LF_Test_SpikeStripPreview` before the change: the script failed to compile (`Undefined function LF_SpikeStripPreview.AvailableTo/Evaluate`); implemented, green; then `--red LF_Test_SpikeStripPreview` printed `RED PROOF OK` (a confirm for the red spot, its refusal reason and the kept folded strip all failed with the overlapping strip left out).
// tier: WORLD
//! #1939 acceptance: the spike strip preview ships. An on-duty officer on the main street's south
//! verge, with the folded strip at their feet, is offered the ghost on any world (no dev-tools flag;
//! a civilian is not). The planned spot is valid and green, the plan lies straight across a road
//! (the axis for a player facing 40 degrees off it, on a made-up road direction: the real network
//! runs in LF_Test_PlacementRoad on CoreOnEveron). With a strip already lying in the planned spot the ghost is red
//! with the reason ("... is in the way"), and a confirm sent for that spot is refused by the
//! placement service: nothing is placed, the folded strip stays.
class LF_Test_SpikeStripPreview : LF_Test
{
	static const ResourceName ITEM = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";
	//! A strip already laid: the spot is overlapped (a car or a doorway's building also read red, but their roofs and steps first read as steep ground).
	static const ResourceName BLOCKER = "{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et";
	protected static const string OFFICER_UID = "strip-preview-officer";
	protected static const string CIVILIAN_UID = "strip-preview-civilian";
	//! Open verge south of Village_MainStreet (centre line z = 250), as in LF_Test_PlacementConfirm.
	protected static const vector VERGE = "300 0 245.5";

	override string GetName()
	{
		return "police/spike-strip-preview";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !civilian)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		LF_TestPoliceKit.Account(CIVILIAN_UID, false);
		LF_PoliceRanks.BindOfficer(civilian, CIVILIAN_UID);

		Stand(officer, VERGE, 40);
		Stand(civilian, VERGE + "12 0 0", 0);
		RunPreview(ctx, officer, civilian);

		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPreview(LF_TestContext ctx, IEntity officer, IEntity civilian)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!placeable)
		{
			ctx.Fail("the folded strip has no LF_PlaceableComponent");
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		// Shipped: shown to an on-duty officer whatever the game mode's dev-tools setting, not to a civilian.
		ctx.True(LF_SpikeStripPreview.Enabled(), "the preview is on without the dev-tools flag");
		ctx.True(LF_SpikeStripPreview.AvailableTo(officer, item), "an on-duty officer is offered the ghost");
		ctx.False(LF_SpikeStripPreview.AvailableTo(civilian, item), "a civilian is not");

		// The planned spot: valid, no reason.
		vector spot[4];
		string reason;
		LF_EPlacementVerdict verdict = LF_SpikeStripPreview.Evaluate(officer, placeable, item, spot, reason);
		ctx.Equal(LF_EPlacementVerdict.VALID, verdict, "the verge in front of the officer is green: " + reason);
		ctx.EqualStr("", reason, "a green ghost has no reason");

		// Across the road: for a player facing 40 degrees off a road running east-west, the plan's axis
		// is the road's cross direction (the plan's own axis rule, LF_PlacementGeometry.Axis).
		vector roadDir = "1 0 0";
		vector facing = Vector(Math.Sin(40 * Math.DEG2RAD), 0, Math.Cos(40 * Math.DEG2RAD));
		vector axis = LF_PlacementGeometry.Axis(facing, true, roadDir);
		ctx.InRange(LF_PlacementGeometry.AcrossAngle(axis, roadDir), 0, 0.5, "the plan lies straight across the road, not where the officer faces");

		// A strip already lying in the planned spot (overlapping): red, with the reason.
		IEntity blocker;
		if (!RedProof())
			blocker = LF_Utils.SpawnEntityPrefab(BLOCKER, spot[3], vector.Zero);

		ctx.NotNull(blocker, "the blocker spawned");
		vector blocked[4];
		string blockedReason;
		LF_EPlacementVerdict redVerdict = LF_SpikeStripPreview.Evaluate(officer, placeable, item, blocked, blockedReason);
		ctx.Equal(LF_EPlacementVerdict.BLOCKED, redVerdict, "a prop in the spot turns the ghost red");
		ctx.True(blockedReason.IndexOf("in the way") != -1, "red says why: " + blockedReason);

		// A confirm sent for that spot is refused by the placement service: nothing placed, the item stays.
		int before = LF_SpikeStrips.Count();
		EntityID itemId = item.GetID();
		IEntity placed = LF_PlacementService.Confirm(officer, item, blocked);
		ctx.True(placed == null, "a confirm for the red spot is refused");
		if (placed)
			LF_TestCharacterFixture.Delete(placed);

		string refusal = LF_PlacementService.LastRefusal();
		ctx.True(refusal.IndexOf("in the way") != -1, "the refusal names the reason: " + refusal);
		ctx.Equal(before, LF_SpikeStrips.Count(), "a refused confirm places nothing");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) != null, "a refused confirm keeps the folded strip");

		LF_TestCharacterFixture.Delete(blocker);
		LF_TestCharacterFixture.Delete(item);
	}

	//------------------------------------------------------------------------------------------------
	//! `yaw` in degrees from north (+z) toward east (+x).
	protected void Stand(IEntity character, vector at, float yaw)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}
}
