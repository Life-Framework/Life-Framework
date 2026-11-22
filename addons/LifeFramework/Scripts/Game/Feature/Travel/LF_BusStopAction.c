// LF_BusStopAction.c - "Take the bus" on a bus stop sign. Opens the timetable on the player's own
// machine (LF_BusStopSheet); the ticket itself is a server request that checks everything again.
[BaseContainerProps(configRoot: true)]
class LF_BusStopAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_BusStop.SYSTEM) && LF_BusStopComponent.Of(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-BusStop_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The timetable is local UI.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus stop action performed without a stop or a user");
			return;
		}

		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		LF_BusStopSheet.Open(pOwnerEntity);
	}
}
