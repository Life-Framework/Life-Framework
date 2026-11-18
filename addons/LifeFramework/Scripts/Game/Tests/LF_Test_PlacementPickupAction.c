// red-proof: observed 2026-11-18 (#2751) with `tools\cli test --no-build --red LF_Test_PlacementPickupAction`: RedProof() skips the owner's PerformAction, so `the owner's Pick up removes the record` and `... removes the chair` fail (RED-OK placement/pickup-action).
// tier: WORLD
//! #2751 (after #2653): a placed permanent prop has a "Pick up" action. Every furniture item's placed
//! prefab carries it and the owner stamp; the action is shown to the player who placed the prop and
//! to nobody else; a bystander performing it changes nothing; the owner performing it takes the prop
//! back (entity and record gone), which is LF_PlacementPickup.PickUp, so the reach is still the server's.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementPickupAction : LF_Test
{
	static const ResourceName CHAIR_ITEM = "{D77E0D49C10C35BB}Prefabs/Placeables/Furniture/Furniture_Chair_Item.et";
	static const ResourceName TABLE_ITEM = "{FEF81109318DEF2C}Prefabs/Placeables/Furniture/Furniture_Table_Item.et";
	protected static const string OWNER = "pickup-action-owner";
	protected static const string OTHER = "pickup-action-other";
	protected static const vector SPOT = "100 0 66"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/pickup-action";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlacementPickupAction FindPickUp(IEntity entity)
	{
		if (!entity)
			return null;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_PlacementPickupAction pickUp = LF_PlacementPickupAction.Cast(action);
			if (pickUp)
				return pickUp;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab an item places, resolved from the item's own LF_PlaceableComponent.
	protected static ResourceName PlacedPrefabOf(LF_TestContext ctx, ResourceName itemPrefab)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(itemPrefab, SPOT + "0 0 30");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		ResourceName placed;
		if (placeable)
			placed = placeable.GetPlacedPrefab();

		ctx.True(!placed.IsEmpty(), "the item places a prefab: " + itemPrefab);
		LF_TestCharacterFixture.Delete(item);
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunItem(ctx, TABLE_ITEM, "Furniture_Table", SPOT + "6 0 0", false);
		RunItem(ctx, CHAIR_ITEM, "Furniture_Chair", SPOT, true);
	}

	//------------------------------------------------------------------------------------------------
	//! One furniture item: its placed prefab has the action and the stamp; with `full`, the owner and a
	//! bystander try it.
	protected void RunItem(LF_TestContext ctx, ResourceName itemPrefab, string kind, vector at, bool full)
	{
		ResourceName placedPrefab = PlacedPrefabOf(ctx, itemPrefab);
		if (placedPrefab.IsEmpty())
			return;

		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity placed = LF_Utils.SpawnEntityPrefab(placedPrefab, at);
		ctx.True(placed != null, "the placed prefab spawns: " + placedPrefab);
		if (!placed)
			return;

		string id = LF_PlacementSave.Record(OWNER, kind, placed);
		ctx.False(id.IsEmpty(), kind + " is recorded as a permanent placement");
		LF_PlacementPickupAction pickUp = FindPickUp(placed);
		ctx.True(pickUp != null, kind + " carries a Pick up action");
		LF_PlacedOwnerComponent stamp = LF_Component<LF_PlacedOwnerComponent>.Find(placed);
		ctx.True(stamp != null && stamp.GetOwnerKey() == OWNER, kind + " carries its owner's key");
		if (!full || !pickUp)
		{
			LF_PlacementSave.Forget(id);
			LF_TestCharacterFixture.Delete(placed);
			return;
		}

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (!owner || !other)
		{
			ctx.Fail("the owner and a bystander exist");
			LF_TestCharacterFixture.Delete(owner);
			LF_TestCharacterFixture.Delete(other);
			LF_PlacementSave.Forget(id);
			LF_TestCharacterFixture.Delete(placed);
			return;
		}

		LF_Houses.BindCharacter(owner, OWNER);
		LF_Houses.BindCharacter(other, OTHER);
		owner.SetOrigin(placed.GetOrigin() + "1 0 0");
		other.SetOrigin(placed.GetOrigin() + "-1 0 0");

		ctx.True(pickUp.CanBeShownScript(owner), "the owner is shown Pick up");
		ctx.False(pickUp.CanBeShownScript(other), "a bystander is not shown Pick up");
		ctx.True(pickUp.CanBePerformedScript(owner), "the owner may perform Pick up");
		ctx.False(pickUp.CanBePerformedScript(other), "a bystander may not perform Pick up");

		// A bystander who performs it anyway (a client can send what it likes): the server refuses.
		pickUp.PerformAction(placed, other);
		ctx.True(LF_PlacementSave.Find(id) != null, "a bystander's Pick up leaves the record");
		ctx.True(LF_PlacementSave.LiveEntity(id) != null, "a bystander's Pick up leaves the prop");
		ctx.True(LF_PlacementPickup.LastRefusal().Contains("only the owner"), "the refusal names the owner rule: " + LF_PlacementPickup.LastRefusal());

		// The owner, too far from it: the server's reach refuses.
		owner.SetOrigin(placed.GetOrigin() + "0 0 10");
		pickUp.PerformAction(placed, owner);
		ctx.True(LF_PlacementSave.LiveEntity(id) != null, "the owner 10 m away leaves the prop");
		ctx.True(LF_PlacementPickup.LastRefusal().Contains("too far"), "the refusal names the reach: " + LF_PlacementPickup.LastRefusal());

		// The act: the owner, within reach. Red proof: skip it.
		owner.SetOrigin(placed.GetOrigin() + "1 0 0");
		if (!RedProof())
			pickUp.PerformAction(placed, owner);

		ctx.True(LF_PlacementSave.Find(id) == null, "the owner's Pick up removes the record");
		ctx.True(LF_PlacementSave.LiveEntity(id) == null, "the owner's Pick up removes the chair");

		if (LF_PlacementSave.Find(id))
			LF_PlacementSave.Forget(id);

		LF_TestCharacterFixture.Delete(placed);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(other);
	}
}
