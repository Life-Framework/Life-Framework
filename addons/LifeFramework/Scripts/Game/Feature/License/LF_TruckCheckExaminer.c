//------------------------------------------------------------------------------------------------
//! The examiner's one action (WK-W21, issue #4696): "Take the truck check ($300)". The server takes the
//! fee, lends the truck and opens the run (LF_TruckCheckLend.Take); a refusal answers in words. The
//! action sits on the examiner's prefab, which the town pass places.
class LF_TruckCheckExaminerAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "LF_TruckCheckExaminerAction"))
			return;

		int minutesLeft;
		LF_ETruckCheckLend outcome = LF_TruckCheckLend.Take(pUserEntity, pOwnerEntity, minutesLeft);

		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (outcome != LF_ETruckCheckLend.OK)
			type = LF_ENotificationType.WARNING;

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(pUserEntity), "#LF-TruckCheck_Title", LF_TruckCheckLend.AnswerKey(outcome), 6.0, type, LF_TruckCheckLend.AnswerArg(outcome, minutesLeft));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = LF_Terms.Text("#LF-TruckCheck_Action");
		return true;
	}
}
