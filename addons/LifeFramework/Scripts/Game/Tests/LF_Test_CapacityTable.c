// red-proof: observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_CapacityTable`: RED-OK vehicles/capacity-table (fails with the hook on, 23 assertions); the hook flattens the pickup to the car's 4 sacks, so "the shipped table is a ladder" (got 'pickup carries 0, no more than car (4)') and the pickup bed-units check fail. The first hook (native only) was HOLLOW and was fixed.
//! Capacity ladder (0.3.0 W16, #4189): the table lists body, packs, car, pickup, van and truck, each carries
//! more than the one below it (a car 4 sacks, a van 8), each vehicle row's bed units equal the prefab's
//! cargo bed and its native sacks need a native storage on the prefab, and every chain's best loop needs the
//! next vehicle up. A flat ladder is refused by the same rule.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CapacityTable : LF_Test
{
	override string GetName()
	{
		return "vehicles/capacity-table";
	}

	override void Run(LF_TestContext ctx)
	{
		// The rule itself: a flat ladder and a loop that fits the rung below are refused.
		array<ref LF_CapacityRow> flat = {};
		flat.Insert(LF_CapacityRow.Make("body", 0, 1, 0));
		flat.Insert(LF_CapacityRow.Make("packs", 1, 3, 0));
		flat.Insert(LF_CapacityRow.Make("car", 2, 4, 0));
		flat.Insert(LF_CapacityRow.Make("pickup", 3, 4, 0));
		flat.Insert(LF_CapacityRow.Make("van", 4, 8, 0));
		flat.Insert(LF_CapacityRow.Make("truck", 5, 14, 0));
		ctx.False(LF_CapacityRules.Check(flat, null).IsEmpty(), "a pickup that carries what a car does is refused");
		flat[3].m_iNativeSacks = 6;
		ctx.EqualStr("", LF_CapacityRules.Check(flat, null), "the same ladder grown is sound");
		array<ref LF_CapacityLoop> loops = {};
		loops.Insert(LF_CapacityLoop.Make("coal", 4, "pickup"));
		ctx.False(LF_CapacityRules.Check(flat, loops).IsEmpty(), "a loop a car can do does not need a pickup");

		LF_CapacityConfig config = LF_CapacityConfig.Get();
		ctx.NotNull(config, "the capacity table loads");
		if (!config)
			return;

		if (RedProof())
		{
			// Flatten the ladder: the pickup carries no more than the car.
			config.m_aRows[3].m_iNativeSacks = config.m_aRows[2].Total();
			config.m_aRows[3].m_iBedUnits = 0;
		}

		ctx.EqualStr("", LF_CapacityRules.Check(config.m_aRows, config.m_aLoops), "the shipped table is a ladder");
		ctx.Equal(4, LF_CapacityConfig.SacksOf("car"), "a car carries 4 sacks");
		ctx.Equal(8, LF_CapacityConfig.SacksOf("van"), "a van carries 8 sacks");
		ctx.Equal(0, LF_CapacityConfig.SacksOf("rocket"), "an unknown class carries nothing");

		foreach (LF_CapacityRow row : config.m_aRows)
		{
			if (!row)
				continue;

			if (row.m_Prefab.IsEmpty())
			{
				ctx.Equal(0, row.m_iBedUnits, "body and packs have no bed: " + row.m_sClass);
				continue;
			}

			Resource resource = Resource.Load(row.m_Prefab);
			ctx.True(resource && resource.IsValid(), "the " + row.m_sClass + " prefab loads");
			if (!resource || !resource.IsValid())
				continue;

			BaseContainer prefab = resource.GetResource().ToBaseContainer();
			ctx.Equal(row.m_iBedUnits, BedUnits(prefab), "the " + row.m_sClass + " bed units equal the prefab's cargo bed");
			if (row.m_iNativeSacks > 0)
				ctx.True(HasStorage(prefab), string.Format("the %1 has native storage for its %2 sacks", row.m_sClass, row.m_iNativeSacks));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The LF_CargoBedComponent capacity the prefab or an ancestor sets, 0 for no bed.
	protected int BedUnits(BaseContainer prefab)
	{
		BaseContainer node = prefab;
		while (node)
		{
			BaseContainerList components = node.GetObjectArray("components");
			if (components)
			{
				for (int i = 0; i < components.Count(); i++)
				{
					BaseContainer component = components.Get(i);
					if (component && component.GetClassName() == "LF_CargoBedComponent")
					{
						int capacity;
						component.Get("m_iCapacity", capacity);
						return capacity;
					}
				}
			}

			node = node.GetAncestor();
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab or an ancestor carries an inventory storage component.
	protected bool HasStorage(BaseContainer prefab)
	{
		BaseContainer node = prefab;
		while (node)
		{
			BaseContainerList components = node.GetObjectArray("components");
			if (components)
			{
				for (int i = 0; i < components.Count(); i++)
				{
					BaseContainer component = components.Get(i);
					if (component && component.GetClassName().Contains("Storage"))
						return true;
				}
			}

			node = node.GetAncestor();
		}

		return false;
	}
}
