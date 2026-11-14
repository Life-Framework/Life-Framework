//------------------------------------------------------------------------------------------------
//! Comms part of the world reset (Core/LF_WorldReset.c): the service-call board, finished calls
//! and the call-back offers and requests. A call in progress is left to its players.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Calls());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Calls : LF_WorldResetStep
{
	override string GetName()
	{
		return "calls";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		int count = LF_ServiceBoard.ClearCalls();
		count += LF_PhoneCalls.Exchange().PurgeEnded();
		count += LF_PhoneCallBacks.Book().Clear();
		return count;
	}
}
