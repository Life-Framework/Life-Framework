// red-proof: observed 2026-11-17 with `--red` (log test-20261117-231653): RED-OK placement/cap, the cap check skipped: FAIL "the third placement is refused naming the owner cap", "the owner cap has its own toast". Hook off: green.
//! placement/cap (#1032, slice #2653): the next permanent placement past the owner's cap, or past the
//! property's, is refused with that cap named; a temporary kind (the spike strip) is never capped nor
//! recorded; the owner within reach picks a placement up, which deletes its row and the entity, and
//! nobody else (or the owner from too far) can.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementCap : LF_Test
{
	static const ResourceName STRIP_ITEM = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";
	protected static const string OWNER = "placement-cap-owner";
	protected static const string OTHER = "placement-cap-other";
	protected static const string HOUSE = "placement-cap-house";
	protected static const vector SPOT = "100 0 78"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/cap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig caps = new LF_PlacementPolicyConfig();
		caps.m_iOwnerCap = 2;
		caps.m_iPropertyCap = 1;
		LF_PlacementPolicyConfig.UseForTest(caps);

		array<IEntity> spawned = {};
		RunCaps(ctx, spawned);
		RunTemporary(ctx);
		RunPickup(ctx, spawned);

		foreach (IEntity entity : spawned)
		{
			string id = LF_PlacementSave.IdOf(entity);
			if (!id.IsEmpty())
				LF_PlacementSave.Forget(id);
			LF_TestCharacterFixture.Delete(entity);
		}

		LF_PlacementSave.Forget("pl-cap-house");
		LF_PlacementPolicyConfig.Reload();
		ctx.Equal(LF_PlacementSave.DEFAULT_OWNER_CAP, 40, "the decided owner cap is 40");
		ctx.Equal(LF_PlacementSave.DEFAULT_PROPERTY_CAP, 25, "the decided property cap is 25");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCaps(LF_TestContext ctx, notnull array<IEntity> spawned)
	{
		string toast;
		ctx.EqualStr("", LF_PlacementSave.CapProblem(OWNER, "", toast), "an owner with nothing placed is under the cap");

		for (int i = 0; i < 2; i++)
		{
			IEntity chair = LF_Test_PlacementPersist.Spawn(SPOT + Vector(i * 2, 0, 0));
			spawned.Insert(chair);
			LF_PlacementSave.Record(OWNER, "chair", chair);
		}

		string why;
		if (!RedProof())
			why = LF_PlacementSave.CapProblem(OWNER, "", toast);
		ctx.True(why.Contains("owner cap of 2"), "the third placement is refused naming the owner cap: " + why);
		ctx.EqualStr("#LF-Placement_CapOwner", toast, "the owner cap has its own toast");
		ctx.EqualStr("", LF_PlacementSave.CapProblem(OTHER, "", toast), "another owner is not held by this owner's cap");

		// A property's cap counts every owner's placements on it.
		array<ref LF_PlacementRow> onHouse = {};
		vector at[4];
		Math3D.MatrixIdentity4(at);
		vector houseSpot = SPOT + "0 0 4";
		houseSpot[1] = GetGame().GetWorld().GetSurfaceY(houseSpot[0], houseSpot[2]);
		at[3] = houseSpot;
		onHouse.Insert(LF_PlacementRow.Create("pl-cap-house", LF_Test_PlacementPersist.CHAIR, at, OTHER, "chair", HOUSE, ""));
		ctx.Equal(1, LF_PlacementSave.ApplyAll(onHouse), "a placement stands on the property");
		spawned.Insert(LF_PlacementSave.LiveEntity("pl-cap-house"));
		string houseWhy = LF_PlacementSave.CapProblem(OWNER + "-2", HOUSE, toast);
		ctx.True(houseWhy.Contains("property cap of 1"), "a placement past the property cap is refused naming it: " + houseWhy);
		ctx.EqualStr("#LF-Placement_CapProperty", toast, "the property cap has its own toast");
	}

	//------------------------------------------------------------------------------------------------
	//! The spike strip is TEMPORARY: never capped, never recorded, whatever its placer holds.
	protected void RunTemporary(LF_TestContext ctx)
	{
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity item = LF_Utils.SpawnEntityPrefab(STRIP_ITEM, SPOT + "0 0 -6");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!owner || !placeable)
		{
			ctx.Fail("the owner and the folded strip spawn");
			LF_TestCharacterFixture.Delete(owner);
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		LF_Houses.BindCharacter(owner, OWNER);
		ctx.Equal(LF_EPlacementLifetime.TEMPORARY, placeable.GetLifetime(), "the spike strip is temporary");
		string toast;
		ctx.EqualStr("", LF_PlacementSave.CheckBeforePlace(owner, placeable, SPOT, toast), "a temporary kind is not capped at the owner's cap");
		int before = LF_PlacementSave.Count();
		ctx.EqualStr("", LF_PlacementSave.OnPlaced(owner, placeable, item, item), "a temporary kind is not recorded");
		ctx.Equal(before, LF_PlacementSave.Count(), "no row was added for the temporary kind");
		LF_TestCharacterFixture.Delete(item);
		LF_TestCharacterFixture.Delete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPickup(LF_TestContext ctx, notnull array<IEntity> spawned)
	{
		IEntity chair = spawned.Get(0);
		string id = LF_PlacementSave.IdOf(chair);
		ctx.False(id.IsEmpty(), "the first chair is a recorded placement");

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (!owner || !other || !chair)
		{
			ctx.Fail("the owner, a bystander and the chair exist");
			LF_TestCharacterFixture.Delete(owner);
			LF_TestCharacterFixture.Delete(other);
			return;
		}

		LF_Houses.BindCharacter(owner, OWNER);
		LF_Houses.BindCharacter(other, OTHER);
		other.SetOrigin(chair.GetOrigin() + "1 0 0");
		owner.SetOrigin(chair.GetOrigin() + "0 0 10");

		ctx.False(LF_PlacementPickup.PickUp(other, chair), "a bystander cannot pick up the owner's chair");
		ctx.True(LF_PlacementPickup.LastRefusal().Contains("only the owner"), "the refusal names the owner rule: " + LF_PlacementPickup.LastRefusal());
		ctx.False(LF_PlacementPickup.PickUp(owner, chair), "the owner 10 m away cannot pick it up");
		ctx.True(LF_PlacementPickup.LastRefusal().Contains("too far"), "the refusal names the reach: " + LF_PlacementPickup.LastRefusal());

		owner.SetOrigin(chair.GetOrigin() + "1 0 0");
		ctx.True(LF_PlacementPickup.PickUp(owner, chair), "the owner within reach picks it up: " + LF_PlacementPickup.LastRefusal());
		ctx.True(LF_PlacementSave.Find(id) == null, "the pick-up deletes the record");
		ctx.True(LF_PlacementSave.LiveEntity(id) == null, "the pick-up deletes the chair");
		ctx.Equal(1, LF_PlacementSave.CountOwned(OWNER), "the owner is under the cap again");
		string toast;
		ctx.EqualStr("", LF_PlacementSave.CapProblem(OWNER, "", toast), "after the pick-up the owner may place one more");

		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(other);
	}
}
