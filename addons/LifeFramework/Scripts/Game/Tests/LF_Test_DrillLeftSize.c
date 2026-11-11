// red-proof: 2026-11-11, with LF_Bank.SetDown moving the dropped drill instead of copying it (the old LeaveTool),
//   cli test --no-build --only LF_Test_DrillLeftSize:
//   [LFTEST]   - the left drill is at the drill prefab's scale (expected 0.4, got 1)
//   [LFTEST]   - the left drill is a safe drill's size (1.16937 m tall, want 0.3 to 0.6)
//   and the server log: [LFDebug:Bank] drill out of the inventory at scale 1 (prefab 0.4). Restored; green.
// tier: WORLD
//! crime/drill-left-size: a drill left at a vault or the cash van box lies at the drill prefab's size (a 0.47 m
//! safe drill), upright, with its serial. The prefab is the vanilla bench drill at 0.4, but an item that leaves an
//! inventory is back at scale 1, so the left drill stood there as a 1.2 m green pillar drill (#571 review).
class LF_Test_DrillLeftSize : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/drill-left-size";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		// The fixture's jacket is too small for a drill; a field pack carries it.
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack");
		ctx.True(LF_InventoryUtils.AddItem(inventory, LF_Bank.DRILL_PREFAB), "a drill is handed over");
		array<IEntity> drills = LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Bank.DRILL_PREFAB);
		if (drills.IsEmpty())
		{
			ctx.Fail("the drill is not in the inventory");
			LF_TestCharacterFixture.Delete(robber);
			return;
		}

		IEntity drill = drills[0];
		LF_SerialComponent stamp = LF_SerialComponent.Cast(drill.FindComponent(LF_SerialComponent));
		ctx.True(stamp && stamp.SetSerial("LF-TEST-77"), "the fence stamps the drill");
		InventoryItemComponent item = InventoryItemComponent.Cast(drill.FindComponent(InventoryItemComponent));
		ctx.True(item && item.GetParentSlot() && inventory.TryRemoveItemFromStorage(drill, item.GetParentSlot().GetStorage()), "the drill leaves the inventory");
		LF_Debug.Info("Bank", string.Format("drill out of the inventory at scale %1 (prefab %2)", drill.GetScale(), LF_Utils.PrefabScale(Resource.Load(LF_Bank.DRILL_PREFAB))));

		vector spot = robber.GetOrigin() + "1.5 0 0";
		IEntity left = LF_Bank.SetDown(drill, spot, 30);
		ctx.NotNull(left, "a drill lies at the spot");
		if (left)
		{
			ctx.EqualFloat(LF_Utils.PrefabScale(Resource.Load(LF_Bank.DRILL_PREFAB)), left.GetScale(), 0.01, "the left drill is at the drill prefab's scale");
			vector mins;
			vector maxs;
			left.GetWorldBounds(mins, maxs);
			float tall = maxs[1] - mins[1];
			ctx.True(tall > 0.3 && tall < 0.6, string.Format("the left drill is a safe drill's size (%1 m tall, want 0.3 to 0.6)", tall));
			ctx.True(vector.DistanceXZ(left.GetOrigin(), spot) < 0.01, "it lies at the spot");
			ctx.EqualFloat(30, left.GetYawPitchRoll()[0], 0.5, "it faces the way it was set down");
			LF_SerialComponent kept = LF_SerialComponent.Cast(left.FindComponent(LF_SerialComponent));
			ctx.True(kept && kept.GetSerial() == "LF-TEST-77", "it keeps the fence serial");
			SCR_EntityHelper.DeleteEntityAndChildren(left);
		}

		LF_TestCharacterFixture.Delete(robber);
	}
}
