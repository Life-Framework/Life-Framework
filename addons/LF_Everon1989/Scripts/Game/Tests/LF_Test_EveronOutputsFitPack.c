// red-proof: observed 2026-11-23 with `tools\cli test --no-build --only LF_Test_EveronOutputsFitPack` on release-0.2.1 ec62b7bcc: FAIL `AppleJuice lands in the backpack or a pocket, not in the hands (w=0.01 kg v=1664 cm3 dims=<8,26,8>)`; with ItemPhysAttributes (Volume 550) on AppleJuice.et it passes.
//! 0.2.1 blocker N1: the juice press took three apples and toasted "Apple Juice x1", but the bottle
//! dropped at the newcomer's feet and could not be picked up (`#AR-Inventory_InfoTooHeavy`), so the
//! tutorial's "Market day" sale could never happen. A fresh newcomer given the island's starting kit
//! (the canvas backpack) must take every pocketable output the first two hours hand out through the
//! give path the press, the fishing line and the gather rows use (`LF_InventoryUtils.AddAmount`), and the
//! item must land in the inventory, not on the ground. Planks are hand-carried (`HandCarryItem_Base`)
//! and stay out of the list on purpose.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronOutputsFitPack : LF_Test
{
	static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/outputs-fit-pack";
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ResourceName> Outputs()
	{
		return {
			JUICE,
			"{C9D661E5B0714711}Prefabs/Items/Food/Apple.et",
			"{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et",
			"{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et",
			"{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et",
			"{10AC1C974EEB714B}Prefabs/Items/Fishing/LF_FishPerch.et",
			"{978A71CB3CBE87F7}Prefabs/Items/Fishing/LF_FishCarp.et",
			"{C240073B498C7A71}Prefabs/Items/Fishing/LF_FishBass.et",
			"{9412431FCE67E0FA}Prefabs/Items/Fishing/LF_FishMackerel.et",
			"{320AF694AB999DF5}Prefabs/Items/Fishing/LF_SmokedTrout.et",
		};
	}

	//------------------------------------------------------------------------------------------------
	//! A bare character at a civilian spawn point, wearing nothing the kit does not give it.
	protected IEntity SpawnBare(LF_TestContext ctx)
	{
		Resource res = Resource.Load(LF_TestCharacterFixture.CHARACTER_PREFAB);
		if (!res.IsValid())
		{
			ctx.Fail("the character prefab loads");
			return null;
		}

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
		{
			ctx.Fail("a spawn point resolves for the newcomer");
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! "w=0.55 kg v=550 cm3 dims=8 8 8" for the failure message.
	protected static string Measure(ResourceName prefab)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, "0 0 0");
		if (!item)
			return "no spawn";

		string text = "no physical attributes";
		InventoryItemComponent inv = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (inv)
		{
			ItemPhysicalAttributes phys = ItemPhysicalAttributes.Cast(inv.FindAttribute(ItemPhysicalAttributes));
			if (phys)
				text = string.Format("w=%1 kg v=%2 cm3 dims=%3", phys.GetWeight(), phys.GetVolume(), phys.GetDimensions());
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every carried `prefab` sits in a worn item's storage (backpack, jacket, trousers).
	//! The hand slot does not count: a player walking up to a press usually holds something, and a
	//! bottle that only fits the hands drops at their feet (N1).
	protected static bool InCarriedStorage(IEntity character, ResourceName prefab)
	{
		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (!manager)
			return false;

		array<IEntity> found = LF_InventoryUtils.FindItemsByPrefab(manager, prefab);
		if (found.IsEmpty())
			return false;

		foreach (IEntity item : found)
		{
			InventoryItemComponent inv = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!inv)
				return false;

			InventoryStorageSlot slot = inv.GetParentSlot();
			if (!slot || !slot.GetStorage() || slot.GetStorage().GetOwner() == character)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		ctx.NotNull(spawnLogic, "the pack leg runs the LF spawn logic");
		if (!spawnLogic)
			return;

		IEntity newcomer = SpawnBare(ctx);
		ctx.NotNull(newcomer, "the newcomer spawns");
		if (!newcomer)
			return;

		ctx.True(spawnLogic.GiveRoleKit(newcomer, LF_Faction.CIVILIAN), "the newcomer is given the civilian starting kit");

		// A real newcomer already carries the welcome water bottle in the drink slot, so a bottle too big
		// for the backpack has nowhere else to go. A bare kit hid N1: the juice slipped into that slot.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(newcomer, WATER, 1), "the newcomer carries the welcome water bottle");

		foreach (ResourceName prefab : Outputs())
		{
			string label = FilePath.StripExtension(FilePath.StripPath(prefab));
			int before = LF_InventoryUtils.GetAmount(newcomer, prefab);
			// No overflow drop here: a refused insert returns 0 and deletes the item.
			int added = LF_InventoryUtils.AddAmount(newcomer, prefab, 1);
			int after = LF_InventoryUtils.GetAmount(newcomer, prefab);
			ctx.True(added == 1 && after == before + 1, string.Format("a fresh newcomer's pack takes %1 (%2)", label, Measure(prefab)));
			if (added == 1)
				ctx.True(InCarriedStorage(newcomer, prefab), string.Format("%1 lands in the backpack or a pocket, not in the hands (%2)", label, Measure(prefab)));
			if (added > 0)
				LF_InventoryUtils.RemoveAmount(newcomer, prefab, added);
		}

		// A give that still does not fit says where it went instead of vanishing.
		string dropped = LF_InventoryUtils.DroppedText(JUICE, 1);
		ctx.True(dropped.Contains("Apple Juice") && dropped.Contains("feet"), "a dropped give tells the player where it is (" + dropped + ")");

		LF_TestCharacterFixture.Delete(newcomer);
	}
}
