// red-proof: 2026-11-18 tools/cli test --no-build --red LF_Test_FishingItems RED-OK (RedProof() spawns nothing, so every prefab check fails); the plain run is green.
//! fishing/items: the first fishing items (#2928) are real: the rod, the bait tin and the trout in three sizes
//! each load and spawn with their own mesh (not the placeholder pickaxe and apple of #2927), fit an inventory,
//! the rod is a hand tool with tool id rod, and the three trout grow from small to large.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishingItems : LF_Test
{
	protected const string ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";
	protected const string TIN = "{53E0B27FA119A866}Prefabs/Items/Fishing/LF_BaitTin.et";
	protected const string TROUT_SMALL = "{FAEF245300848A2C}Prefabs/Items/Fishing/LF_FishTroutSmall.et";
	protected const string TROUT = "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et";
	protected const string TROUT_LARGE = "{368CD7CE4669CC29}Prefabs/Items/Fishing/LF_FishTroutLarge.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/items";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity rod = Spawn(ctx, ROD, "the rod", 4.0);
		if (rod)
		{
			LF_HandToolComponent tool = LF_HandTool.Of(rod);
			ctx.NotNull(tool, "the rod carries LF_HandToolComponent");
			if (tool)
			{
				ctx.EqualStr("rod", tool.GetToolId(), "the rod's tool id is rod");
				ctx.True(tool.CanBeHeld(), "the rod can be held in the hands");
			}

			SCR_EntityHelper.DeleteEntityAndChildren(rod);
		}

		IEntity tin = Spawn(ctx, TIN, "the bait tin", 0.5);
		if (tin)
			SCR_EntityHelper.DeleteEntityAndChildren(tin);

		float small = SizeOf(ctx, TROUT_SMALL, "the small trout");
		float medium = SizeOf(ctx, TROUT, "the trout");
		float large = SizeOf(ctx, TROUT_LARGE, "the large trout");
		ctx.True(small > 0.1 && small < medium, string.Format("the small trout (%1 m) is shorter than the medium (%2 m)", small, medium));
		ctx.True(large > medium, string.Format("the large trout (%1 m) is longer than the medium (%2 m)", large, medium));
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab and checks it: loads, spawns, has a mesh from Assets/Items/Fishing (a placeholder
	//! would be a vanilla xob) and a volume that fits an inventory (above zero, at most `maxVolumeL` litres). Returns the entity, or null after a failed check; the caller deletes it.
	protected IEntity Spawn(LF_TestContext ctx, ResourceName prefab, string label, float maxVolumeL)
	{
		if (RedProof())
		{
			ctx.Fail(label + " red proof: nothing spawned");
			return null;
		}

		Resource res = Resource.Load(prefab);
		ctx.True(res.IsValid(), label + " prefab loads");
		if (!res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(item, label + " spawns");
		if (!item)
			return null;

		VObject mesh = item.GetVObject();
		ctx.True(mesh != null, label + " has a mesh");
		if (mesh)
		{
			string xob = mesh.GetResourceName();
			ctx.True(xob.Contains("Assets/Items/Fishing/") && xob.EndsWith(".xob"), label + " uses its own xob, not a placeholder (" + xob + ")");
		}

		InventoryItemComponent inv = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		ctx.NotNull(inv, label + " is an inventory item");
		if (inv)
		{
			ItemPhysicalAttributes phys = ItemPhysicalAttributes.Cast(inv.FindAttribute(ItemPhysicalAttributes));
			ctx.NotNull(phys, label + " has physical attributes");
			if (phys)
			{
				float litres = phys.GetVolume() / 1000.0;
				ctx.True(litres > 0 && litres <= maxVolumeL, string.Format("%1 fits an inventory (%2 l)", label, litres));
				ctx.True(phys.GetWeight() > 0 && phys.GetWeight() < 5, string.Format("%1 has a sane weight (%2 kg)", label, phys.GetWeight()));
			}
		}

		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! The longest bounding-box side of a spawned fish; deletes it.
	protected float SizeOf(LF_TestContext ctx, ResourceName prefab, string label)
	{
		IEntity fish = Spawn(ctx, prefab, label, 2.0);
		if (!fish)
			return 0;

		vector mins;
		vector maxs;
		fish.GetWorldBounds(mins, maxs);
		vector span = maxs - mins;
		float longest = Math.Max(span[0], Math.Max(span[1], span[2]));
		SCR_EntityHelper.DeleteEntityAndChildren(fish);
		return longest;
	}
}
