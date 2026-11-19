//------------------------------------------------------------------------------------------------
//! Bins part of the world reset (Core/LF_WorldReset.c): every bin is empty again (#3323).

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Bins());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Bins : LF_WorldResetStep
{
	override string GetName()
	{
		return "bins";
	}

	override int Run()
	{
		return LF_BinComponent.EmptyAll();
	}
}
