//------------------------------------------------------------------------------------------------
//! Vault robbery action. Offer checks read the gates and do not arm the charge.
//! Perform calls LF_Bank.Rob, which calls LF_Breach.Arm once the gates pass.
//------------------------------------------------------------------------------------------------
class LF_BankRobAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// Hidden while the Robbery system is off (#1073).
		return Vault() != null && LF_Systems.IsEnabled(LF_RobAction.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_BankVaultComponent vault = Vault();
		if (!vault)
		{
			SetCannotPerformReason("#EL-Bank_NotReady");
			return false;
		}

		string reason = LF_Bank.GateReason(vault.GetVaultId(), LF_Utils.GetPlayerUid(user), user);
		if (!reason.IsEmpty())
		{
			SetCannotPerformReason(LF_Terms.Text(reason));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		// A prompt shown just before Robbery was turned off (#1073).
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "vault robbery"))
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Systems_Off", "#EL-Robbery_Failed", 3.0);
			return;
		}

		LF_BankVaultComponent vault = LF_Component<LF_BankVaultComponent>.Find(pOwnerEntity);
		if (!vault)
		{
			LF_Debug.Error("Bank", "rob action: owner has no LF_BankVaultComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Bank", "rob action refused: player has no account id");
			return;
		}

		LF_Bank.Rob(vault.GetVaultId(), playerUid, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Bank_Rob";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BankVaultComponent Vault()
	{
		if (!GetOwner())
			return null;

		return LF_Component<LF_BankVaultComponent>.Find(GetOwner());
	}
}
