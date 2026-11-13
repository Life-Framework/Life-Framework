//------------------------------------------------------------------------------------------------
//! Farming part of the world reset (Core/LF_WorldReset.c): every planted plot is empty again.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Farm());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Farm : LF_WorldResetStep
{
	override string GetName()
	{
		return "farm";
	}

	override int Run()
	{
		int cleared = 0;
		map<string, LF_FarmPlotComponent> live = LF_FarmPlotComponent.Live();
		for (int i = 0; i < live.Count(); i++)
		{
			LF_FarmPlotComponent plot = live.GetElement(i);
			if (!plot || plot.IsEmpty())
				continue;

			plot.Clear();
			cleared++;
		}

		return cleared;
	}
}
