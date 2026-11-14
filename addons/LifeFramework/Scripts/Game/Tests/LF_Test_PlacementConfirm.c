// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_PlacementConfirm`: LF_PlacementService.Confirm with its tolerance check disabled failed `a proposal 3 m off is refused`, `... the logged reason says 'from the server's spot'` and `a proposal 3 m off keeps the item` (the strip was placed 3 m off); restored, green.
// tier: WORLD
//! #1026 acceptance: the placement service's server confirm. An on-duty officer stands on the main
//! street's south verge facing across it with the folded spike strip at their feet. A proposal at
//! the server's own plan places the strip there and uses the item up. A proposal 3 m along the
//! street, one turned 30 degrees, one on the strip just laid, and one from a civilian proposing
//! the same place are each refused with the reason logged, and their items stay.
class LF_Test_PlacementConfirm : LF_Test
{
	static const ResourceName ITEM = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";
	static const ResourceName STRIP = "{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et";
	protected static const string OFFICER_UID = "placement-confirm-officer";
	protected static const string CIVILIAN_UID = "placement-confirm-civilian";
	//! Open verge south of Village_MainStreet (centre line z = 250), east of the fuel station.
	protected static const vector VERGE = "300 0 245.5";

	override string GetName()
	{
		return "placement/confirm";
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

		Stand(officer, VERGE);
		// Out of the way while the officer places.
		Stand(civilian, VERGE + "12 0 0");
		IEntity strip = RunConfirm(ctx, officer, civilian);

		LF_TestCharacterFixture.Delete(strip);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity RunConfirm(LF_TestContext ctx, IEntity officer, IEntity civilian)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!placeable)
		{
			ctx.Fail("the folded strip has no LF_PlaceableComponent");
			LF_TestCharacterFixture.Delete(item);
			return null;
		}

		ctx.True(LF_Utils.SamePrefab(placeable.GetPlacedPrefab(), STRIP), "the folded strip places the strip prefab");
		ctx.Equal(LF_EPlacementWho.ON_DUTY_POLICE, placeable.GetWho(), "only on-duty police place it");

		// The server's own spot for the officer: what their ghost shows.
		vector own[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		LF_EPlacementVerdict verdict = LF_PlacementService.Plan(officer, placeable, item, own, probe);
		ctx.Equal(LF_EPlacementVerdict.VALID, verdict, "the verge in front of the officer is valid: " + LF_PlacementRules.Reason(verdict, probe, LF_PlacementService.MAX_STEP));

		// 3 m along the street: further than the tolerance from the server's spot.
		vector far[4];
		Copy(own, far);
		far[3] = own[3] + "3 0 0";
		ExpectRefused(ctx, officer, item, far, "from the server's spot", "a proposal 3 m off");

		// Turned 30 degrees: the heading is outside the tolerance.
		vector turned[4];
		Copy(own, turned);
		vector yaw[4];
		Math3D.AnglesToMatrix("30 0 0", yaw);
		turned[2] = yaw[2];
		ExpectRefused(ctx, officer, item, turned, "from the server's spot", "a proposal turned 30 degrees");

		// The civilian at the officer's spot proposes the same place.
		IEntity civilianItem = LF_Utils.SpawnEntityPrefab(ITEM, civilian.GetOrigin() + "0.5 0 0");
		ExpectRefused(ctx, civilian, civilianItem, own, "not on-duty police", "a civilian's proposal");
		LF_TestCharacterFixture.Delete(civilianItem);

		// Within the tolerance (20 cm along the street): placed where the proposal said.
		vector near[4];
		Copy(own, near);
		near[3] = own[3] + "0.2 0 0";
		// A wrongly accepted proposal above used the item up (and failed there); carry on with another.
		if (!item)
			item = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");

		EntityID itemId = item.GetID();
		int before = LF_SpikeStrips.Count();
		IEntity strip = LF_PlacementService.Confirm(officer, item, near);
		if (!strip)
		{
			ctx.Fail("the officer's proposal at the server's spot was refused: " + LF_PlacementService.LastRefusal());
			LF_TestCharacterFixture.Delete(item);
			return null;
		}

		ctx.Equal(before + 1, LF_SpikeStrips.Count(), "the placed strip registers");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) == null, "the folded item is used up");
		vector placed[4];
		strip.GetWorldTransform(placed);
		ctx.InRange(Horizontal(placed[3], near[3]), 0, 0.05, "the strip lies where the proposal put it, not at the server's own spot");
		float ground = GetGame().GetWorld().GetSurfaceY(placed[3][0], placed[3][2]);
		ctx.InRange(placed[3][1] - ground, -0.05, 0.3, "the strip lies on the ground");

		// A second strip proposed on top of the first.
		IEntity second = LF_Utils.SpawnEntityPrefab(ITEM, officer.GetOrigin() + "0.5 0 0");
		ExpectRefused(ctx, officer, second, near, "in the way", "a proposal on the laid strip");
		LF_TestCharacterFixture.Delete(second);
		return strip;
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectRefused(LF_TestContext ctx, IEntity player, IEntity item, vector proposal[4], string reason, string label)
	{
		if (!item)
		{
			ctx.Fail(label + ": no item left to propose with (an earlier proposal used it up)");
			return;
		}

		int before = LF_SpikeStrips.Count();
		EntityID itemId = item.GetID();
		IEntity placed = LF_PlacementService.Confirm(player, item, proposal);
		ctx.True(placed == null, label + " is refused");
		if (placed)
			LF_TestCharacterFixture.Delete(placed);

		string refusal = LF_PlacementService.LastRefusal();
		ctx.True(refusal.IndexOf(reason) != -1, string.Format("%1: the logged reason says '%2': %3", label, reason, refusal));
		ctx.Equal(before, LF_SpikeStrips.Count(), label + " places nothing");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) != null, label + " keeps the item");
	}

	//------------------------------------------------------------------------------------------------
	protected void Copy(vector from[4], out vector to[4])
	{
		for (int i = 0; i < 4; i++)
			to[i] = from[i];
	}

	//------------------------------------------------------------------------------------------------
	protected float Horizontal(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}

	//------------------------------------------------------------------------------------------------
	protected void Stand(IEntity character, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix("0 0 0", transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}
}
