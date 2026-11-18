// red-proof: 2026-11-18 `tools\cli test --no-build --only LF_Test_MiningReagentItems` before the prefabs existed: `coal lump prefab loads` and the other two failed (GUIDs not in the resource database). `--red` keeps RedProof() from spawning anything, so every check fails.
//! mining/reagent-items (#3230): the coal lump, limestone block and acid carboy of the mining chains load,
//! spawn with a vanilla mesh, carry localization keys for name and description, and weigh what the design
//! says: coal is a light reagent, limestone is heavy (about 10 fill a backpack, so stone needs a truck) and
//! the acid carboy is a full two-handed load.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MiningReagentItems : LF_Test
{
	protected const string COAL = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";
	protected const string LIMESTONE = "{6ADCC5039A8EF571}Prefabs/Resources/Mining/Limestone.et";
	protected const string ACID = "{C4CF913B23D64824}Prefabs/Items/Resources/AcidCarboy.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/reagent-items";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float coal = CheckItem(ctx, COAL, "the coal lump", "LF-Item_CoalLump", "Assets/Clutter/Coal/", 0.3, 2.0);
		float limestone = CheckItem(ctx, LIMESTONE, "the limestone block", "LF-Item_Limestone", "Assets/Props/Debris/", 2.5, 6.0);
		float acid = CheckItem(ctx, ACID, "the acid carboy", "LF-Item_AcidCarboy", "Assets/Items/Fuel/Jerrycan_01/Jerrycan_01.xob", 5.0, 20.0);

		ctx.True(limestone > coal * 2, string.Format("a limestone block (%1 kg) is much heavier than a coal lump (%2 kg)", limestone, coal));
		ctx.True(acid > limestone, string.Format("an acid carboy (%1 kg) outweighs one limestone block (%2 kg)", acid, limestone));
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one prefab and checks it: loads, spawns, uses the expected vanilla mesh, names itself with the
	//! `#<key>_Name` and `#<key>_Description` keys and weighs between minKg and maxKg. Deletes it and returns its weight (0 when a check failed early).
	protected float CheckItem(LF_TestContext ctx, ResourceName prefab, string label, string key, string meshPart, float minKg, float maxKg)
	{
		if (RedProof())
		{
			ctx.Fail(label + " red proof: nothing spawned");
			return 0;
		}

		Resource res = Resource.Load(prefab);
		ctx.True(res.IsValid(), label + " prefab loads");
		if (!res.IsValid())
			return 0;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(item, label + " spawns");
		if (!item)
			return 0;

		VObject mesh = item.GetVObject();
		ctx.True(mesh != null, label + " has a mesh");
		if (mesh)
		{
			string xob = mesh.GetResourceName();
			ctx.True(xob.Contains(meshPart), label + " uses the vanilla mesh " + meshPart + " (" + xob + ")");
		}

		float weight = 0;
		InventoryItemComponent inv = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		ctx.NotNull(inv, label + " is an inventory item");
		if (inv)
		{
			UIInfo info = inv.GetUIInfo();
			ctx.NotNull(info, label + " has a display name");
			if (info)
			{
				ctx.EqualStr("#" + key + "_Name", info.GetName(), label + " name is a localization key");
				ctx.EqualStr("#" + key + "_Description", info.GetDescription(), label + " description is a localization key");
			}

			ItemPhysicalAttributes phys = ItemPhysicalAttributes.Cast(inv.FindAttribute(ItemPhysicalAttributes));
			ctx.NotNull(phys, label + " has physical attributes");
			if (phys)
			{
				weight = phys.GetWeight();
				ctx.True(weight >= minKg && weight <= maxKg, string.Format("%1 weighs %2 kg, inside %3 to %4 kg", label, weight, minKg, maxKg));
				ctx.True(phys.GetVolume() > 0, label + " has a volume");
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return weight;
	}
}
