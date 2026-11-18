//------------------------------------------------------------------------------------------------
//! The licence desk's one action (#1766): apply for the firearm permit, ask after it, or collect it.
//! The server decides everything (LF_LicenceDesk.Visit) and answers with a notification. When an
//! outlet sells the permit over its counter (a catalogue licence row, the pack's gun store), the desk
//! leaves the permit to that counter and this action is hidden and refused (LF_LicenceDesk.DeskIssuesPermit).
class LF_LicenceDeskAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM) && LF_LicenceDesk.DeskIssuesPermit();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "LF_LicenceDeskAction"))
			return;

		if (!LF_LicenceDesk.DeskIssuesPermit())
		{
			LF_Debug.Warn(LF_LicenceDesk.FEATURE, "licence desk refused: the firearm permit is sold at a shop counter (catalogue licence row)");
			return;
		}

		int minutesLeft;
		LF_ELicenceDesk outcome = LF_LicenceDesk.Visit(pUserEntity, minutesLeft);
		string arg = LF_LicenceDesk.AnswerArg(outcome, minutesLeft);

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

//------------------------------------------------------------------------------------------------
//! The licence desk's second action (#3128): buy the driving licence at once for the fee in carried
//! cash (LF_LicenceDesk.BuyDriving). Shown whenever the License system is on, also where the permit
//! is sold at a shop counter; hidden from a player whose body already holds the licence.
class LF_LicenceDeskDrivingAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM))
			return false;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(user);
		return !licences || !licences.HasLicense(LF_LicenceDesk.DRIVING);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "LF_LicenceDeskDrivingAction"))
			return;

		LF_ELicenceDesk outcome = LF_LicenceDesk.BuyDriving(pUserEntity);
		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (outcome != LF_ELicenceDesk.ISSUED && outcome != LF_ELicenceDesk.ALREADY_HELD)
			type = LF_ENotificationType.WARNING;

		if (outcome == LF_ELicenceDesk.ISSUED)
			LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(pUserEntity), "#LF-LicenceDesk_Title", LF_LicenceDesk.DrivingAnswerKey(outcome), 6.0, type, LF_LicenceDesk.DrivingAnswerArg(outcome));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-LicenceDesk_DrivingAction";
		return true;
	}
}
