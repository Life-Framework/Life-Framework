//------------------------------------------------------------------------------------------------
//! The licence desk's one action (#1766): apply for the firearm permit, ask after it, or collect it.
//! The server decides everything (LF_LicenceDesk.Visit) and answers with a notification.
class LF_LicenceDeskAction : ScriptedUserAction
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

		if (LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "LF_LicenceDeskAction"))
			return;

		int minutesLeft;
		LF_ELicenceDesk outcome = LF_LicenceDesk.Visit(pUserEntity, minutesLeft);

		string arg;
		if (outcome == LF_ELicenceDesk.APPLIED || outcome == LF_ELicenceDesk.WAITING)
			arg = ((int)Math.Ceil(minutesLeft / 60.0)).ToString();
		else if (outcome == LF_ELicenceDesk.NOT_ENOUGH_CASH)
			arg = LF_MoneyFormat.FormatMoney(LF_LicencesConfig.PermitFee());

		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (outcome != LF_ELicenceDesk.ISSUED && outcome != LF_ELicenceDesk.APPLIED && outcome != LF_ELicenceDesk.WAITING && outcome != LF_ELicenceDesk.ALREADY_HELD)
			type = LF_ENotificationType.WARNING;

		if (outcome == LF_ELicenceDesk.ISSUED)
			LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(pUserEntity), "#LF-LicenceDesk_Title", LF_LicenceDesk.AnswerKey(outcome), 6.0, type, arg);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-LicenceDesk_Action";
		return true;
	}
}
