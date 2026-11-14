//------------------------------------------------------------------------------------------------
//! Mining part of the world reset (Core/LF_WorldReset.c): every rock grows back whole.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Ore());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Ore : LF_WorldResetStep
{
	override string GetName()
	{
		return "ore";
	}

	override int Run()
	{
		// Mining off: no rock grows back; a rock already spent keeps its state (#1075).
		if (!LF_Systems.IsEnabled(LF_OreNodeComponent.SYSTEM))
			return 0;

		int regrown = 0;
		array<IEntity> all = LF_WorldResetScan.All();
		foreach (IEntity entity : all)
		{
			if (!entity)
				continue;

			LF_OreNodeComponent node = LF_OreNodeComponent.Cast(entity.FindComponent(LF_OreNodeComponent));
			if (!node)
				continue;

			node.Regrow();
			regrown++;
		}

		return regrown;
	}
}
