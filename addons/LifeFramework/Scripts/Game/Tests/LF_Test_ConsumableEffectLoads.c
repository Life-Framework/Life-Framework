// red-proof: observed 2026-11-14 with `toolscli test --no-build --only LF_Test_ConsumableEffectLoads`: with [BaseContainerProps()] removed from modded SCR_ConsumableBandage it failed `the bandage's effect entry loads as SCR_ConsumableBandage` and the log had `Unknown class 'SCR_ConsumableBandage'`; restored, green.
// tier: WORLD
//! The vanilla field dressing prefab keeps its SCR_ConsumableBandage effect. A `modded class` of
//! that config class must repeat the base's [BaseContainerProps()]; without it the prefab's effect
//! entry fails with "Unknown class 'SCR_ConsumableBandage'" and the item does nothing.
class LF_Test_ConsumableEffectLoads : LF_Test
{
	static const ResourceName BANDAGE_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";

	override string GetName()
	{
		return "medic/consumable-effect-loads";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(BANDAGE_PREFAB);
		if (!res.IsValid())
		{
			ctx.Fail("bandage prefab does not load: " + BANDAGE_PREFAB);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!item)
		{
			ctx.Fail("bandage prefab did not spawn");
			return;
		}

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (!consumable)
			ctx.Fail("bandage has no SCR_ConsumableItemComponent");
		else
			ctx.NotNull(SCR_ConsumableBandage.Cast(consumable.GetConsumableEffect()), "the bandage's effect entry loads as SCR_ConsumableBandage");

		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}
}
