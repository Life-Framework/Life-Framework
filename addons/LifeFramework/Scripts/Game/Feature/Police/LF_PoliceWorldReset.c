//------------------------------------------------------------------------------------------------
//! Police part of the world reset (Core/LF_WorldReset.c): wanted reports and their posters, the
//! dispatch log, and the spike strips laid on the road.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Police());
		steps.Insert(new LF_WorldReset_SpikeStrips());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Police : LF_WorldResetStep
{
	override string GetName()
	{
		return "police";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		int count = LF_WantedReports.ClearAll() + LF_Dispatch.ClearLog();
		// The posters follow the reports and the wanted levels; rebuild the board now.
		LF_WantedPosters.MarkDirty("world reset");
		return count;
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_SpikeStrips : LF_WorldResetStep
{
	override string GetName()
	{
		return "spikes";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		return LF_SpikeStrips.DeleteAll();
	}
}
