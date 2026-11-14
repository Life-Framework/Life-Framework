//------------------------------------------------------------------------------------------------
//! Medic part of the world reset (Core/LF_WorldReset.c): the practice patient is put back at its
//! spot and downed again, so the next test starts with someone to treat.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Patient());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Patient : LF_WorldResetStep
{
	override string GetName()
	{
		return "patient";
	}

	override int Run()
	{
		return LF_DownedPatient.ResetAll();
	}
}
