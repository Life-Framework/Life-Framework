//------------------------------------------------------------------------------------------------
//! "Rent a locker" on a locker bank (#2659). The engine runs PerformAction on the server for the
//! acting character; the server finds the bank from that character's position and takes the cash
//! (LF_LockerBank.RentNearest). The client only shows the prompt.
class LF_LockerRentAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_LockerBank.FEATURE);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// No price check here: the owner's price layer is read on the server only, so a client check could
		// grey out a rent the server would take. The server refuses short cash with a toast.
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_LockerBank.FEATURE, "LF_LockerRentAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, "rent action performed without a character");
			return;
		}

		LF_ELockerBankResult result = LF_LockerBank.RentNearest(pUserEntity);
		LF_Utils.NotifyEntity(pUserEntity, LF_LockerBank.ResultKey(result), "#LF-LockerBank_Title", 4.0);
	}
}
