//------------------------------------------------------------------------------------------------
//! Vault robbery action. Offer checks read the gates and do not arm the charge.
//! Perform calls LF_Bank.Rob, which calls LF_Breach.Arm once the gates pass.
//------------------------------------------------------------------------------------------------
class LF_BankRobAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return Vault() != null;
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

		string reason = LF_Bank.GateReason(vault.GetVaultId(), EL_Utils.GetPlayerUid(user));
		if (!reason.IsEmpty())
		{
			SetCannotPerformReason(reason);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_BankVaultComponent vault = EL_Component<LF_BankVaultComponent>.Find(pOwnerEntity);
		if (!vault)
		{
			EL_Debug.Error("Bank", "rob action: owner has no LF_BankVaultComponent");
			return;
		}

		string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Bank", "rob action refused: player has no account id");
			return;
		}

		LF_Bank.Rob(vault.GetVaultId(), playerUid);
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

		return EL_Component<LF_BankVaultComponent>.Find(GetOwner());
	}
}
